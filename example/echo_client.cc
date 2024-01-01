/* 一个简单的echo客户端，每隔一秒发送一次数据 */
#include <unistd.h>
#include <iostream>
#include <string>
#include "zest/base/logging.h"
#include "zest/net/tcp_client.h"

int main()
{
  zest::Logger::InitGlobalLogger("DEBUG", "echo_client", "../logs/", 1000000, 1000, 25);

  zest::net::InetAddress server_addr("127.0.0.1", 12345);
  zest::net::TcpClient echo_client(server_addr);

  echo_client.connect();
  echo_client.setTimer(20*1000);
  int i = 1;
  while (1) {
    std::string send_msg = std::string("hello, count = ") + std::to_string(i);
    if (echo_client.send(send_msg) == false) {
      std::cerr << "send failed" << std::endl;
      exit(-1);
    }
    std::string recv_msg;
    if (echo_client.recv(recv_msg) == false) {
      std::cerr << "recv failed" << std::endl;
      exit(-1);
    }
    std::cout << recv_msg << std::endl;
    sleep(1);
    ++i;
  }

  return 0;
}