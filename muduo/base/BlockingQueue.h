// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <deque>
#include <assert.h>

namespace muduo
{

template<typename T>
class BlockingQueue : boost::noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_); // �Զ��н��б���
    queue_.push_back(x);
    notEmpty_.notify(); // TODO: move outside of lock ֪ͨ�ȴ����߳� ʵ���߳�ͬ��
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait(); // Ϊ������
    }
    assert(!queue_.empty());
    T front(queue_.front());
    queue_.pop_front();
    return front;
  }

  size_t size() const /* �����ж���̷߳���������Ҫ���� */
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
  mutable MutexLock mutex_;    // Mutex mutable ��Ϊ��Щ��Ա������Ҫ�ͷ��� �ı�������̬
  Condition         notEmpty_;  // ��������
  std::deque<T>     queue_;     // ʹ���˱�׼���deque<T>˫�˶���
};

}

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
