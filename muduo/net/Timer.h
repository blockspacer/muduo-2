// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMER_H
#define MUDUO_NET_TIMER_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>

namespace muduo
{
namespace net
{
///
/// Internal class for timer event.
///
class Timer : boost::noncopyable // �Զ�ʱ����һ���߶ȳ���
{
 public:
  Timer(const TimerCallback& cb, Timestamp when, double interval)
    : callback_(cb),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet()) // ����sequence�ļ���
  { }

  void run() const
  {
    callback_();
  }

  Timestamp expiration() const  { return expiration_; }
  bool repeat() const { return repeat_; }
  int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

  static int64_t numCreated() { return s_numCreated_.get(); }

 private:
  const TimerCallback callback_; // ��ʱ���ص�����
  Timestamp expiration_;         // ���� ��һ�εĳ�ʱʱ��
  const double interval_;        // ��ʱ��� �����һ���Զ�ʱ��Ϊ0
  const bool repeat_;		     // �Ƿ��ظ�
  const int64_t sequence_;       // ��ʱ�����

  static AtomicInt64 s_numCreated_; // ��ʱ������ ��ǰ�Ѿ������Ķ�ʱ������
};
}
}
#endif  // MUDUO_NET_TIMER_H
