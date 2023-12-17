/* 定义一些工具函数 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/base/util.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

namespace zest
{

static pid_t g_pid = 0;
static thread_local pid_t t_tid = 0;

int sig_pipefd[2];  // 用于传递信号的管道

// 生成日志文件名
std::string get_logfile_name(const std::string &file_name, const std::string &file_path)
{
  char str_t[25] = {0};
  struct timeval tv;
  gettimeofday(&tv, NULL);
  time_t sec = tv.tv_sec;
  struct tm *ptime = localtime(&sec);
  strftime(str_t, 26, "%Y%m%d%H%M%S", ptime);

  return std::string(file_path + file_name + "_" + str_t + ".log");
}


// 返回当前时间的字符串,格式类似于： 20230706 21:05:57.229383
std::string get_time_str()
{
  char str_t[27] = {0};
  struct timeval tv;
  gettimeofday(&tv, NULL);
  time_t sec = tv.tv_sec;
  struct tm *ptime = localtime(&sec);
  strftime(str_t, 26, "%Y-%m-%d %H:%M:%S", ptime);

  char usec[8];
  snprintf(usec, sizeof(usec), ".%ld", tv.tv_usec);
  strcat(str_t, usec);

  return std::string(str_t);
}

// 返回当前时间的ms表示
int64_t get_now_ms()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// 返回进程ID
pid_t getPid()
{
  return g_pid != 0 ? g_pid : (g_pid = getpid());
}

// 返回线程ID
pid_t getTid()
{
  return t_tid != 0 ? t_tid : (t_tid = syscall(SYS_gettid));
}

// 检测文件夹是否存在
bool folderExists(const std::string& folderPath)
{
  struct stat info;

  if (stat(folderPath.c_str(), &info) != 0) {
    return false; // stat 函数失败，文件夹不存在
  }

  return S_ISDIR(info.st_mode);
}

// 将文件描述符设置为非阻塞
bool set_non_blocking(int fd)
{
  int old_opt = fcntl(fd, F_GETFL);
  if (old_opt & O_NONBLOCK)
    return true;

  if (fcntl(fd, F_SETFL, old_opt | O_NONBLOCK) == -1) {
    return false;
  }
  return true;
}

void _signal_handler(int sig)
{
  int old_errno = errno;
  send(sig_pipefd[1], &sig, 1, 0);
  errno = old_errno;
}

// 设置信号的处理函数
bool add_signal(int sig)
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));

  act.sa_flags |= SA_RESTART;  // 当信号打断系统调用时，系统调用会自动重启
  sigfillset(&act.sa_mask);    // 处理当前信号，会阻塞其它所有信号
  act.sa_handler = _signal_handler;
  if (sigaction(sig, &act, NULL) == -1) {
    return false;
  }
  return true;
}

} // namespace zest
