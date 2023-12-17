/* 封装IO线程 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/io_thread.h"

#include <stdexcept>

#include "zest/base/logging.h"
#include "zest/base/util.h"

using namespace zest;
using namespace zest::net;


IOThread::IOThread()
{
  int rt = pthread_create(&m_thread, NULL, IOThread::ThreadFunc, this);
  if (rt < 0) {
    LOG_ERROR << "Create IO thread failed";
    throw std::runtime_error("Create IO thread failed");
  }
  // 等待IO线程完成一些初始化操作
  m_init_sem.wait();
}

IOThread::~IOThread()
{
  stop();
  pthread_join(m_thread, NULL);
}

// IO线程函数
void* IOThread::ThreadFunc(void *arg)
{
  IOThread *thread = static_cast<IOThread*>(arg);
  thread->m_eventloop = EventLoop::CreateEventLoop();
  thread->m_tid = getTid();
  if (!thread->m_eventloop) {
    LOG_ERROR << "IO thread get null eventloop ptr";
    thread->m_init_sem.post();
    return nullptr;
  }
  thread->m_is_valid = true;

  // IO线程初始化工作完成，唤醒主线程的构造函数
  thread->m_init_sem.post();

  // 等待主线程中调用 start() 启动eventloop循环
  thread->m_start_sem.wait();

  thread->m_eventloop->loop();
  thread->m_is_valid = false;

  LOG_INFO << "IO thread exit normally";
  return nullptr;
}
