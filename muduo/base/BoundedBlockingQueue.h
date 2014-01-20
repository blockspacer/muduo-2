#ifndef MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
#define MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/circular_buffer.hpp>
#include <boost/noncopyable.hpp>
#include <assert.h>

namespace muduo
{

template<typename T>
class BoundedBlockingQueue : boost::noncopyable
{
 public:
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      queue_(maxSize)
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.full())
    {
      notFull_.wait(); // �������˲���д�� �����ȴ�
    }
    assert(!queue_.full());
    queue_.push_back(x);
    notEmpty_.notify(); //  д�����ݲ�Ϊ����
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    while (queue_.empty()) // ����Ϊ�ղ���ȡ�� �����ȴ�
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    T front(queue_.front());
    queue_.pop_front();
    notFull_.notify(); // ȡ������ ֪ͨ���в�������д����
    return front;
  }

  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }

  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }
// ���ʾ���������-������ģ��
 private:
  mutable MutexLock          mutex_;
  Condition                  notEmpty_; // �ж��Ƿ�ǿ� �ǿտɶ�
  Condition                  notFull_;  // �ж��Ƿ��� ������д
  boost::circular_buffer<T>  queue_;    // ʹ����boost��Ļ��λ�����
};

}

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
