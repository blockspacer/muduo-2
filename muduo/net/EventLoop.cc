// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Singleton.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
__thread EventLoop* t_loopInThisThread = 0; // ��ǰָ����� ��__thread��ʾ�̴߳洢

const int kPollTimeMs = 10000; // 10s

// eventfd�����̼߳� ͨ�� ����pipe socketpair
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)), // ���ﴴ����һ��eventfdͨ��
    currentActiveChannel_(NULL)
{
  LOG_TRACE << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread) // �����ǰ�߳��Ѿ�������EventLoop���� LOG_FATAL
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(
      boost::bind(&EventLoop::handleRead, this)); // ��Ҫ���߷����һֱ����
  // we are always reading the wakeupfd
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
  ::close(wakeupFd_); // �ر�wakeupFd
  t_loopInThisThread = NULL;
}

// ���ܿ��̵߳��� ֻ���ڴ����ö�����߳��е���
// ����ͨ����ע���IO������ ����ʱ�¼�
void EventLoop::loop()
{
  assert(!looping_);    // �����Ƿ����¼�ѭ����
  assertInLoopThread(); // �����Ƿ��ڴ����ö�����߳���
  looping_ = true;
  quit_ = false;
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); // ��ʱ�¼�
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels(); // ��־�Ĵ���
    }
    // TODO sort channel by priority ͨ������Ȩ����ͨ��
    eventHandling_ = true; // true
    for (ChannelList::iterator it = activeChannels_.begin(); // �����ͨ�����д���
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false; // false
    doPendingFunctors();    // Ϊ����IO�߳�Ҳ��ִ��һЩ��������
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit() // ���Կ��̵߳���
{
  quit_ = true;
  if (!isInLoopThread())
  {
    wakeup();         // ���ѱ���߳�
  }
}

// ��IO�߳���ִ��ĳ���ص����� ִ��ĳ������ �ú������Կ��̵߳��� 
void EventLoop::runInLoop(const Functor& cb) // �ڲ������������ ��֤�̰߳�ȫ
{
  if (isInLoopThread()) // ͬ��IO
  {
    cb();               // ����ǵ�ǰ�߳� ����ͬ��cb
  }
  else                  // �첽��������  
  {
    queueInLoop(cb);// ����������̵߳��� ���첽�ؽ�cb��ӵ�������
  }
}

// ��������ӵ��첽���������
void EventLoop::queueInLoop(const Functor& cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(cb);
  }

// ������ǵ�ǰIO�߳� ����Ҫ���ѵ�ǰ�߳� ���ߵ�ǰ�߳����ڵ���pending functor Ҳ��Ҫ����
// ֻ�е�ǰIO�̵߳��¼��ص��е���queueInLooop�Ų���Ҫ����
  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

// ��ʱ������ һ���Զ�ʱ��
TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb) 
{
  return timerQueue_->addTimer(cb, time, 0.0);
}

// ��ʱ������ 
TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

// ����Զ�ʱ��
TimerId EventLoop::runEvery(double interval, const TimerCallback& cb) // ��ʱ������
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

// ��ʱ������
void EventLoop::cancel(TimerId timerId)  
{
  return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

// ���ѱ���̺߳���
void EventLoop::wakeup()
{
  uint64_t one = 1; // 8���ֽڵĻ����� д�� 1 wakeup
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

// ������Ҫ���ߵ�ԭ������ΪLT �����Ļ� ��һֱ����
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

// ���Ǽ򵥵����ٽ���һ�ε���Functor ���ǰѻص��б�swap��functors�� ����һ����������ٽ����ĳ���
// ��ζ�������������̵߳�queueLoop()
// ��һ����Ҳ���������� ��ΪFunctor�����ٴε���queueInLooop()
// ����doPendingFunctors()���õ�Functor�����ٴε���queueInLoop(cb) ��ʱ queueInLoop()�ͱ���wakeup()
// ����������cb���ܾͲ��ܼ�ʱ������
// muduo û�з���ִ��doPendingFunctors()ֱ��pendingFunctorsΪ�� ���������
// ����IO�߳̿���������ѭ�� �޷�����IO�¼�
void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_); // ��pendFunctors_����ӵ�functors�� ������ִ��
  }

  for (size_t i = 0; i < functors.size(); ++i)
  {
    functors[i]();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}

