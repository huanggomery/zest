/* 启动echo服务器的主函数 */
#include "example/echo_server.h"


int main()
{
  zest::Logger::InitGlobalLogger("DEBUG", "echo_server", "../logs/", 5000000, 1000, 25);
  zest::net::InetAddress local_addr("127.0.0.1", 12345);
  EchoServer server(local_addr);
  server.start();

  return 0;
}