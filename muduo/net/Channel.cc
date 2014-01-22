// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

// ��ʼ��
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI; // IN or POLLPRI �������� ��������
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false)
{
}

Channel::~Channel()
{
  assert(!eventHandling_);
}

void Channel::tie(const boost::shared_ptr<void>& obj) 
{
  tie_ = obj;    // ������������ ���Ὣshared_ptr�����ü�����1
  tied_ = true;
}

void Channel::update()
{
  loop_->updateChannel(this); // ���� Poller �� updateChannel
}

void Channel::remove()
{
  assert(isNoneEvent());
  loop_->removeChannel(this);// ���� Poller �� updateChannel ��PollPoller����ʵ��
}

void Channel::handleEvent(Timestamp receiveTime)   // �ɶ�д�¼�������
{
  boost::shared_ptr<void> guard;   
  if (tied_)
  {
    guard = tie_.lock(); // �����Ƕ���ָ���һ������
    if (guard)
    {
      handleEventWithGuard(receiveTime); // ������ǰע��Ļص����������д�¼�
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
  eventHandling_ = true;
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) // �жϷ��ص��¼� Ϊ�Ҷ� close
  {
    if (logHup_)
    {
      LOG_WARN << "Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();           // ���ö�Ӧ���¼�������
  }

  if (revents_ & POLLNVAL) 						    // �ļ�������fdû�򿪻��߷Ƿ�
  {
    LOG_WARN << "Channel::handle_event() POLLNVAL";
  }

  if (revents_ & (POLLERR | POLLNVAL))             // �����
  {
    if (errorCallback_) errorCallback_();
  }
  // POLLRDHUP �ر����ӻ��߹رհ�����
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))   // �Եȷŵ���close�ر����� ���ܵ�POLLRDHUP POLLPRI(��������)
  {
    if (readCallback_) readCallback_(receiveTime); // �����ɶ��¼� ���ö�����
  }
  if (revents_ & POLLOUT)
  {
    if (writeCallback_) writeCallback_();  // ��д�¼��Ĳ��� ����д�Ļص�����
  }
  eventHandling_ = false;
}

string Channel::reventsToString() const
{
  std::ostringstream oss;
  oss << fd_ << ": ";
  if (revents_ & POLLIN)
    oss << "IN ";
  if (revents_ & POLLPRI)
    oss << "PRI ";
  if (revents_ & POLLOUT)
    oss << "OUT ";
  if (revents_ & POLLHUP)
    oss << "HUP ";
  if (revents_ & POLLRDHUP)
    oss << "RDHUP ";
  if (revents_ & POLLERR)
    oss << "ERR ";
  if (revents_ & POLLNVAL)
    oss << "NVAL ";

  return oss.str().c_str();
}
