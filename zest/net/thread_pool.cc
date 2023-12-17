/* 封装线程池 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/thread_pool.h"
#include <iostream>
#include "zest/base/logging.h"

using namespace zest;
using namespace zest::net;

ThreadPool::ThreadPool(int n): m_thread_num(n), m_index(0), m_thread_pool(n, nullptr)
{
  for (int i = 0; i < n; ++i) {
    try {
      m_thread_pool[i].reset(new IOThread());
    }
    catch(const std::runtime_error& e) {
      std::cerr << e.what() << '\n';
      break;
    }
    catch(const std::bad_alloc& e) {
      std::cerr << e.what() << '\n';
      break;
    }
  }
}

ThreadPool::~ThreadPool()
{
  /* do nothing 
   * 但是 ThreadPool 析构时，会引发 m_thread_pool 中所有 IOThread 的析构函数
   * IOThread 的析构函数就是停止工作线程的eventloop循环，并等待工作线程退出
   */
  LOG_INFO << "Thread pool exit";
}

void ThreadPool::start()
{
  LOG_INFO << "Thread pool start";
  for (int i = 0; i < m_thread_num; ++i) {
    if (m_thread_pool[i])
      m_thread_pool[i]->start();
  }
}

void ThreadPool::stop()
{
  m_thread_pool.clear();
}

// 按照轮转调度法获取io线程
IOThread::s_ptr ThreadPool::get_io_thread()
{
  // 考虑到IO线程中 EventLoop::CreateEventLoop() 可能失败，所以要判断eventloop是否在运行
  if (m_thread_pool[m_index] && m_thread_pool[m_index]->is_valid()) {
    auto rt = m_thread_pool[m_index];
    m_index = (m_index + 1) % m_thread_num;
    return rt;
  }
  
  for (int i = (m_index+1) % m_thread_num; i != m_index; i = (i+1) % m_thread_num) {
    if (m_thread_pool[i] && m_thread_pool[i]->is_valid()) {
      m_index = (i + 1) % m_thread_num;
      return m_thread_pool[i];
    }
  }

  // 不太可能执行到这里，除非每个线程的 EventLoop::CreateEventLoop() 都失败了
  LOG_FATAL << "NO IO thread can be used";
  exit(-1);
}
