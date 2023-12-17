/* 封装IPv4地址 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/inet_addr.h"
#include <string.h>
#include "zest/base/logging.h"

using namespace zest;
using namespace zest::net;

InetAddress::InetAddress(const std::string &addr_str)
{
  memset(&m_sockaddr, 0, sizeof(m_sockaddr));
  std::size_t pos = addr_str.find_first_of(':');
  if (pos == std::string::npos) {
    LOG_ERROR << "invalid IPv4 address: " << addr_str;
    return;
  }

  m_ip = addr_str.substr(0, pos);
  m_port = std::stoi(addr_str.substr(pos+1));
  m_sockaddr.sin_family = AF_INET;
  m_sockaddr.sin_addr.s_addr = inet_addr(m_ip.c_str());
  m_sockaddr.sin_port = htons(m_port);
}

InetAddress::InetAddress(const std::string &ip_str, uint16_t port) :
  m_ip(ip_str), m_port(port)
{
  memset(&m_sockaddr, 0, sizeof(m_sockaddr));
  m_sockaddr.sin_family = AF_INET;
  m_sockaddr.sin_addr.s_addr = inet_addr(ip_str.c_str());
  m_sockaddr.sin_port = htons(port);
}

InetAddress::InetAddress(const char *addr_str) : InetAddress(std::string(addr_str))
{
  /* 使用了委托构造函数 */
}

InetAddress::InetAddress(const char *ip_str, uint16_t port) :
  InetAddress(std::string(ip_str), port)
{
  /* 使用了委托构造函数 */
}

InetAddress::InetAddress(sockaddr_in addr) : m_sockaddr(addr)
{
  m_ip = std::string(inet_ntoa(m_sockaddr.sin_addr));
  m_port = ntohs(m_sockaddr.sin_port);
}

InetAddress::InetAddress(in_addr_t ip, uint16_t port) : m_port(port)
{
  memset(&m_sockaddr, 0, sizeof(m_sockaddr));
  m_sockaddr.sin_addr.s_addr = ip;
  m_sockaddr.sin_family = AF_INET;
  m_sockaddr.sin_port = htons(port);
  m_ip = std::string(inet_ntoa(m_sockaddr.sin_addr));
}

::sockaddr* InetAddress::sockaddr()
{
  return reinterpret_cast<::sockaddr*>(&m_sockaddr);
}

socklen_t InetAddress::socklen() const
{
  return sizeof(m_sockaddr);
}

int InetAddress::family() const
{
  return AF_INET;
}

std::string InetAddress::to_string() const
{
  return m_ip + ':' + std::to_string(m_port);
}

bool InetAddress::check() const
{
  // 检查IP地址
  if (m_ip.empty() || inet_addr(m_ip.c_str()) == INADDR_NONE) {
    return false;
  }

  // 检查端口号
  if (m_port == 0) {
    return false;
  }
  
  return true;
}

NetBaseAddress::s_ptr InetAddress::copy() const
{
  return std::make_shared<InetAddress>(*this);
}
