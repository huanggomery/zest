// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/tcp_client.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "zest/base/logging.h"
#include "zest/net/eventloop.h"
#include "zest/net/fd_event.h"
#include "zest/net/tcp_buffer.h"
#include "zest/net/timer_container.h"
#include "zest/net/timer_event.h"

using namespace zest;
using namespace zest::net;

TcpClient::TcpClient(NetBaseAddress &peer_addr):
  m_peer_address(peer_addr.copy()), m_eventloop(EventLoop::CreateEventLoop())
{
  int clientfd = socket(peer_addr.family(), SOCK_STREAM, 0);
  if (clientfd == -1) {
    std::cerr << "create socket failed, errno = " << errno << std::endl;
    exit(-1);
  }
  m_connection.reset(new TcpConnection(clientfd, m_eventloop, m_peer_address));
  m_connection->setState(NotConnected);
}

TcpClient::~TcpClient()
{
  if (m_connection->getState() == Connected)
    disconnect();
}

bool TcpClient::connect()
{
  m_eventloop->assertInLoopThread();

  int rt = ::connect(m_connection->socketfd(), 
                     m_peer_address->sockaddr(), 
                     m_peer_address->socklen());
  
  if (rt == 0) {
    m_connection->setState(Connected);
    return true;
  }
  else {
    // clientfd 是非阻塞的，所以可能会 EINPROGRESS
    if (errno == EINPROGRESS) {
      // 添加超时定时器
      m_connection->addTimer(
        "connect_timeout",
        3000,
        [this](TcpConnection &conn){
          if (conn.getState() == NotConnected) {
            conn.setState(Failed);
            LOG_ERROR << "Timeout, can't connect with server: " << conn.peerAddress().to_string();
            this->m_eventloop->stop();
          }
        }
      );
    }
    else if (errno == EISCONN) {
      m_connection->setState(Connected);
      return true;
    }
    else {
      m_connection->setState(Failed);
      LOG_ERROR << "Connect to " << m_connection->peerAddress().to_string() << " failed, errno = " << errno;
      return false;
    }
  }

  waitConnecting();   // 阻塞等待连接成功或失败
  return m_connection->getState() == Connected;
}

void TcpClient::disconnect()
{
  if (m_connection->getState() == Connected || m_connection->getState() == HalfClosing)
    m_connection->close();
}

bool TcpClient::recv(std::string &buf)
{
  m_eventloop->assertInLoopThread();

  buf.clear();
  if (m_connection->getState() != Connected)
    return false;
  FdEvent::s_ptr fd_event = std::make_shared<FdEvent>(m_connection->socketfd());
  fd_event->listen(EPOLLIN | EPOLLET, std::bind(&TcpConnection::handleRead, m_connection.get(), true));
  m_eventloop->addEpollEvent(fd_event);

  m_eventloop->loop();   // 阻塞调用，handleRead执行完或者定时器触发时会返回
  if (m_connection->getState() != Connected)
    return false;
  buf = std::move(m_connection->data());
  m_connection->clearData();
  return true;
}

bool TcpClient::send(const std::string &str)
{
  m_eventloop->assertInLoopThread();

  if (m_connection->getState() != Connected)
    return false;
  FdEvent::s_ptr fd_event = std::make_shared<FdEvent>(m_connection->socketfd());

  TcpBuffer tmp_buf(str);
  swap(*m_connection->m_out_buffer, tmp_buf);

  fd_event->listen(EPOLLOUT | EPOLLET, std::bind(&TcpConnection::handleWrite, m_connection.get(), true));
  m_eventloop->addEpollEvent(fd_event);

  m_eventloop->loop();   // 阻塞调用，handleWrite执行完或者定时器触发时会返回
  if (m_connection->getState() != Connected)
    return false;
  return true;
}

bool TcpClient::send(const char *str)
{
  return send(std::string(str));
}

bool TcpClient::send(const char *str, std::size_t len)
{
  return send(std::string(str, len));
}

void TcpClient::setTimer(uint64_t interval)
{
  m_eventloop->assertInLoopThread();

  m_connection->addTimer(
    "client_timer",
    interval,
    [this](TcpConnection &conn){
      LOG_INFO << "client time out";
      conn.close();
      this->m_eventloop->stop();
    }
  );
}

void TcpClient::resetTimer()
{
  m_eventloop->assertInLoopThread();
  m_connection->resetTimer("client_timer");
}

void TcpClient::resetTimer(uint64_t interval)
{
  m_eventloop->assertInLoopThread();
  m_connection->resetTimer("client_timer", interval);
}

void TcpClient::cancelTimer()
{
  m_eventloop->assertInLoopThread();
  m_connection->cancelTimer("client_timer");
}

void TcpClient::waitConnecting()
{
  auto conn = m_connection;
  // 绑定可读事件的回调函数
  FdEvent::s_ptr fd_event = std::make_shared<FdEvent>(m_connection->socketfd());
  fd_event->listen(
    EPOLLOUT | EPOLLET,
    [conn, this](){
      int error = 0;
      socklen_t len = sizeof(error);
      // 发起connect后，有可读事件发生即意味着连接建立或者失败，通过getsockopt来确认
      if (getsockopt(conn->socketfd(), SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
        LOG_ERROR << "getsockopt failed, errno = " << errno;
        conn->setState(Failed);
        this->m_eventloop->stop();
        return;
      }
      if (error != 0) {
        LOG_ERROR << "Connect to " << conn->peerAddress().to_string() << " failed, errno = " << errno;
        conn->setState(Failed);
        this->m_eventloop->stop();
        return;
      }
      if (conn->getState() == NotConnected)
        conn->setState(Connected);
      this->m_eventloop->stop();
    }
  );
  m_eventloop->addEpollEvent(fd_event);

  // 等待状态改变
  while (m_connection->getState() == NotConnected) {
    m_eventloop->loop();
  }

  // 取消超时定时器，删除epoll监听事件
  m_connection->cancelTimer("connect_timeout");
  m_eventloop->deleteEpollEvent(m_connection->socketfd());
}

