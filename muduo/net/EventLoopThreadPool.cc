#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
  : baseLoop_(baseLoop),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;

  for (int i = 0; i < numThreads_; ++i)
  {
    EventLoopThread* t = new EventLoopThread(cb);
    threads_.push_back(t); // ѹ�봴����IO�߳�
    loops_.push_back(t->startLoop()); // ����EventLoopThread�߳� �ڽ����¼�ѭ��֮ǰ �����cb
  }
  if (numThreads_ == 0 && cb)
  {
	  //ֻ��һ���߳� EventLoop �����EventLoop�����¼�ѭ��֮ǰ ����cb
    cb(baseLoop_);
  }
}

// ���µ����ӵ���ʱ ʹ��һ��EventLoop������
EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_; // ���߳�

// baseLoop_����mainReactor���Ϊ�� ֱ�ӷ����� ���������´���
  if (!loops_.empty())
  {
    // round-robin
    loop = loops_[next_]; // �����ֻһ���߳� ��ȡ��һ���¼�ѭ��EventLoop
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }

  return loop; // ���ֻ��һ���߳����ﷵ�صľ������߳�
}

