/* 异步日志的对外接口 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_BASE_LOGGING_H
#define ZEST_BASE_LOGGING_H

#include <atomic>
#include <string>

#include <zest/base/fix_buffer.h>
#include "zest/base/noncopyable.h"


namespace zest
{

class Logger: public noncopyable
{
  using self = Logger;
  using Buffer = FixedBuffer<SmallBufferSize>;
 public:
  enum LogLevel
  {
    DEBUG = 0,
    INFO,
    ERROR,
    FATAL,
    SYNC,
    NUM_LOG_LEVELS
  };

  // 供用户代码调用，初始化日志级别和AsyncLogger的配置，启动后端线程
  // 日志级别["DEBUG","INFO","ERROR","FATAL"]，日志文件名，日志路径，单个文件最大记录数，刷盘间隔，最大缓冲区数目
  static void InitGlobalLogger(
    const std::string &loglevel, 
    const std::string &file_name, 
    const std::string &file_path, 
    int max_file_size = 5000000, 
    int sync_interval = 1000, 
    int max_buffers_num = 25
  );

  Logger() = delete;
  Logger(const std::string &basename, int line, LogLevel level);
  ~Logger();

 public:
  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  self& operator<<(float v);
  self& operator<<(double v);

  self& operator<<(char v)
  {
    m_buffer.append(&v, 1);
    return *this;
  }

  self& operator<<(const char *str)
  {
    if (str)
      m_buffer.append(str, strlen(str));
    else
      m_buffer.append("(null)", 6);
    return *this;
  }

  self& operator<<(const std::string &v)
  {
    m_buffer.append(v.c_str(), v.size());
    return *this;
  }
    
 private:
  void append(const char *data, int len) {m_buffer.append(data, len);}
  Buffer m_buffer;
  LogLevel m_level;
};

extern std::atomic<bool> g_init;
extern Logger::LogLevel g_level;

#define LOG_DEBUG if (zest::g_init && zest::g_level <= zest::Logger::DEBUG) \
  zest::Logger(__FILE__, __LINE__, zest::Logger::DEBUG)
#define LOG_INFO if (zest::g_init && zest::g_level <= zest::Logger::INFO) \
  zest::Logger(__FILE__, __LINE__, zest::Logger::INFO)
#define LOG_ERROR if (zest::g_init && zest::g_level <= zest::Logger::ERROR) \
  zest::Logger(__FILE__, __LINE__, zest::Logger::ERROR)
#define LOG_FATAL if (zest::g_init && zest::g_level <= zest::Logger::FATAL) \
  zest::Logger(__FILE__, __LINE__, zest::Logger::FATAL)

// TODO: 目前这个还不能用
#define LOG_SYNC if (zest::g_init) \
  zest::Logger(__FILE__, __LINE__, zest::Logger::SYNC)
    
} // namespace zest

#endif // ZEST_BASE_LOGGING_H
