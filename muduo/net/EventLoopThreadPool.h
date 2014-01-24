#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <vector>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback& cb = ThreadInitCallback());
  EventLoop* getNextLoop();

 private:

  EventLoop* baseLoop_; // ���߳�
  bool started_;
  int numThreads_; // �߳���
  int next_;       // �������ӵ��� ��ѡ���EventLoop�����±�
  boost::ptr_vector<EventLoopThread> threads_; // EventLoopThread IO�߳��б�  ע������ʹ����boost��ptr_vector����
  std::vector<EventLoop*> loops_;              // EventLoop�б� ջ�϶��� ����Ҫ�ֶ�����
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
