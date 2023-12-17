/* 对网络监听套接字的封装 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_TCP_ACCEPTOR_H
#define ZEST_NET_TCP_ACCEPTOR_H

#include <memory>
#include <unordered_map>

#include "zest/base/noncopyable.h"


namespace zest
{
namespace net
{

class NetBaseAddress;  // 前向声明

class TcpAcceptor : public noncopyable
{
  using AddressPtr = std::shared_ptr<NetBaseAddress>;

 public:
  using s_ptr = std::shared_ptr<TcpAcceptor>;

  TcpAcceptor(AddressPtr addr);
  void listen();
  int socketfd() const {return m_listenfd;}

  // 接受新连接，并返回客户端套接字和地址
  std::unordered_map<int, AddressPtr> accept();
  
 private:
  AddressPtr m_local_addr;
  int m_domain;
  int m_listenfd;
};
  
} // namespace net
} // namespace zest

#endif // ZEST_NET_TCP_ACCEPTOR_H
