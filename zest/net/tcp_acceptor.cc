/* 对网络监听套接字的封装 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/tcp_acceptor.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "zest/base/logging.h"
#include "zest/base/util.h"
#include "zest/net/inet_addr.h"

using namespace zest;
using namespace zest::net;

TcpAcceptor::TcpAcceptor(AddressPtr addr) : m_local_addr(addr), m_domain(addr->family())
{
  // 验证地址是否合法
  if (!addr->check()) {
    LOG_FATAL << "invalid address: " << addr->to_string();
    exit(-1);   // 连监听套接字都没有，服务器完全不能使用，因此直接退出
  }

  m_listenfd = socket(m_domain, SOCK_STREAM, 0);
  if (m_listenfd == -1) {
    LOG_FATAL << "socket failed";
    exit(-1);
  }
  if (set_non_blocking(m_listenfd) == false) {
    LOG_FATAL << "set listenfd nonblocking failed";
    close(m_listenfd);
    exit(-1);
  }

  // 终止 time-wait
  int reuse = 1;
  if (setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
      LOG_ERROR << "setsockopt REUSEADDR failed, errno = " << errno;
  }

  if (bind(m_listenfd, m_local_addr->sockaddr(), m_local_addr->socklen()) == -1) {
    LOG_FATAL << "bind failed, errno = " << errno;
    close(m_listenfd);
    exit(-1);
  }

  LOG_DEBUG << "create acceptor successful";
}

void TcpAcceptor::listen()
{
  int ret = ::listen(m_listenfd, 1000);
  if (ret == -1) {
    LOG_FATAL << "listen failed, errno = " << errno;
    close(m_listenfd);
    exit(-1);
  }
}

// 接受新连接，并返回客户端套接字和地址
std::unordered_map<int, NetBaseAddress::s_ptr> TcpAcceptor::accept()
{
  std::unordered_map<int, NetBaseAddress::s_ptr> new_clients;
  if (m_domain == PF_INET) {
    sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    memset(&client_addr, 0, len);
    int clientfd;
    while ((clientfd = ::accept(m_listenfd, reinterpret_cast<sockaddr*>(&client_addr), &len)) != -1) {
      InetAddress::s_ptr peer_addr = std::make_shared<InetAddress>(client_addr);
      if (peer_addr->check() == false) {
        LOG_ERROR << "invalid peer address";
        close(clientfd);
        continue;
      }
      new_clients.insert({clientfd, peer_addr});
      memset(&client_addr, 0, len);
    }
  }
  else {
    // other protocol...
    LOG_ERROR << "Unknow protocol families: " << m_domain;
  }
  
  return new_clients;
}
