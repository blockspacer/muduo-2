#include <muduo/net/EventLoopThread.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this)),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  loop_->quit(); // �˳� IO �߳� ��IO�̵߳�loopѭ���˳� �Ӷ��˳���IO�߳�
  thread_.join();// �ȴ��߳��˳�
}

EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL) // loopΪ��һֱ�ȴ� ֱ����IO�¼�����
    {
      cond_.wait(); // �ȴ�notify
    }
  }
  return loop_;
}

void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }
  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop; 

    // ����ָ��ָ����һ��ջ�϶��� û�����ٵ�ԭ���ǣ��߳�ִ�е�ʱ���������߳�ִ�к���
    // ���߳�ִ�н�����ʱ�� �̶߳��������� ϵͳ���Զ��ͷ���Դ

    cond_.notify(); // ����notify �¼�ѭ���к����� loop_��Ϊ����
  }

  loop.loop();
  //assert(exiting_);
}

