/* Reactor模型 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_EVENT_LOOP_H
#define ZEST_NET_EVENT_LOOP_H

#include <pthread.h>

#include <atomic>
#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>

#include "zest/base/noncopyable.h"
#include "zest/base/sync.h"

namespace zest
{
namespace net
{

class FdEvent;
class WakeUpFdEvent;
class TimerEvent;
class TimerFdEvent;

class EventLoop: public noncopyable
{
 public:
  using s_ptr = std::shared_ptr<EventLoop>;
  using CallBackFunc = std::function<void()>;  // 回调函数
  using FdEventPtr = std::shared_ptr<FdEvent>;
  using TimerEventPtr = std::shared_ptr<TimerEvent>;
 public:
  static std::shared_ptr<EventLoop> CreateEventLoop();   // 工厂函数
  ~EventLoop();

  // 核心功能：循环监听注册在epoll_fd上的文件描述符，并处理回调函数
  void loop();

  // 停止loop循环
  void stop();

  // 判断是否在运行
  bool is_running() const {return m_is_running;}

  // 唤醒epoll_wait
  void wakeup();

  void addEpollEvent(FdEventPtr fd_event);
  void deleteEpollEvent(FdEventPtr fd_event);
  void deleteEpollEvent(int fd);

  // 添加一个定时器
  void addTimerEvent(TimerEventPtr timer_event);
    
  void runInLoop(CallBackFunc cb);

  bool isThisThread() const;   // 判断调用者是否是创建该对象的线程

  void assertInLoopThread() const;

 private:
  EventLoop();
  void addTask(CallBackFunc cb, bool wake_up = false);
  void doPendingTask();

 private:
  std::unordered_map<int, FdEventPtr> m_listen_fds;  // 所有监听的fd的集合
  pid_t m_tid {0};                        // 记录创建该对象的线程号
  int m_epoll_fd {0};                         // epoll_fd
  bool m_is_running {false};                   // 是否正在运行
  std::atomic<bool> m_stop;                   
  std::queue<CallBackFunc> m_pending_tasks;   // 等待处理的回调函数
  Mutex m_mutex;                              // 互斥锁
  int m_wakeup_fd {0};                        // wakeup_fd
  std::shared_ptr<WakeUpFdEvent> m_wakeup_event;  // 用于唤醒epoll_wait的事件
  std::shared_ptr<TimerFdEvent> m_timer;          // 管理所有定时器
};

} // namespace net
} // namespace zest

#endif // ZEST_NET_EVENT_LOOP_H
