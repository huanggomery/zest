/* 对echo服务器进行压力测试 */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "zest/net/tcp_client.h"

int seconds = 0;
int clients = 0;
zest::net::InetAddress::s_ptr server_address = nullptr;

// 生成指定范围内长度的随机字符串函数
std::string generateRandomString(int minLength, int maxLength) {
  // 定义包含数字、字母和符号的字符集合
  const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()_+{}[]|;:,.<>?";

  // 创建随机数引擎
  std::random_device rd;
  std::mt19937 gen(rd());

  // 创建均匀分布器
  std::uniform_int_distribution<> lengthDistribution(minLength, maxLength);

  int length = lengthDistribution(gen); // 生成随机长度
  std::uniform_int_distribution<> dis(0, characters.length() - 1);

  std::string randomString;
  for (int i = 0; i < length; ++i) {
    // 生成随机索引并获取对应字符
    int index = dis(gen);
    randomString += characters[index];
  }

  return randomString;
}

// 显示帮助信息
void showHelp()
{
  std::string help_msg = 
" \
Usage: ./echo_bench [options] \n \
Options: \n \
-t Runing time (seconds)\n \
-c Clients number\n \
-s Server address\n \
-h Show help information. \n \
For example: ./echo_bench -t 60 -c 10 -s 127.0.0.1:12345\n \
";

  std::cout << help_msg;
}

std::pair<int,int> echoTest()
{
  std::pair<int,int> count{0, 0};  // 发送消息次数和服务器正确响应次数
  zest::net::TcpClient client(*server_address);
  client.addTimer(
    "test_time",
    seconds * 1000,
    [&client](){
      client.connection().close();
    }
  );

  std::string msg = generateRandomString(1, 50);
  client.setOnConnectionCallback(
    [msg, &count](zest::net::TcpConnection &conn){
      conn.Put<std::string>("msg_send", msg);
      conn.send(msg);
    }
  );
  client.connection().setMessageCallback(
    [&count](zest::net::TcpConnection &conn) {
      std::string *msg_send = conn.Get<std::string>("msg_send");
      std::string msg_recv = conn.data();
      conn.clearData();
      if (msg_recv == *msg_send)
        ++count.second;
      else {
        std::cout << count.first << std::endl;
        std::cout << "send: " << *msg_send << std::endl;
        std::cout << "recv: " << msg_recv << std::endl;
      }
      *msg_send = generateRandomString(1, 50);
      conn.send(*msg_send);
    }
  );
  client.connection().setWriteCompleteCallback(
    [&count](zest::net::TcpConnection &conn) {
      ++count.first;
      conn.waitForMessage();
    }
  );
  client.connection().setCloseCallback(
    [&client](zest::net::TcpConnection &conn) {
      client.stop();
    }
  );
  client.start();
  return count;
}

int main(int argc, char *argv[])
{
  // 先解析参数
  int opt;
  const char *str = "t:s:c:h";
  while ((opt = getopt(argc, argv, str)) != -1)
  {
    switch (opt)
    {
    case 't':
      seconds = atoi(optarg);
      break;
    case 's':
      server_address.reset(new zest::net::InetAddress(optarg));
      break;
    case 'c':
      clients = atoi(optarg);
      break;
    case 'h':
      showHelp();
      exit(0);
    default:
      showHelp();
      exit(-1);
    }
  }

  // 检查是否指定了必要的参数
  if (seconds == 0 || clients == 0 || server_address == nullptr) {
    showHelp();
    exit(-1);
  }

  int *pipefd = new int[2 * clients];
  for (int i = 0; i < clients; ++i) {
    if (pipe(pipefd + 2*i) == -1) {
      std::cerr << "pipe failed" << std::endl;
      exit(-1);
    }
  }

  std::vector<int> pid(clients, 0);
  std::pair<int,int> result{0, 0};
  for (int i = 0; i < pid.size(); ++i) {
    pid[i] = fork();
    if (pid[i] < 0) {
      std::cerr << "fork failed" << std::endl;
      exit(-1);
    }
    else if (pid[i] == 0) {
      for (int j = 0; j < 2*clients; ++j) {
        if (j == 2*i+1) continue;
        close(pipefd[j]);
      }
      auto res = echoTest();
      ssize_t len;
      len = write(pipefd[2*i+1], &res.first, sizeof(int));
      len = write(pipefd[2*i+1], &res.second, sizeof(int));
      close(pipefd[2*i+1]);
      exit(0);
    }
  }

  // 只有主进程才能执行到这
  for (int i = 0; i < pid.size(); ++i) {
    waitpid(pid[i], NULL, 0);
    close(pipefd[2*i+1]);
    std::pair<int, int> res_from_child;
    ssize_t len;
    len = read(pipefd[2*i], &res_from_child.first, sizeof(int));
    len = read(pipefd[2*i], &res_from_child.second, sizeof(int));
    close(pipefd[2*i]);
    result.first += res_from_child.first;
    result.second += res_from_child.second;
  }

  delete []pipefd;
  std::cout << "send message:    " << result.first << std::endl;
  std::cout << "success message: " << result.second << std::endl;
}