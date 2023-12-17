/* 封装IPv4地址 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_NET_INET_ADDR_H
#define ZEST_NET_INET_ADDR_H

#include <arpa/inet.h>
#include <string>
#include "zest/net/base_addr.h"

namespace zest
{
namespace net
{

class InetAddress : public NetBaseAddress
{
 public:
  InetAddress() = delete;
  ~InetAddress() = default;

  // 各种形式的构造函数
  InetAddress(const std::string &addr_str);
  InetAddress(const std::string &ip_str, uint16_t port);
  InetAddress(const char *addr_str);
  InetAddress(const char *ip_str, uint16_t port);
  InetAddress(sockaddr_in addr);
  InetAddress(in_addr_t ip, uint16_t port);

  ::sockaddr* sockaddr() override;
  socklen_t socklen() const override;
  int family() const override;
  std::string to_string() const override;
  bool check() const override;
  NetBaseAddress::s_ptr copy() const override;

 private:
  sockaddr_in m_sockaddr;
  std::string m_ip;
  uint16_t m_port {0};
};
  
} // namespace net
} // namespace zest

#endif // ZEST_NET_INET_ADDR_H
