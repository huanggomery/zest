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

  ~TcpClient();

  bool connect();

  void disconnect();

  bool recv(std::string &buf);

  bool send(const std::string &str);  // 发送数据
  bool send(const char *str);
  bool send(const char *str, std::size_t len);

  void setTimer(uint64_t interval);

  void resetTimer();

  void resetTimer(uint64_t interval);

  void cancelTimer();

 private:
  void waitConnecting();

 private:
  NetBaseAddress::s_ptr m_peer_address;
  std::shared_ptr<EventLoop> m_eventloop;
  TcpConnection::s_ptr m_connection {nullptr};
};
  
} // namespace net
} // namespace zest

#endif // ZEST_NET_TCP_CLIENT_H
