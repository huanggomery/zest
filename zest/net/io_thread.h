/* 封装IO线程 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_IO_THREAD_H
#define ZEST_NET_IO_THREAD_H

#include <pthread.h>

#include <memory>
#include <vector>

#include "zest/base/noncopyable.h"
#include "zest/base/sync.h"
#include "zest/net/eventloop.h"

namespace zest
{
namespace net
{
  
class IOThread: public noncopyable
{
 public:
  using s_ptr = std::shared_ptr<IOThread>;
 public:
  IOThread();
  ~IOThread();
  void start() {m_start_sem.post();}
  void stop() {m_eventloop->stop();}
  EventLoop::s_ptr get_eventloop() {return m_eventloop;}
  pid_t get_tid() const {return m_tid;}
  bool is_valid() const {return m_is_valid;}

 private:
  // IO线程函数
  static void* ThreadFunc(void *arg);

 private:
  EventLoop::s_ptr m_eventloop {nullptr};   // 每个IO线程拥有一个eventloop
  pthread_t m_thread {0};     // 用于创建和等待IO线程
  pid_t m_tid {0};            // 线程ID
  Sem m_init_sem {0};         // 用于IO线程创建时的同步
  Sem m_start_sem {0};        // 用于启动eventloop
  bool m_is_valid {false};    // 标记是否正在运行
};
    
} // namespace net
} // namespace zest

#endif // ZEST_NET_IO_THREAD_H
