/* 一个简单的echo客户端，每隔一秒发送一次数据 */
#include <iostream>
#include <string>
#include "zest/net/tcp_client.h"
#include "zest/base/logging.h"

int main()
{
  zest::Logger::InitGlobalLogger("DEBUG", "echo_client", "../logs/", 1000000, 1000, 25);

  zest::net::InetAddress server_addr("127.0.0.1", 12345);
  zest::net::TcpClient echo_client(server_addr);

  echo_client.connection().Put<int>("count", 0);

  // 客户端周期性发送数据
  echo_client.connection().addTimer(
    "send message periodic", 
    1000, 
    [](zest::net::TcpConnection &conn) {
      int *count = conn.Get<int>("count");
      ++(*count);
      std::string message = "count = ";
      message += std::to_string(*count);
      conn.send(message);
    },
    true
  );

  // 客户端发送数据后等待读取
  echo_client.connection().setWriteCompleteCallback(
    [](zest::net::TcpConnection &conn) {
      LOG_DEBUG << "成功发送数据，等待数据到达";
      conn.waitForMessage();
    }
  );

  // 客户端收到数据后打印
  echo_client.connection().setMessageCallback(
    [](zest::net::TcpConnection &conn) {
      std::string message = conn.data();
      conn.clearData();
      std::cout << message << std::endl;
    }
  );

  // 断开连接后关闭客户端
  echo_client.connection().setCloseCallback(
    [&echo_client](zest::net::TcpConnection &conn) {
      echo_client.stop();
    }
  );

  // 客户端运行20s后断开连接
  echo_client.addTimer(
    "echo_client_countdown",
    20000,
    [&echo_client]() {
      echo_client.connection().close();
    }
  );

  echo_client.start();

  return 0;
}