// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <boost/noncopyable.hpp>

namespace muduo
{
namespace net
{

class EventLoop;

// 1.����һ���߳�
// 2.���̺߳����д���һ��EventLoop���� ���ҵ���EventLoop::loop
class EventLoopThread : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback());
  ~EventLoopThread();
  EventLoop* startLoop();        // �����߳� ���̳߳�ΪIO�߳�

 private:
  void threadFunc(); 	         // �̺߳���

  EventLoop* loop_; 	         // ָ��EventLoop����
  bool exiting_;
  Thread thread_;
  MutexLock mutex_;
  Condition cond_;
  ThreadInitCallback callback_;  // �ص�������EventLoop::loop�¼�ѭ��֮ǰ������ �൱�ڳ�ʼ��
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H
