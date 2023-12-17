#ifndef ZEST_EXAMPLE_ECHO_SERVER_H
#define ZEST_EXAMPLE_ECHO_SERVER_H

#include "zest/net/tcp_server.h"


class EchoServer
{
 public:
  explicit EchoServer(zest::net::NetBaseAddress &local_addr, int num = 4);

  void start();

  void onConnectionCallback(zest::net::TcpConnection &conn);
  void onMessageCallback(zest::net::TcpConnection &conn);
  void writeCompleteCallback(zest::net::TcpConnection &conn);
  void closeCallback(zest::net::TcpConnection &conn);
  
 private:
  zest::net::TcpServer m_server;
};

#endif // ZEST_EXAMPLE_ECHO_SERVER_H
