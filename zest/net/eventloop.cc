/* Reactor模型 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/eventloop.h"

#include <assert.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>

#include "zest/base/logging.h"
#include "zest/base/util.h"
#include "zest/net/fd_event.h"
#include "zest/net/timer_event.h"
#include "zest/net/timer_fd_event.h"
#include "zest/net/wakeup_fd_event.h"

using namespace zest;
using namespace zest::net;

static thread_local std::shared_ptr<EventLoop> t_event_loop = nullptr;
static thread_local pid_t t_tid = 0;
static int g_epoll_max_timeout = 3000;   // 单位 ms
static int g_epoll_max_events = 100;

std::shared_ptr<EventLoop> EventLoop::CreateEventLoop()
{
  if (t_event_loop)
    return t_event_loop;
  t_tid = getTid();

  try {
    t_event_loop.reset(new EventLoop());
  }
  catch(const std::bad_alloc &e) {
    LOG_ERROR << "Create new EventLoop failed";
    return nullptr;
  }
  catch(const std::runtime_error &e) {
    LOG_ERROR << e.what();
    return nullptr;
  }

  return t_event_loop;    
}

EventLoop::EventLoop(): 
  m_tid(t_tid), 
  m_epoll_fd(epoll_create(10)), 
  m_is_running(false),
  m_stop(false),
  m_mutex(),
  m_wakeup_fd(eventfd(0, EFD_NONBLOCK)),
  m_timer(new TimerFdEvent()),
  m_wakeup_event(new WakeUpFdEvent(m_wakeup_fd))
{
  if (m_epoll_fd == -1) {
    LOG_ERROR << "epoll_create failed";
    throw std::runtime_error("epoll_create failed");
  }

  if (m_wakeup_fd == -1) {
    LOG_ERROR << "eventfd failed";
    throw std::runtime_error("eventfd failed");
  }
  
  addEpollEvent(m_timer);
  addEpollEvent(m_wakeup_event);
}

// 析构函数，把工作队列中剩下的事处理完
EventLoop::~EventLoop()
{
  // 处理工作队列中的回调函数
  doPendingTask();
  
  close(m_wakeup_fd);
  close(m_epoll_fd);
}

// 核心功能：循环监听注册在epoll_fd上的文件描述符，并处理回调函数
void EventLoop::loop()
{
  assert(!m_is_running);
  if (!isThisThread()) {
    LOG_FATAL << "try to start event loop BY other thread";
    exit(-1);
  }
  m_is_running = true;
  m_stop = false;   // FIXME: what if someone calls stop() before loop() ?
  LOG_DEBUG << "EventLoop start";
  struct epoll_event events[g_epoll_max_events];

  // 在进入loop前先把已经发生的事件处理了
  doPendingTask();

  while (!m_stop) {

    int n = epoll_wait(m_epoll_fd, events, g_epoll_max_events, g_epoll_max_timeout);
    if (n < 0) {
      LOG_ERROR << "epoll_wait failed, errno = " << errno;
      continue;
    }

    for (int i = 0; i < n; ++i) {
      epoll_event event = events[i];
      FdEventPtr fd_event = m_listen_fds[event.data.fd];
      if (event.events & EPOLLIN) {
        addTask(fd_event->handler(FdEvent::IN_EVENT));
      }
      if (event.events & EPOLLOUT) {
        addTask(fd_event->handler(FdEvent::OUT_EVENT));
      }
      if (event.events & EPOLLERR) {
        deleteEpollEvent(fd_event);
        addTask(fd_event->handler(FdEvent::ERROR_EVENT));
      }
    }

    doPendingTask();
  }
  LOG_DEBUG << "stop event loop";
  m_is_running = false;
}

void EventLoop::stop()
{
  m_stop = true;

  if (!isThisThread())
    wakeup();
}

void EventLoop::wakeup()
{
  m_wakeup_event->wakeup();
}

void EventLoop::addTask(CallBackFunc cb, bool wake_up /*=false*/)
{
  ScopeMutex mutex(m_mutex);
  m_pending_tasks.push(cb);
  mutex.unlock();

  if (wake_up) {
    // LOG_DEBUG << "addTask wakeup";
    wakeup();
  }
}

// 向epoll内核注册或修改事件
void EventLoop::addEpollEvent(FdEventPtr fd_event)
{
  /* 为了避免：
    *     1. 在epoll_wait期间，其它线程修改epoll内核中注册的事件
    *     2. 对 m_listen_fds 的修改引发竞态条件
    * 所以需要由同一个线程（即创建该对象的线程）来处理
    */
  if (isThisThread()) {
    int fd = fd_event->getFd();
    epoll_event tmp = fd_event->getEpollEvent();
    int op;
    if (m_listen_fds.find(fd) != m_listen_fds.end()) {
      op = EPOLL_CTL_MOD;
      m_listen_fds[fd] = fd_event;
    }
    else {
      op = EPOLL_CTL_ADD;
      m_listen_fds.insert({fd, fd_event});
    }
    if (epoll_ctl(m_epoll_fd, op, fd, &tmp) == -1) {
      m_listen_fds.erase(fd);
      LOG_ERROR << "epoll_ctl failed";
    }
  }
  else {
    // 其它线程试图修改epoll内核注册的事件
    // 将本函数添加进工作队列中，并用wakeup唤醒epoll_wait，起到“延迟修改”的作用
    auto cb = [this, fd_event](){this->addEpollEvent(fd_event);};
    addTask(cb, true);
  }
}

// 在epoll内核中删除事件
void EventLoop::deleteEpollEvent(int fd)
{
  // 原因同 void EventLoop::addEpollEvent(FdEventPtr fd_event)
  if (isThisThread()) {
    if (m_listen_fds.find(fd) != m_listen_fds.end()) {
      m_listen_fds.erase(fd);
      epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    }
  }
  else {
    auto cb = [this, fd](){this->deleteEpollEvent(fd);};
    addTask(cb, true);
  }
}

void EventLoop::deleteEpollEvent(FdEventPtr fd_event)
{
  deleteEpollEvent(fd_event->getFd());
}

// 添加一个定时器
void EventLoop::addTimerEvent(TimerEventPtr t_event)
{
  m_timer->addTimerEvent(t_event);
}

void EventLoop::runInLoop(CallBackFunc cb)
{
  if (isThisThread()) {
    if (cb) cb();
  }
  else {
    addTask(cb, true);
  }
}

bool EventLoop::isThisThread() const
{
  return m_tid == t_tid;
}

void EventLoop::doPendingTask()
{
  std::queue<CallBackFunc> pending_tasks;
  ScopeMutex mutex(m_mutex);
  pending_tasks.swap(m_pending_tasks);
  mutex.unlock();
  
  while (!pending_tasks.empty()) {
    auto cb = pending_tasks.front();
    pending_tasks.pop();
    if (cb) cb();
  }
}
