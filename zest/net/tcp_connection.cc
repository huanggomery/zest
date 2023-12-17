/* 封装TCP连接，需要有读写操作、业务处理以及修改epoll监听事件 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/tcp_connection.h"

#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#include "zest/base/logging.h"
#include "zest/net/eventloop.h"
#include "zest/net/fd_event.h"
#include "zest/net/tcp_buffer.h"
#include "zest/net/timer_event.h"

using namespace zest;
using namespace zest::net;


TcpConnection::TcpConnection(int fd, EventLoopPtr eventloop, NetAddrPtr peer_addr) :
  m_sockfd(fd), m_eventloop(eventloop), m_peer_addr(peer_addr), 
  m_in_buffer(new TcpBuffer()), m_out_buffer(new TcpBuffer()),
  m_state(Connected), m_fd_event(new FdEvent(m_sockfd))
{
  m_fd_event->set_non_blocking();
}

TcpConnection::~TcpConnection()
{
  if (m_state == Closed || m_state == NotConnected)
    return;
  deleteFromEventLoop();
  this->close();
  setState(Closed);
}

void TcpConnection::waitForMessage()
{
  if (m_eventloop->isThisThread()) {
    if (m_state != Connected)
      return;
    m_fd_event->listen(EPOLLIN | EPOLLET, std::bind(&TcpConnection::handleRead, this));
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

void TcpConnection::send(const std::string &str)
{
  if (m_eventloop->isThisThread()) {
    if (m_state != Connected)
      return;
    
    TcpBuffer tmp_buf(str);
    swap(*m_out_buffer, tmp_buf);

    m_fd_event->listen(EPOLLOUT | EPOLLET, std::bind(&TcpConnection::handleWrite, this));
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
  if (m_eventloop->isThisThread()) {
    if (m_state == Connected || m_state == HalfClosing) {
      // 把定时器全部取消
      for (auto it = m_timer_map.begin(); it != m_timer_map.end();) {
        it->second->set_valid(false);
        it = m_timer_map.erase(it);
      }
      
      ::close(m_sockfd);
      if (m_close_callback)
        m_close_callback(*this);
      setState(Closed);
      deleteFromEventLoop();
    }
  }
  else {
    m_eventloop->runInLoop(std::bind(&TcpConnection::close, this));
  }
}

void TcpConnection::handleRead()
{
  bool is_error = false, is_closed = false, is_finished = false;
  char tmp_buf[1024];

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
    }
  }

  // 出错的情况，半关闭连接，然后等待对端关闭
  if (is_error) {
    this->close();
    LOG_ERROR << "TCP read error, close connection: " << m_peer_addr->to_string() << " errno = " << errno;
    return;
  }
  if (is_closed) {
    this->close();
    LOG_DEBUG << "receive FIN from peer: " << m_peer_addr->to_string();
    return;
  }

  // LOG_DEBUG << "receive " << recv_len << " bytes data from " << m_peer_addr->to_string();
  if (m_message_callback)
    m_message_callback(*this);
}

void TcpConnection::handleWrite()
{
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
    this->shutdown();
    LOG_ERROR << "TCP write error, shutdown connection";
    return;
  }

  // LOG_DEBUG << "send data to " << m_peer_addr->to_string();
  if (m_write_complete_callback)
    m_write_complete_callback(*this);
}

void TcpConnection::addTimer(const std::string &timer_name, uint64_t interval,
                             ConnectionCallbackFunc cb, bool periodic /*=false*/)
{
  if (m_eventloop->isThisThread()) {
    if (m_timer_map.find(timer_name) != m_timer_map.end())
      return;
    std::shared_ptr<TimerEvent> timer_event = std::make_shared<TimerEvent>(
      interval, 
      [this, cb, timer_name](){
        cb(*this);
        // 如果定时器不是周期性的，在触发后，需要在TcpConnection中删除，避免内存泄漏
        if (this->m_timer_map[timer_name]->is_periodic() == false) {
          this->m_timer_map.erase(timer_name);
        }
      },
      periodic);
    m_timer_map.insert({timer_name, timer_event});
    m_eventloop->addTimerEvent(timer_event);
  }
  else {
    m_eventloop->runInLoop(std::bind(&TcpConnection::addTimer, this, timer_name, interval, cb, periodic));
  }
}

void TcpConnection::resetTimer(const std::string &timer_name)
{
  if (m_eventloop->isThisThread()) {
    auto old_timer = m_timer_map.find(timer_name);
    if (old_timer == m_timer_map.end())
      return;
    old_timer->second->set_valid(false);  // 将原先的定时器取消

    // 创建新的定时器
    auto new_timer = std::make_shared<TimerEvent>(*(old_timer->second));
    m_timer_map[timer_name] = new_timer;
    m_eventloop->addTimerEvent(new_timer);
  }
  else {
    auto cb = [this, timer_name]() {this->resetTimer(timer_name);};
    m_eventloop->runInLoop(cb);
  }
}

void TcpConnection::resetTimer(const std::string &timer_name, uint64_t interval)
{
  if (m_eventloop->isThisThread()) {
    auto old_timer = m_timer_map.find(timer_name);
    if (old_timer == m_timer_map.end())
      return;
    old_timer->second->set_valid(false);  // 将原先的定时器取消

    // 创建新的定时器
    auto new_timer = std::make_shared<TimerEvent>(
      interval, old_timer->second->handler(), old_timer->second->is_periodic()
    );
    m_timer_map[timer_name] = new_timer;
    m_eventloop->addTimerEvent(new_timer);
  }
  else {
    auto cb = [this, timer_name, interval]() {this->resetTimer(timer_name, interval);};
    m_eventloop->runInLoop(cb);
  }
}

void TcpConnection::cancelTimer(const std::string &timer_name)
{
  if (m_eventloop->isThisThread()) {
    auto timer = m_timer_map.find(timer_name);
    if (timer == m_timer_map.end())
      return;
    timer->second->set_valid(false);
  }
  else {
    auto cb = [this, timer_name]() {this->cancelTimer(timer_name);};
    m_eventloop->runInLoop(cb);
  }
}

void TcpConnection::deleteFromEventLoop()
{
  m_eventloop->deleteEpollEvent(m_sockfd);
}
