#ifndef ZEST_NET_TCP_CLIENT_H
#define ZEST_NET_TCP_CLIENT_H

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#include <atomic>
#include <functional>
#include <memory>

#include "zest/base/logging.h"
#include "zest/base/noncopyable.h"
#include "zest/net/tcp_connection.h"

namespace zest
{
namespace net
{

class EventLoop;

// FIXME: may be unsafe
class TcpClient: public noncopyable
{
  using ConnectionCallbackFunc = std::function<void(TcpConnection&)>;

 public:
  TcpClient(NetBaseAddress &peer_addr);
  
  ~TcpClient() = default;

  TcpConnection& connection()
  { return *m_connection; }

  void start();

  void stop();

  void addTimer(uint64_t interval, std::function<void()> cb, bool periodic = false);

  void setOnConnectionCallback(const ConnectionCallbackFunc &cb)
  { m_on_connection_callback = cb; }

private:
  void connect();

 private:
  NetBaseAddress::s_ptr m_peer_address;
  std::shared_ptr<EventLoop> m_eventloop;
  TcpConnection::s_ptr m_connection {nullptr};
  std::atomic<bool> m_running {false};

  // 连接成功的回调函数
  ConnectionCallbackFunc m_on_connection_callback {nullptr};
};
  
} // namespace net
} // namespace zest

#endif // ZEST_NET_TCP_CLIENT_H
