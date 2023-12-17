/* 封装线程池 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_THREAD_POOL_H
#define ZEST_NET_THREAD_POOL_H

#include <vector>

#include "zest/base/noncopyable.h"
#include "zest/net/io_thread.h"

namespace zest
{
namespace net
{

class ThreadPool: public noncopyable
{
 public:
  using s_ptr = std::shared_ptr<ThreadPool>;

 public:
  ThreadPool() = delete;
  explicit ThreadPool(int n);
  ~ThreadPool();
  void start();
  void stop();

  // 按照轮转调度法获取io线程
  IOThread::s_ptr get_io_thread();
  
 private:
  int m_thread_num;   // 线程数
  int m_index;        // 下一个要使用的线程
  std::vector<IOThread::s_ptr> m_thread_pool;  // 所有线程的指针
};
  
} // namespace net 
} // namespace zest

#endif // ZEST_NET_THREAD_POOL_H
