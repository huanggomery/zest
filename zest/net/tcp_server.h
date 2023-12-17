/* 封装 TcpServer */
/* 主线程，拥有tcp_acceptor、线程池以及自己的eventloop */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_NET_TCP_SERVER_H
#define ZEST_NET_TCP_SERVER_H

#include <functional>
#include <memory>
#include <unordered_map>

#include "zest/base/logging.h"
#include "zest/base/noncopyable.h"
#include "zest/net/base_addr.h"
#include "zest/net/tcp_connection.h"

namespace zest
{
namespace net
{

class EventLoop;
class TcpAcceptor;
class ThreadPool;


class TcpServer: public noncopyable
{
  using ConnectionCallbackFunc = std::function<void(TcpConnection&)>;
  using ConnectionMap = std::unordered_map<int, TcpConnection::s_ptr>;

 public:
  using s_ptr = std::shared_ptr<TcpServer>;

  explicit TcpServer(NetBaseAddress &local_addr, int thread_nums = 4);

  ~TcpServer();

  void setOnConnectionCallback(const ConnectionCallbackFunc &cb)
  { m_on_connection_callback = cb; }

  void setMessageCallback(const ConnectionCallbackFunc &cb)
  { m_message_callback = cb; }

  void setWriteCompleteCallback(const ConnectionCallbackFunc &cb)
  { m_write_complete_callback = cb;}

  void setCloseCallback(const ConnectionCallbackFunc &cb)
  { m_close_callback = cb;}
  
  void start();

 private:

  void handleAccept();

  // 信号产生时的回调函数
  void handleSignal();

  // 清理断开的连接
  void clearClosedConnection();

  // 一个异常安全的创建TcpConnection对象的函数
  TcpConnection::s_ptr createConnection(int sockfd, 
                                    std::shared_ptr<EventLoop> eventloop, 
                                    NetBaseAddress::s_ptr peer_addr) noexcept;

  // 向eventloop添加信号处理事件
  bool addSignalEvent();

  // 优雅地结束服务器
  void shutdown();

 private:
  std::unique_ptr<TcpAcceptor> m_acceptor;        // TCP连接的接收器
  std::shared_ptr<EventLoop> m_main_eventloop;    // 主线程eventloop，负责监听本地地址的套接字
  std::unique_ptr<ThreadPool> m_thread_pool;      // 线程池

  // 所有的TCP连接
  ConnectionMap m_connections;

  // 各种事件的回调函数
  ConnectionCallbackFunc m_on_connection_callback {nullptr};
  ConnectionCallbackFunc m_message_callback {nullptr};
  ConnectionCallbackFunc m_write_complete_callback {nullptr};
  ConnectionCallbackFunc m_close_callback {nullptr};

  // 用于传递信号的管道
  int m_pipefd[2];
};

} // namespace net
} // namespace zest

#endif // ZEST_NET_TCP_SERVER_H
