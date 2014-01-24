#include <muduo/net/Acceptor.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie()),
    acceptChannel_(loop, acceptSocket_.fd()),
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);     // ���õ�ַ�ظ�����
  acceptSocket_.bindAddress(listenAddr);
  acceptChannel_.setReadCallback(       // ����ͨ���ж��¼���ʱ��Ļص�����
      boost::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listenning_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
  loop_->assertInLoopThread();
  InetAddress peerAddr(0);
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr); // ����socket��accept���ȴ��ͻ��˵�����
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      newConnectionCallback_(connfd, peerAddr); // ���ӵ������������ӵĻص�����������
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    // By Marc Lehmann, author of livev. ���ŵĹرմ����fd  ��׼��һ�����е�fd������
    if (errno == EMFILE) // EMFILE too many fd ����̫�������
    {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC); // �����ǵ�ƽ��������׼��һ�����е��ļ�������
    }
  }
}

