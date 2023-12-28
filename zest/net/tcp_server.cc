/* 封装 TcpServer */
/* 主线程，拥有tcp_acceptor、线程池以及自己的eventloop */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/tcp_server.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include "zest/base/util.h"
#include "zest/net/base_addr.h"
#include "zest/net/eventloop.h"
#include "zest/net/fd_event.h"
#include "zest/net/io_thread.h"
#include "zest/net/tcp_acceptor.h"
#include "zest/net/thread_pool.h"
#include "zest/net/timer_event.h"

using namespace zest;
using namespace zest::net;


// 清除已断开的连接的间隔
static const uint64_t CLEAR_CLOSED_CONNECTION_INTERVAL = 2000;


TcpServer::TcpServer(NetBaseAddress &local_addr, int thread_nums /*=4*/) :
  m_acceptor(new TcpAcceptor(local_addr.copy())),
  m_main_eventloop(EventLoop::CreateEventLoop()), 
  m_thread_pool(new ThreadPool(thread_nums))
{
  /* do nothing */
}

TcpServer::~TcpServer()
{
  ::close(sig_pipefd[0]);
  ::close(sig_pipefd[1]);
}

void TcpServer::start()
{
  // 添加连接套接字的事件
  FdEvent::s_ptr listenfd_event = std::make_shared<FdEvent>(m_acceptor->socketfd());
  listenfd_event->listen(EPOLLIN | EPOLLET, std::bind(&TcpServer::handleAccept, this));
  m_main_eventloop->addEpollEvent(listenfd_event);

  // 添加定时器定期清理断开的连接
  TimerEvent::s_ptr clear_timer = std::make_shared<TimerEvent>(
    CLEAR_CLOSED_CONNECTION_INTERVAL, 
    std::bind(&TcpServer::clearClosedConnection, this),
    true
  );
  m_main_eventloop->addTimerEvent(clear_timer);

  if (addSignalEvent() == false) {
    std::cerr << "addSignalEvent failed" << std::endl;
    LOG_FATAL << "addSignalEvent failed";
    exit(-1);
  }

  m_thread_pool->start();
  m_acceptor->listen();
  m_main_eventloop->loop();

  LOG_INFO << "TcpServer exit!";
  std::cout << "TcpServer exit!" << std::endl;
}

void TcpServer::handleAccept()
{
  assert(m_main_eventloop->isThisThread());
  
  auto new_clients = m_acceptor->accept();
  for (const auto &client : new_clients) {
    int sockfd = client.first;
    NetBaseAddress::s_ptr peer_addr = client.second;
    if (!peer_addr->check())
      continue;
    IOThread::s_ptr io_thread = m_thread_pool->get_io_thread();
    auto connection = createConnection(sockfd, io_thread->get_eventloop(), peer_addr);
    if (connection) {
      m_connections[sockfd] = connection;
      LOG_INFO << "Accept new connection, fd = " << sockfd << " address: " << peer_addr->to_string();
      // 如果设置了连接回调函数，则执行回调函数；否则等待读取数据
      if (m_on_connection_callback)
        m_on_connection_callback(*connection);
      else {
        connection->waitForMessage();
      }
    }
  }
}

void TcpServer::handleSignal()
{
  char signals[1024];
  int ret = recv(sig_pipefd[0], signals, 1024, 0);
  if (ret <= 0)
    return;
  for (int i = 0; i < ret; ++i) {
    switch (signals[i])
    {
    case SIGINT:
      LOG_INFO << "receive SIGINT";
    case SIGTERM:
      LOG_INFO << "receive SIGTERM";
      this->shutdown();
      break;
    
    default:
      LOG_ERROR << "get some signals, and don't know how to handle";
      break;
    }
  }
}

// 清理断开的连接
void TcpServer::clearClosedConnection()
{
  assert(m_main_eventloop->isThisThread());

  auto it = m_connections.begin();
  while (it != m_connections.end()) {
    if (it->second == nullptr || it->second->getState() == Closed) {
      if (it->second) {
        LOG_DEBUG << "remove closed connection: " << it->second->peerAddress().to_string();
      }
      it = m_connections.erase(it);
    }
    else
      ++it;
  }
}

// 一个异常安全的创建TcpConnection对象的函数
std::shared_ptr<TcpConnection> TcpServer::createConnection(int sockfd, 
                                    std::shared_ptr<EventLoop> eventloop, 
                                    NetBaseAddress::s_ptr peer_addr) noexcept
{
  TcpConnection::s_ptr connection = nullptr;
  try {
    connection.reset(new TcpConnection(sockfd, eventloop, peer_addr));
  }
  catch(const std::bad_alloc &e) {
    LOG_ERROR << "create new connection object failed, bad alloc";
    return nullptr;
  }
  catch(const std::exception &e) {
    LOG_ERROR << "create new connection object failed, unknow reason";
    return nullptr;
  }

  connection->setMessageCallback(m_message_callback);
  connection->setWriteCompleteCallback(m_write_complete_callback);
  connection->setCloseCallback(m_close_callback);
  
  return connection;
}

// 向eventloop添加信号处理事件
bool TcpServer::addSignalEvent()
{
  if (socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd) == -1) {
    LOG_ERROR << "socketpair failed, errno = " << errno;
    return false;
  }

  if (!set_non_blocking(sig_pipefd[0]) || !set_non_blocking(sig_pipefd[1])) {
    LOG_ERROR << "set sig_pipefd nonblocking failed, errno" << errno;
    return false;
  }

  bool ret = true;
  ret = ret && add_signal(SIGINT);
  ret = ret && add_signal(SIGTERM);
  if (!ret) {
    LOG_ERROR << "add_signal failed";
    return false;
  }

  FdEvent::s_ptr sig_pipefd_event = std::make_shared<FdEvent>(sig_pipefd[0]);
  sig_pipefd_event->listen(EPOLLIN | EPOLLET, std::bind(&TcpServer::handleSignal, this));
  m_main_eventloop->addEpollEvent(sig_pipefd_event);

  return true;
}

void TcpServer::shutdown()
{
  m_main_eventloop->stop();
  m_thread_pool->stop();
}
