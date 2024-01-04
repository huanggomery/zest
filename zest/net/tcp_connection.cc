/* 封装TCP连接，需要有读写操作、业务处理以及修改epoll监听事件 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/tcp_connection.h"

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "zest/base/logging.h"
#include "zest/net/eventloop.h"
#include "zest/net/fd_event.h"
#include "zest/net/tcp_buffer.h"
#include "zest/net/timer_container.h"
#include "zest/net/timer_event.h"

using namespace zest;
using namespace zest::net;


TcpConnection::TcpConnection(int fd, EventLoopPtr eventloop, NetAddrPtr peer_addr) :
  m_sockfd(fd), m_eventloop(eventloop), m_peer_addr(peer_addr), 
  m_in_buffer(new TcpBuffer()), m_out_buffer(new TcpBuffer()),
  m_state(Connected), m_fd_event(new FdEvent(m_sockfd)),
  m_timer_container(new TimerContainer<std::string>(eventloop))
{
  m_fd_event->set_non_blocking();
}

TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::~TcpConnection(), this = " << this << ", fd = " << m_sockfd;
  // FIXME: What if m_state == Connected now?
  // assert(m_state == Closed || m_state == NotConnected);
  delete m_timer_container;
}

void TcpConnection::waitForMessage()
{
  if (m_eventloop->isThisThread()) {
    if (m_state != Connected && m_state != HalfClosing)
      return;
    m_fd_event->listen(EPOLLIN | EPOLLET, std::bind(&TcpConnection::handleRead, this, false));
    m_eventloop->addEpollEvent(m_fd_event);
  }
  else {
    m_eventloop->runInLoop(std::bind(&TcpConnection::waitForMessage, this));
  }
}

std::string TcpConnection::data() const
{
  return m_in_buffer->to_string();
}

std::size_t TcpConnection::dataSize() const
{
  return m_in_buffer->size();
}

void TcpConnection::send(const std::string &str)
{
  if (m_eventloop->isThisThread()) {
    if (m_state != Connected)
      return;
    
    TcpBuffer tmp_buf(str);
    swap(*m_out_buffer, tmp_buf);

    m_fd_event->listen(EPOLLOUT | EPOLLET, std::bind(&TcpConnection::handleWrite, this, false));
    m_eventloop->addEpollEvent(m_fd_event);
  }
  else {
    m_eventloop->runInLoop([str, this](){this->send(str);});
  }
}

void TcpConnection::send(const char *str)
{
  send(std::string(str));
}

void TcpConnection::send(const char *str, std::size_t len)
{
  send(std::string(str, len));
}

void TcpConnection::clearData()
{
  m_in_buffer->clear();
}

void TcpConnection::clearBytesData(int bytes)
{
  if (m_in_buffer->size() > bytes)
    m_in_buffer->move_forward(bytes);
  else {
    LOG_ERROR << "attempt to drop " << bytes << " bytes data, but only" << m_in_buffer->size() << " available!";
    m_in_buffer->clear();
  }
}

void TcpConnection::shutdown()
{
  if (m_eventloop->isThisThread()) {
    if (m_state == Connected) {
      ::shutdown(m_sockfd, SHUT_WR);
      setState(HalfClosing);
      waitForMessage();
    }
  }
  else {
   m_eventloop->runInLoop(std::bind(&TcpConnection::shutdown, this)); 
  }
}

void TcpConnection::close()
{
  m_eventloop->assertInLoopThread();
  assert(m_state == Connected || m_state == HalfClosing);

  // 把定时器全部取消
  clearTimer();
  
  setState(Closed);
  if (m_close_callback)
    m_close_callback(*this);
  deleteFromEventLoop();

  // ::close()必须放在最后
  // 因为close后该fd可能会被其它线程重用
  // 导致 TcpServer的ConnectionMap 中当前对象的智能指针被覆盖，当前对象被析构
  // 所以必须保证::close()后不再使用当前对象的任何资源
  LOG_DEBUG << "TcpConnection::close(), this = " << this << ", sockfd = " << m_sockfd;
  ::close(m_sockfd);
}

void TcpConnection::handleRead(bool client)
{
  if (m_state != Connected && m_state != HalfClosing)
    return;
  bool is_error = false, is_closed = false, is_finished = false;
  char tmp_buf[1500];

  if (m_state == HalfClosing)
    is_closed = true;

  ssize_t recv_len = 0;
  while (!is_error && !is_closed && !is_finished) {
    memset(tmp_buf, 0, sizeof(tmp_buf));
    ssize_t len = ::recv(m_sockfd, tmp_buf, sizeof(tmp_buf), 0);
    if (len < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        is_finished = true;
      else if (errno == EINTR)
        continue;
      else
        is_error = true;
    }
    else if (len == 0) {
      is_closed = true;
    }
    // 正常情况
    else {
      m_in_buffer->append(tmp_buf, len);
      recv_len += len;
      if (len < sizeof(tmp_buf))
        is_finished = true;
    }
  }

  // 出错的情况，半关闭连接，然后等待对端关闭
  if (is_error) {
    LOG_ERROR << "TCP read error, close connection: " << m_peer_addr->to_string() << " errno = " << errno;
    if (!client)
      this->shutdown();
    else {
      if (m_state == Connected) {
        ::shutdown(m_sockfd, SHUT_WR);
        setState(HalfClosing);
      }
    }
    return;
  }
  if (is_closed) {
    LOG_DEBUG << "receive FIN from peer: " << m_peer_addr->to_string();
    this->close();
    if (client)
      m_eventloop->stop();
    return;
  }

  // LOG_DEBUG << "receive " << recv_len << " bytes data from " << m_peer_addr->to_string();
  if (m_message_callback)
    m_message_callback(*this);
  if (client)
    m_eventloop->stop();
}

void TcpConnection::handleWrite(bool client)
{
  if (m_state != Connected)
    return;
  bool is_error = false;

  while (!m_out_buffer->empty()) {
    const char *ptr = m_out_buffer->c_str();
    ssize_t len = ::send(m_sockfd, ptr, m_out_buffer->size(), 0);
    if (len == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else if (errno = EINTR)
        continue;
      else {
        is_error = true;
        break;
      }
    }
    m_out_buffer->move_forward(len);
  }

  if (is_error) {
    LOG_ERROR << "TCP write error, shutdown connection";
    if (!client)
      this->shutdown();
    else {
      if (m_state == Connected) {
        ::shutdown(m_sockfd, SHUT_WR);
        setState(HalfClosing);
        m_fd_event->listen(EPOLLIN | EPOLLET, std::bind(&TcpConnection::handleRead, this, true));
        m_eventloop->addEpollEvent(m_fd_event);
      }
    }
    return;
  }

  // 缓冲区没写完，遇到 EAGAIN 或 EWOULDBLOCK 错误，继续等待套接字可写
  if (!m_out_buffer->empty()) {
    return;
  }

  // LOG_DEBUG << "send data to " << m_peer_addr->to_string();
  if (m_write_complete_callback)
    m_write_complete_callback(*this);
  if (client)
    m_eventloop->stop();
}

void TcpConnection::addTimer(const std::string &timer_name, uint64_t interval,
                             ConnectionCallbackFunc cb, bool periodic /*=false*/)
{
  m_timer_container->addTimer(
    timer_name,
    interval,
    [this, cb](){
      if (this->m_state == Closed || this->m_state == NotConnected)
        return;
      cb(*this);
    },
    periodic
  );
}

void TcpConnection::resetTimer(const std::string &timer_name)
{
  m_timer_container->resetTimer(timer_name);
}

void TcpConnection::resetTimer(const std::string &timer_name, uint64_t interval)
{
  m_timer_container->resetTimer(timer_name, interval);
}

void TcpConnection::cancelTimer(const std::string &timer_name)
{
  m_timer_container->cancelTimer(timer_name);
}

void TcpConnection::clearTimer()
{
  m_timer_container->clearTimer();
}

void TcpConnection::deleteFromEventLoop()
{
  m_eventloop->deleteEpollEvent(m_sockfd);
}
