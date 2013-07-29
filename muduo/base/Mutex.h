// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include <muduo/base/CurrentThread.h>
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{

class MutexLock : boost::noncopyable          // ��Mutex�ķ�װ
{
 public:
  MutexLock()
    : holder_(0)
  {
    int ret = pthread_mutex_init(&mutex_, NULL); // init
    assert(ret == 0); (void) ret;
  }

  ~MutexLock()
  {
    assert(holder_ == 0);                     // ʹ��assert��߳���Ľ�׳��
    int ret = pthread_mutex_destroy(&mutex_); // destory
    assert(ret == 0); (void) ret;
  }

  bool isLockedByThisThread()          		  // �Ƿ���ס���ǵ�ǰ����
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked()
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  void lock()
  {
    pthread_mutex_lock(&mutex_);              // lock
    holder_ = CurrentThread::tid();           // id�ǵ�ǰ�̵߳�tid
  }

  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);            // unlock set holder_(0)
  }

  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;  						  // ���ص��ǵ�ַ
  }

 private:

  pthread_mutex_t mutex_;
  pid_t holder_;
};

// ��MutexLock���з�װ ������������������Է�ֹ���ǽ���

class MutexLockGuard : boost::noncopyable 
{
 public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:

  MutexLock& mutex_; // ��Ĺ�ϵ�Ĺ��� �������ͷ�mutex_
};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long! һ����ʱ������hold���ܳ�ʱ��
#define MutexLockGuard(x) error "Missing guard object name" // ���⹹��һ�������Ķ���

#endif  // MUDUO_BASE_MUTEX_H
