#include "example/echo_server.h"

#include <functional>
#include <iostream>
#include <string>

#include "zest/base/logging.h"

using std::placeholders::_1;

EchoServer::EchoServer(zest::net::NetBaseAddress &local_addr, int num)
  : m_server(local_addr, num)
{
  m_server.setOnConnectionCallback(std::bind(&EchoServer::onConnectionCallback, this, _1));
  m_server.setMessageCallback(std::bind(&EchoServer::onMessageCallback, this, _1));
  m_server.setWriteCompleteCallback(std::bind(&EchoServer::writeCompleteCallback, this, _1));
  m_server.setCloseCallback(std::bind(&EchoServer::closeCallback, this, _1));
}

void EchoServer::start()
{
  m_server.start();
}

void EchoServer::onConnectionCallback(zest::net::TcpConnection &conn)
{
  conn.Put<std::string>("data_buffer");

  // 断开10s不活跃的连接
  conn.addTimer(
    "clear_inactive_connection", 
    10000, 
    [](zest::net::TcpConnection &conn){
      LOG_INFO << "disconnect with inactive connection " << conn.peerAddress().to_string();
      conn.shutdown();
    },
    false);
  conn.waitForMessage();
}

void EchoServer::onMessageCallback(zest::net::TcpConnection &conn)
{
  // 重置定时器
  conn.resetTimer("clear_inactive_connection");

  std::string msg = conn.data();
  conn.clearData();
  conn.Get<std::string>("data_buffer")->append(msg);   // 把所有收到的数据存起来，在断开连接时打印
  conn.send(msg);
}

void EchoServer::writeCompleteCallback(zest::net::TcpConnection &conn)
{
  conn.waitForMessage();
}

void EchoServer::closeCallback(zest::net::TcpConnection &conn)
{
  /* do nothing */
}