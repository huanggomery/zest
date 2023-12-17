// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/tcp_client.h"

#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "zest/base/logging.h"
#include "zest/net/eventloop.h"
#include "zest/net/fd_event.h"
#include "zest/net/timer_event.h"

using namespace zest;
using namespace zest::net;

TcpClient::TcpClient(NetBaseAddress &peer_addr)
  : m_peer_address(peer_addr.copy()), m_eventloop(EventLoop::CreateEventLoop())
{
  int clientfd = socket(peer_addr.family(), SOCK_STREAM, 0);
  if (clientfd == -1) {
    std::cerr << "create socket failed, errno = " << errno << std::endl;
    exit(-1);
  }
  m_connection.reset(new TcpConnection(clientfd, m_eventloop, m_peer_address));
  m_connection->setState(NotConnected);
}

void TcpClient::start()
{
  if (m_running == true)
    return;
  m_running = true;
  
  this->connect();  
  m_eventloop->loop();
  m_running = false;
}

void TcpClient::stop()
{
  LOG_DEBUG << "TcpClient::stop()";
  if (m_eventloop->isThisThread()) {
    if (m_running) {
      // m_connection->close();
      m_eventloop->stop();
      m_running = false;
    }
  }
  else {
    m_eventloop->runInLoop(std::bind(&TcpClient::stop, this));
  }
}

void TcpClient::addTimer(uint64_t interval, std::function<void()> cb, bool periodic /*=false*/)
{
  if (m_eventloop->isThisThread()) {
    TimerEvent::s_ptr timer = 
      std::make_shared<TimerEvent>(interval, cb, periodic);
    m_eventloop->addTimerEvent(timer);
  }
  else {
    m_eventloop->runInLoop(
      [interval, cb, periodic, this](){
        this->addTimer(interval, cb, periodic);
      }
    );
  }
}

void TcpClient::connect()
{
  FdEvent::s_ptr try_again = std::make_shared<FdEvent>(m_connection->socketfd());
  int rt = ::connect(m_connection->socketfd(), 
                     m_peer_address->sockaddr(), 
                     m_peer_address->socklen());
                     
  if (rt == -1) {
    // clientfd 是非阻塞的，所以可能会 EINPROGRESS
    if (errno == EINPROGRESS) {
      try_again->listen(EPOLLOUT, std::bind(&TcpClient::connect, this));
      m_eventloop->addEpollEvent(try_again);

      auto eventloop = m_eventloop;
      auto conn = m_connection;
      // 如果3秒内无法连接服务器，则放弃尝试并退出
      m_connection->addTimer(
        "connect_timeout",
        3000,
        [eventloop, try_again](TcpConnection &conn) {
          ::close(conn.socketfd());
          conn.setState(Failed);
          eventloop->deleteEpollEvent(try_again);
          eventloop->stop();
          LOG_ERROR << "Timeout, can't connect with server: " << conn.peerAddress().to_string();
        }
      );
    }
    else if (errno == EISCONN) {
      m_connection->setState(Connected);
    }
    else {
      m_connection->setState(Failed);
      LOG_ERROR << "Connect to " << m_connection->peerAddress().to_string() << " failed, errno = " << errno;
      ::close(m_connection->socketfd());
    }
  }
  else {
    // ::connect 返回0，连接成功
    m_connection->setState(Connected);
  }

  if (m_connection->getState() == Connected || m_connection->getState() == Failed) {
    m_eventloop->deleteEpollEvent(try_again);
    m_connection->cancelTimer("connect_timeout");
  }
  if (m_connection->getState() == Connected) {
    if (m_on_connection_callback)
      m_on_connection_callback(*m_connection);
  }
}
