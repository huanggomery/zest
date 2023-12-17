/* 异步日志的核心，定义了FixedBuffer和AsyncLogging */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_BASE_ASYNC_LOGGING_H
#define ZEST_BASE_ASYNC_LOGGING_H

#include <string.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include <zest/base/fix_buffer.h>
#include "zest/base/noncopyable.h"
#include "zest/base/sync.h"

namespace zest
{

// 异步日志的核心类
class AsyncLogging: public noncopyable
{
  using s_ptr = std::shared_ptr<AsyncLogging>;
  using Buffer = FixedBuffer<LargeBufferSize>;
  using BufferPtr = std::unique_ptr<Buffer>;
  using BufferVector = std::vector<BufferPtr>;
  using BufferList = std::list<BufferPtr>;
 public:
  static void InitAsyncLogger(const std::string &file_name, const std::string &file_path, 
                int max_file_size, int sync_interval, int max_buffers);   // 初始化函数，构造AsyncLogging对象，开启后端线程
  static s_ptr GetGlobalLogger();
  void append(const char *logline, int len);  // 供前端调用
  void flush();     // 立刻刷盘（同步）
  ~AsyncLogging();

 private:
  static void *threadFunc(void *logger);  // 辅助调用后端线程
  void backendThreadFunc();   // 后端线程函数

  AsyncLogging(const std::string &file_name, const std::string &file_path, 
                int max_file_size, int sync_interval, int max_buffers);

  // 计算当前缓冲区和刷盘缓冲区的距离，用于实时调整缓冲区数量
  int calcBufferDistance(BufferList::iterator begin, BufferList::iterator end);

 private:
  /* 日志配置信息 */
  FILE *m_file;
  const std::string m_file_name;
  const std::string m_file_path;
  const int m_max_file_size;    // 单个日志文件中最大记录条数
  const int m_sync_interval;    // 日志刷盘间隔，单位ms
  const int m_max_buffers;      // 最多能拥有的buffer数

  int m_cur_file_size;          // 当前日志文件中记录条数

  /* 缓冲区信息 */
  BufferList m_buffers;      // 所有的缓冲区
  int m_buffers_n;           // 当前缓冲区数目
  BufferList::iterator m_current_itr;         // 当前正在写的缓冲区
  BufferList::iterator m_next_to_flush;       // 下一个需要刷盘的缓冲区

  /* 线程并发保护 */
  Mutex m_mutex;
  Mutex m_flush_mutex;  // 刷盘保护，防止flush()和后端线程一起刷盘
  Condition m_cond;
  Sem m_init_sem;    // 仅用于确保后端线程正确开启

  /* 后端线程的启停 */
  pthread_t m_tid;
  bool m_running;
};

} // namespace zest

#endif // ZEST_BASE_ASYNC_LOGGING_H
