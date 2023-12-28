/* 异步日志的对外接口 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/base/logging.h"

#include <sys/stat.h>

#include <algorithm>
#include <iostream>
#include <unordered_map>

#include "zest/base/async_logging.h"
#include "zest/base/util.h"

namespace
{
std::unordered_map<std::string, zest::Logger::LogLevel> str2loglevel{
  {"DEBUG", zest::Logger::DEBUG},
  {"INFO", zest::Logger::INFO},
  {"ERROR", zest::Logger::ERROR},
  {"FATAL", zest::Logger::FATAL},
};

std::string loglevel2str[] = {
  "DEBUG",
  "INFO",
  "ERROR",
  "FATAL",
  "SYNC"
};


const char digits[] = "9876543210123456789";
const char *zero = digits + 9;
const char digitsHex[] = "0123456789abcdef";

// 辅助整型变量写入到字符串中
template <typename Integer>
void formatInteger(zest::FixedBuffer<zest::SmallBufferSize> &buf, Integer value)
{
  if (buf.avail() < zest::MaxNumericSize)
    return;
  char ibuf[zest::MaxNumericSize];
  char *tmp = ibuf;
  Integer i = value;
  do
  {
    int lsd = i % 10;
    *tmp++ = zero[lsd];
    i /= 10;
  } while (i != 0);
  
  if (value < 0)
    *tmp++ = '-';
  *tmp = '\0';
  std::reverse(ibuf,tmp);
  buf.append(ibuf, tmp-ibuf);
}

// 将整型变成16进制写入字符串
template <typename Integer>
void formatIntegerHex(zest::FixedBuffer<zest::SmallBufferSize> &buf, Integer value)
{
  if (buf.avail() < zest::MaxNumericSize)
    return;
  char ibuf[zest::MaxNumericSize];
  char *tmp = ibuf;
  Integer i = value;
  do
  {
    int lsd = i % 16;
    *tmp++ = digitsHex[lsd];
    i /= 16;
  } while (i != 0);

  *tmp = '\0';
  std::reverse(ibuf, tmp);
  buf.append(ibuf, tmp-ibuf);
}

} // namespace

namespace zest
{
// 全局的日志级别
Logger::LogLevel g_level;

// 日志系统是否初始化
std::atomic<bool> g_init(false);

// 供用户代码调用，初始化日志级别和AsyncLogger的配置，启动后端线程
void Logger::InitGlobalLogger(
  const std::string &loglevel, 
  const std::string &file_name, 
  const std::string &file_path, 
  int max_file_size, 
  int sync_interval, 
  int max_buffers_num)
{
  g_level = str2loglevel[loglevel];
  // 检查日志文件夹是否存在，不存在的话新建文件夹
  if (!folderExists(file_path)) {
    if (mkdir(file_path.c_str(), 0775) != 0) {
      std::cerr << "Create log file folder failed" << std::endl;
      exit(-1);
    }
  }
  AsyncLogging::InitAsyncLogger(file_name, file_path, max_file_size, sync_interval, max_buffers_num);
  g_init = true;
}

Logger::Logger(const std::string &basename, int line, LogLevel level) : m_level(level)
{
  (*this) << loglevel2str[level] <<'\t' << get_time_str() << '\t'
          << getPid() << ':' << getTid() << '\t'
          << basename << ':' << line << '\t';
}

Logger::~Logger()
{
  (*this) << '\n';
  AsyncLogging::GetGlobalLogger()->append(m_buffer.data(), m_buffer.size());

  if (m_level == SYNC) {
    AsyncLogging::GetGlobalLogger()->flush();
  }
}


/**************** 以下部分都是重载<<操作符 ****************/
Logger &Logger::operator<<(short v)
{
  (*this) << static_cast<int>(v);
  return *this;
}

Logger &Logger::operator<<(unsigned short v)
{
  (*this) << static_cast<unsigned int>(v);
  return *this;
}

Logger &Logger::operator<<(int v)
{
  formatInteger(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(unsigned int v)
{
  formatInteger(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(long v)
{
  formatInteger(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(unsigned long v)
{
  formatInteger(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(long long v)
{
  formatInteger(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(unsigned long long v)
{
  formatInteger(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(const void *p)
{
  uintptr_t v = reinterpret_cast<uintptr_t>(p);
  m_buffer.append("0x", 2);
  formatIntegerHex(m_buffer, v);
  return *this;
}

Logger &Logger::operator<<(float v)
{
  (*this) << static_cast<double>(v);
  return *this;
}

Logger &Logger::operator<<(double v)
{
  if (m_buffer.avail() > MaxNumericSize)
  {
    char buf[MaxNumericSize] = {0};
    snprintf(buf, MaxNumericSize, "%.12g", v);
    m_buffer.append(buf, strlen(buf));
  }
  return *this;
}

/******************************************************/

} // namespace zest
