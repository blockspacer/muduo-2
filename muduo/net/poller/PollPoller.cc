// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/PollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Channel.h>

#include <assert.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop)
{
}

PollPoller::~PollPoller()
{
}

Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  // XXX pollfds_ shouldn't change
  // ����ϵͳ��poll ����������׵�ַ ��ע��Щfd�Ŀɶ���д�¼�
  // ���ﴫ����ǵ�ַ �����Ǽ�ӵ�ַָ��
  int numEvents = ::poll(&*pollfds_.begin(), polclfds_.size(), timeoutMs); 
  Timestamp now(Timestamp::now());
  if (numEvents > 0) // �������0����Щ�¼����ص�IOͨ����
  {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels); // ������¼��������¼����뵽ͨ����
  }
  else if (numEvents == 0) // timeout
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    LOG_SYSERR << "PollPoller::poll()";
  }
  return now;
}

void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
// ����ͨ���е�fds
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
    if (pfd->revents > 0) // �������¼� �����¼� 
    {
      --numEvents;        // �����¼�
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);// ͨ��fd����ͨ��
      assert(ch != channels_.end());
      Channel* channel = ch->second;      // �õ�ͨ��
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);
      // pfd->revents = 0;
      activeChannels->push_back(channel); // ��ͨ��ѹ�뵽�ͨ����
    }
  }
}

// ע��ĳ��fd�Ŀɶ���д�¼����߸���
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread(); // EventLoop��Ӧthread
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  
  if (channel->index() < 0) // �������е�λ��δ֪ �µ�ͨ��
  {
    // a new one, add to pollfds_ 
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size())-1; // vector����
    channel->set_index(idx);     // ����λ��
    channels_[pfd.fd] = channel; // ����fd��Ӧ��channel_
  }
  else
  {
    // update existing one ����ͨ��
    assert(channels_.find(channel->fd()) != channels_.end()); // �Ѿ��е�ͨ�������ҵ�
    assert(channels_[channel->fd()] == channel);
	
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
	
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
	
    pfd.events = static_cast<short>(channel->events()); // �����¼�
    pfd.revents = 0;

	// ��һ��ͨ����ʱ����Ϊ����ע�¼� ���ǲ���Poller���Ƴ���ͨ��
    if (channel->isNoneEvent())
    {
      // ignore this pollfd
      // ��ʱ���Ը��ļ����������¼� 
      // ����pfd.fd ����ֱ������Ϊ-1 ����������Ϊ��removeChannel�Ż�
      pfd.fd = -channel->fd()-1; // -1Ϊ�˴���0 ����2�ε�����������Եõ�ԭ����ֵ
    }
  }
}

void PollPoller::removeChannel(Channel* channel) // �Ƴ�fd��Ӧ��IOͨ��
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());  // ����û���¼�
  
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());
  size_t n = channels_.erase(channel->fd()); // ��key��ͬͨ��map���Ƴ� 
  assert(n == 1); (void)n;
  
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1) // ���һ��
  {
    pollfds_.pop_back();
  }
  else // �������һ��
  {
  // �����Ƴ����㷨���Ӷ�ΪO(1) ����û��˳���ϵ���� �����Ƴ���Ԫ�������һ��Ԫ�ؽ�����pop_back�ͺ�
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1); // swap: iter_swap
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
    channels_[channelAtEnd]->set_index(idx); // �������һ��Ԫ�ص�index
    pollfds_.pop_back();
  }
}

