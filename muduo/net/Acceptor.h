
#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <muduo/net/Channel.h>
#include <muduo/net/Socket.h>

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
class Acceptor : boost::noncopyable
{
 public:
  typedef boost::function<void (int sockfd,
                                const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();

  EventLoop* loop_;       // ��Ӧ���¼�ѭ��
  Socket acceptSocket_;
  Channel acceptChannel_; // ͨ���۲�socketfd�Ŀɶ��¼�
  NewConnectionCallback newConnectionCallback_; // �����ӵ����Ļص�����
  bool listenning_;      // �ͷż�����
  int idleFd_;
};

}
}

#endif  // MUDUO_NET_ACCEPTOR_H
