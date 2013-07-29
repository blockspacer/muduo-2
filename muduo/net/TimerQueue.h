// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Channel.h>

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  // һ�����̰߳�ȫ�� ���Կ��̵߳��� ͨ������±������̵߳���
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);

  void cancel(TimerId timerId);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  // unique_ptr��C++ 11��׼��һ����������Ȩ������ָ�� ��������ָ��
  // �޷��õ�ָ��ͬһ���������unique_ptrָ��
  // �����Խ����ƶ��������ƶ���ֵ���� ������Ȩ�����ƶ�����һ������(���ǿ�������)
  typedef std::pair<Timestamp, Timer*> Entry;     // ����Ķ�����ͬ ��ʱ������
  typedef std::set<Entry> TimerList;
  
  typedef std::pair<Timer*, int64_t> ActiveTimer; // ����ַ����
  typedef std::set<ActiveTimer> ActiveTimerSet;

// һ�³�Ա����ֻ��������������IO�̵߳��� ���Բ���Ҫ����
// ����������ɱ��֮һ�������� ���Ծ����ܽ���������
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers

  // ���س�ʱ�Ķ�ʱ���б�
  std::vector<Entry> getExpired(Timestamp now);
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);

  EventLoop* loop_;                // ����EventLooop
  const int timerfd_;
  Channel timerfdChannel_;
  // Timer list sorted by expiration ��ʱ��˳��
  TimerList timers_;

  // for cancel()
  ActiveTimerSet activeTimers_; // timer_��activeTimers_���������ͬ������ һ������ʱ������ һ�����ն����ַ����
  bool callingExpiredTimers_; /* atomic */
  ActiveTimerSet cancelingTimers_; // ������Ǳ�ȡ���Ķ�ʱ��
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
