/* 定义一些工具函数 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_BASE_UTIL_H
#define ZEST_BASE_UTIL_H

#include <string>

namespace zest
{

extern int sig_pipefd[2];  // 用于传递信号的管道，在utils.cc中定义

// 生成日志文件名
std::string get_logfile_name(const std::string &file_name, const std::string &file_path);

// 返回当前时间的字符串,格式类似于： 20230706 21:05:57.229383
std::string get_time_str();

// 返回当前时间的ms表示
int64_t get_now_ms();

// 返回进程ID
pid_t getPid();

// 返回线程ID
pid_t getTid();

// 检测文件夹是否存在
bool folderExists(const std::string& folderPath);

// 将文件描述符设置为非阻塞
bool set_non_blocking(int fd);

// 设置信号的处理函数
bool add_signal(int sig);

} // namespace zest


#endif // ZEST_BASE_UTIL_H
