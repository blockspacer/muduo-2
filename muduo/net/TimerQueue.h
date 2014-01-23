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

class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  // һ�����̰߳�ȫ�� ���Կ��̵߳��� ͨ������±������̵߳��� �򵥵�2���ӿ�
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);

  void cancel(TimerId timerId);

 private:

  // unique_ptr��C++ 11��׼��һ����������Ȩ������ָ�� ��������ָ���޷��õ�ָ��ͬһ���������unique_ptrָ��
   // �����Խ����ƶ��������ƶ���ֵ���� ������Ȩ�����ƶ�����һ������(���ǿ�������) ����Ķ�����ͬ ��ʱ������

  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerList;              // ����ʱʱ�������
  
  typedef std::pair<Timer*, int64_t> ActiveTimer; // ����ַ���� ��ַ�����
  typedef std::set<ActiveTimer> ActiveTimerSet;   // ������ͬ�Ķ��� ����ַ����

// ����ĳ�Ա����ֻ��������������IO�̵߳��� ���Բ���Ҫ���� ����������ɱ��֮һ�������� ���Ծ����ܽ���������
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId); // called when timerfd alarms
  void handleRead();// move out all expired timers

  std::vector<Entry> getExpired(Timestamp now); // ���س�ʱ�Ķ�ʱ���б�
  void reset(const std::vector<Entry>& expired, Timestamp now); // ���趨ʱ��

  bool insert(Timer* timer);

  EventLoop* loop_;         // ����EventLooop
  const int timerfd_;      // ����Ķ�ʱ���ǰ���fd�������
  Channel timerfdChannel_;  // ��ʱ����д�¼������Ĵ���ͨ��
  TimerList timers_;  // Timer list sorted by expiration ����ʱ��ʱ��˳��

  // for cancel()
  ActiveTimerSet activeTimers_;    // timer_��activeTimers_���������ͬ������ һ������ʱ������ һ�����ն����ַ����
  bool callingExpiredTimers_;
  ActiveTimerSet cancelingTimers_; // ������Ǳ�ȡ���Ķ�ʱ��
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
