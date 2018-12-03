#include <assert.h>
#include <functional>
#include <thread>
#include "Logger.hh"
#include "EventLoop.hh"
#include "SocketHelp.hh"
#include "TcpClient.hh"
#include "TcpConnection.hh"

namespace sola
{
  void removeConnection(EventLoop* p_loop, const TcpConnectionPtr& conn)
  {
    LOG_TRACE << "sola::removeConnection() removeConnection";
    p_loop->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
  }

  void removeConnector(const std::shared_ptr<Connector>& p_connector)
  {
    LOG_TRACE << "sola::removeConnector() Connector use_count " << p_connector.use_count();
    assert(p_connector.unique());
  }
};

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr)
  :p_loop(loop),
  m_isConnectorStarted(false),
  m_enRetry(false),
  p_connector(new Connector(loop, serverAddr))
{
  LOG_TRACE << "ctor[" << this << "]";
  p_connector->setNewConnectionCallBack(std::bind(&TcpClient::newConnetion, this, std::placeholders::_1));
}


TcpClient::~TcpClient()
{
  LOG_TRACE << "dtor[" << this << "]" << " TcpConnection use count " << p_connection.use_count();

  TcpConnectionPtr conn;
  bool unique = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    unique = p_connection.unique();
    conn = p_connection;
  }

  if(conn)
  {
    //not safe.
    NetCallBacks::CloseCallBack cb = std::bind(&sola::removeConnection, p_loop, std::placeholders::_1);

    p_loop->runInLoop(std::bind(&TcpConnection::setCloseCallBack, conn, cb));

    if(unique)
    {
      conn->forceClose();
    }
  }
  else
  {
      if(m_isConnectorStarted)
      {
        p_connector->stop();

        p_loop->runAfter(1, std::bind(&sola::removeConnector, p_connector));
      }
  }

  LOG_TRACE << "dtor[" << this << "]" << " End";

}

void TcpClient::start()
{
  assert(!m_isConnectorStarted);
  connect();
}

void TcpClient::connect()
{
  m_isConnectorStarted = true;
  p_connector->start();
}

void TcpClient::disconnect()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if(p_connection)
    {
      p_connection->shutdown();
    }
  }
}

void TcpClient::stop()
{
  m_isConnectorStarted = false;
  p_connector->stop();
}


void TcpClient::newConnetion(int sockfd)
{
  LOG_TRACE << "TcpClient::newConnetion()";
  p_loop->assertInLoopThread();

  InetAddress localAddr(sockets::getLocalAddr(sockfd));

  InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  char buf[64];
  snprintf(buf, sizeof buf, ":%s", peerAddr.toIpPort().c_str());
  std::string connName = buf;

  TcpConnectionPtr conn(new TcpConnection(p_loop, connName, sockfd, localAddr, peerAddr));
  conn->setConnectionCallBack(m_connectionCallBack);
  conn->setMessageCallBack(m_messageCallBack);
  //conn->setWriteCompleteCallBack();
  conn->setCloseCallBack(std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    p_connection = conn;
  }

  conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
  LOG_TRACE << "TcpClient::removeConnection()";
  p_loop->assertInLoopThread();
  assert(p_loop == conn->getLoop());

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    assert(p_connection  == conn);
    p_connection.reset();
  }

  p_loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

  if (m_enRetry)
  {
    LOG_INFO << "TcpClient::connect[" /*<< m_name*/ << "] - Reconnecting to "
             ;//<< p_connector->serverAddress().toIpPort();
    p_connector->restart();
  }

}