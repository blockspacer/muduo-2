// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#define __STDC_LIMIT_MACROS
#include <muduo/net/TimerQueue.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Timer.h>
#include <muduo/net/TimerId.h>

#include <boost/bind.hpp>

#include <sys/timerfd.h>

namespace muduo
{
namespace net
{
namespace detail
{

// ����timerfd_create������ʱ��
int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when) // ����ת������
{
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch(); // ʱ���
  if (microseconds < 100) // ��ʱʱ����С100ms
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

// ����read������� �����һֱ���� LT 
void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany); 
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration); // ʱ��ת������ ���㵽ʱ��ʱ���
 
  // ����timerfd_settime������ʱ��
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback( // ��עͨ���еĿɶ��¼�
      boost::bind(&TimerQueue::handleRead, this));
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin();
      it != timers_.end(); ++it)
  {
    delete it->second; // ɾ����ʱ�� ��ʹ��������ָ��Ͳ������ֶ�ɾ����
  }
}

// ����һ����ʱ�� �̰߳�ȫ���첽���� �����߳̿��Ե���
TimerId TimerQueue::addTimer(const TimerCallback& cb, // �ص�����
                             Timestamp when,          // ��ʱ�¼�
                             double interval)         // ��� 
{
  Timer* timer = new Timer(cb, when, interval);       // 1.����һ����ʱ��
  loop_->runInLoop(         // 2.�����񽻸�loop_��Ӧ��IO�߳������� �첽�߳�
      boost::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      boost::bind(&TimerQueue::cancelInLoop, this, timerId));
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread(); // ����

  // ����һ����ʱ�� �п��ܻ�ʹ�����絽�ڵĶ�ʱ�������ı� �����ܸ���
  bool earliestChanged = insert(timer);

  if (earliestChanged) 
  {
  // �������ĵ�ʱʱ��ı����ö�ʱ���ĳ�ʱʱ��
    resetTimerfd(timerfd_, timer->expiration()); 
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  ActiveTimer timer(timerId.timer_, timerId.sequence_);
  ActiveTimerSet::iterator it = activeTimers_.find(timer);
  if (it != activeTimers_.end()) // �ҵ�Ҫȡ���Ķ�ʱ��
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1); (void)n;
    delete it->first; // FIXME: no delete please ���ʹ��unique_ptr����Ҫdelete
    activeTimers_.erase(it);
  }
  else if (callingExpiredTimers_) // �����б��� �Ѿ������� �������ڵ��ûص������Ķ�ʱ��
  {
    cancelingTimers_.insert(timer);
  }
  // 
  assert(timers_.size() == activeTimers_.size());
}

// ��עͨ���Ŀɶ��¼� ��ĳһ��ʱ�̶�ʱ�������˳�ʱ
void TimerQueue::handleRead()
{
  loop_->assertInLoopThread(); // ����I/O�߳���
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);  // ʹ����LT ������Ҫ���� ����һֱ����

// ��ȡĳһ��ʱ�̵ĳ�ʱ�б� �п���ĳһʱ�̶����ʱ����ʱ
  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    it->second->run();         // ������ö�ʱ����ʱ �ص���������
  }
  callingExpiredTimers_ = false;

  reset(expired, now);         // ����һ���� ��ʱ�� ��Ҫ����
}

// rvo realease�汾�Ż� ���Բ���Ҫ�������û�ָ��
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

  // ���ص�һ��δ���ڵ�Timer�ĵ�����
  // ��*end >= sentry �Ӷ� end->firt > now
  // ����Entry��pair���� ���Ի��бȽϺ����ֵ ��sentry�����ֵUINTPTR_MAX�ڱ�ֵ
  // ��֤�˵õ��ıȵ�ǰ�Ĵ�
  // lower_bound �Ǵ��ڵ��� upper_bound�Ǵ���
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);

  // �����ڵĶ�ʱ�����뵽expired�� STL�е����䶼�Ǳտ�����
  // ��timers_���Ƴ����ڵĶ�ʱ��
  // copy��������������ʱ�Ķ�ʱ�����浽expired vector��
  std::copy(timers_.begin(), end, back_inserter(expired));
  timers_.erase(timers_.begin(), end);

  // ��activeTimers_���Ƴ����ڵĶ�ʱ��
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

// ����timefd��Ӧ��ʱ��timer
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());

	// ������ظ��Ķ�ʱ��������δȡ���Ķ�ʱ��(���ڻ�Ķ������ҵ�) ��������ʱ��
    if (it->second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      it->second->restart(now); // ���¼�����һ����ʱʱ��
      insert(it->second);       // �������뵽��ʱ��������
    }
    else
    {
      // FIXME move to a free list һ���Զ�ʱ���������� ɾ���ö�ʱ��
      delete it->second;       // FIXME: no delete please
    }
  }
  
  if (!timers_.empty())    // �õ���һ�ζ�ʱ����ʱ��
  {
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())  // ����´ζ�ʱ����ʱ���ǺϷ��� ����timefd��Ӧ�Ĵ���ʱ��
  {
    resetTimerfd(timerfd_, nextExpire);
  }
}

bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread(); 		    // ֻ����IO�߳���ʹ��
  assert(timers_.size() == activeTimers_.size());
  
  bool earliestChanged = false;		    // ���絽��ʱ���Ƿ�ı�
  Timestamp when = timer->expiration(); // when������timer��expiration��
  TimerList::iterator it = timers_.begin(); 
  // ʹ��set��ʵ�� Ĭ�ϴ�С��������ʱ��� map�����õ��ǿ�����multimap

  // �ն��л��߸���ĵ���ʱ�� ���絽��ʱ�䷢���ı�
  if (it == timers_.end() || when < it->first) 
  {
    earliestChanged = true;
  }
  {// ���뵽timer_��
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }
  {// ���뵽activeTimer_��
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

