/* 异步日志的核心，定义了FixedBuffer和AsyncLogging */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/base/async_logging.h"

#include <pthread.h>
#include <unistd.h>

#include <atomic>
#include <iostream>
#include <stdexcept>

#include "zest/base/util.h"

using namespace zest;


// 单例模式指针
std::shared_ptr<AsyncLogging> g_logger = nullptr;

std::shared_ptr<AsyncLogging> AsyncLogging::GetGlobalLogger() {return g_logger;}

// 初始化函数，构造AsyncLogging对象，开启后端线程
void AsyncLogging::InitAsyncLogger(const std::string &file_name, const std::string &file_path, 
                 int max_file_size, int sync_interval, int max_buffers)
{
  if (g_logger == nullptr) {
    g_logger.reset(
      new AsyncLogging(
        file_name, file_path, max_file_size, sync_interval, max_buffers)
    );

    if (pthread_create(&g_logger->m_tid, NULL, threadFunc, NULL) != 0) {
      std::cerr << "Creating logger backend thread failed" << std::endl;
      exit(-1);
    }
    g_logger->m_init_sem.wait();   // 等待 m_running 被设置为true
  }
  // else
  //   throw std::runtime_error("Set global logger more than once");
}

// 辅助调用后端线程函数
void *AsyncLogging::threadFunc(void *)
{
  g_logger->m_running = true;
  g_logger->m_init_sem.post();
  // zest::AsyncLogging *logger_ = reinterpret_cast<zest::AsyncLogging*>(logger);
  g_logger->backendThreadFunc();
  return NULL;
}

AsyncLogging::AsyncLogging(const std::string &file_name, const std::string &file_path, 
                           int max_file_size, int sync_interval, int max_buffers):
  m_file(fopen(get_logfile_name(file_name, file_path).c_str(), "a")),
  m_file_name(file_name),
  m_file_path(file_path.back() == '/' ? file_path : file_path+'/'),  // 检查file_path是不是以“/”结尾，如果不是，则加上“/”
  m_max_file_size(max_file_size),
  m_sync_interval(sync_interval),
  m_max_buffers(std::min(max_buffers, 25)),
  m_cur_file_size(0),
  m_buffers_n(0),
  m_current_itr(m_buffers.end()),
  m_next_to_flush(m_buffers.end()),
  m_mutex(),
  m_flush_mutex(),
  m_cond(m_mutex),
  m_tid(0),
  m_running(false)
{
  // 初始时有4个缓冲区
  for (int i = 0; i < 4; ++i) {
    m_buffers.emplace_back(new Buffer());
    ++m_buffers_n;
  }
  m_current_itr = m_buffers.begin();
  m_next_to_flush = m_buffers.begin();
}

AsyncLogging::~AsyncLogging()
{
  if (m_running || m_tid != 0) {
    m_running = false;
    m_cond.signal();
    pthread_join(m_tid, NULL);
  }
}

// 计算当前缓冲区和刷盘缓冲区的距离，用于实时调整缓冲区数量
inline int AsyncLogging::calcBufferDistance(BufferList::iterator next_to_flush, BufferList::iterator cur_buf)
{
  int count = 0;
  while (next_to_flush != cur_buf) {
    if (++next_to_flush == m_buffers.end())
      next_to_flush = m_buffers.begin();
    ++count;
  }
  return count;
}

// 前端调用函数
void AsyncLogging::append(const char *logline, int len)
{
  ScopeMutex mutex(m_mutex);
  if ((*m_current_itr)->enough(len)) {
    // 当前缓冲区空间足够，可直接写入
    (*m_current_itr)->append(logline, len);
  }
  else {
    // 当前缓冲区空间不够，查看下一个缓冲区
    if (++m_current_itr == m_buffers.end())
      m_current_itr = m_buffers.begin();

    // 没有空闲的缓冲区
    if (m_current_itr == m_next_to_flush) {
      // 缓冲区数目未达上限，可继续分配
      if (m_buffers_n < m_max_buffers) {
        m_current_itr = m_buffers.insert(m_current_itr, BufferPtr(new Buffer()));
        ++m_buffers_n;
      }
      // 缓冲区数目以达上限，说明短期内日志数量太多，直接丢弃
      else return;
    }

    m_cond.signal();
    (*m_current_itr)->append(logline, len);
  }
}

// 立刻刷盘（同步）
void AsyncLogging::flush()
{
  // FIXME: 只是简单地提醒后端线程刷盘，并没有真正做到同步日志
  m_cond.signal();

  // ScopeMutex mutex(m_mutex);
  // while (m_next_to_flush != m_current_itr) {
  //     fwrite((*m_next_to_flush)->data(), 1, (*m_next_to_flush)->size(), m_file);
  //     m_cur_file_size += (*m_next_to_flush)->lines();
  //     (*m_next_to_flush)->reset();
  //     if (++m_next_to_flush == m_buffers.end())
  //         m_next_to_flush = m_buffers.begin();
  // }
  // fflush(m_file);

  // // 日志文件中记录数太多，创建新的日志文件
  // if (m_cur_file_size >= m_max_file_size) {
  //     fclose(m_file);
  //     m_file = fopen(get_logfile_name(m_file_name, m_file_path).c_str(), "a");
  //     m_cur_file_size = 0;
  // }
}

// 后端线程函数
void AsyncLogging::backendThreadFunc()
{
  do
  {
    BufferList::iterator cur, next_to_flush;
    bool reduce;
    {
      ScopeMutex mutex(m_mutex);
      cur = m_current_itr;   // 查看当前是否有缓冲区等待刷盘
      if (cur == m_next_to_flush)
        m_cond.waitForMilliSeconds(m_sync_interval);

      // 快照，后续的刷盘到此为止
      cur = m_current_itr;
      next_to_flush = m_next_to_flush;
      reduce = m_buffers_n - calcBufferDistance(next_to_flush, cur) > 2;   // 是否减少缓冲区
    }
      
    // 刷盘
    while (next_to_flush != cur) {
      fwrite((*next_to_flush)->data(), 1, (*next_to_flush)->size(), m_file);
      m_cur_file_size += (*next_to_flush)->lines();
      (*next_to_flush)->reset();
      if (++next_to_flush == m_buffers.end())
        next_to_flush = m_buffers.begin();
    }

    {
      // 需要更新m_current_itr和m_next_to_flush，需要重新获取锁
      ScopeMutex mutex(m_mutex);

      // 刷盘期间由于没有锁定，所以m_current_itr可能已经变化了
      if (cur == m_current_itr) {
        if (++m_current_itr == m_buffers.end())
          m_current_itr = m_buffers.begin();
      }
      m_next_to_flush = next_to_flush;
    }
    fwrite((*next_to_flush)->data(), 1, (*next_to_flush)->size(), m_file);
    m_cur_file_size += (*next_to_flush)->lines();
    (*next_to_flush)->reset();

    {
      ScopeMutex mutex(m_mutex);
      if (reduce && m_buffers_n > 4) {
        m_next_to_flush = m_buffers.erase(m_next_to_flush);
        --m_buffers_n;
      }
      else
        ++m_next_to_flush;
      if (m_next_to_flush == m_buffers.end())
        m_next_to_flush = m_buffers.begin();
    }

    fflush(m_file);
    // 日志文件中记录数太多，创建新的日志文件
    if (m_cur_file_size >= m_max_file_size) {
      fclose(m_file);
      m_file = fopen(get_logfile_name(m_file_name, m_file_path).c_str(), "a");
      m_cur_file_size = 0;
    }

    /* 人为地制造延时，模拟IO较慢的情况，最终编译时必须注释掉，否则将影响性能！*/
    // sleep(1);
    // std::cout << m_buffers_n << std::endl;
    /***************************************************************/

  } while (m_running);

  /* 将最后剩下缓冲区刷盘，此时不再允许前端写入 */
  ScopeMutex mutex(m_mutex);
  if (++m_current_itr == m_buffers.end())
    m_current_itr = m_buffers.begin();
  while (m_next_to_flush != m_current_itr) {
    fwrite((*m_next_to_flush)->data(), 1, (*m_next_to_flush)->size(), m_file);
    // (*m_next_to_flush)->reset();
    if (++m_next_to_flush == m_buffers.end())
      m_next_to_flush = m_buffers.begin();
  }

  fflush(m_file);
  fclose(m_file);
}
