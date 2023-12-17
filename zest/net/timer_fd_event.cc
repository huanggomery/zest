// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/timer_fd_event.h"

#include <errno.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "zest/base/logging.h"
#include "zest/base/util.h"

using namespace zest;
using namespace zest::net;


TimerFdEvent::TimerFdEvent(): 
  FdEvent(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC))
{
  if (m_fd == -1) {
    LOG_ERROR << "timerfd_create failed, errno = " << errno;
    exit(-1);
  }

  listen(EPOLLIN | EPOLLET, std::bind(&TimerFdEvent::handleTimerEvent, this));
}

void TimerFdEvent::addTimerEvent(TimerEvent::s_ptr timer)
{
  /* TimerFd所维护的timerfd实际上是优先队列中第一个（就是最早一个）定时器的触发时间 
   * 每当timerfd触发，就依次检查优先队列中的定时器，执行所有到期的定时器的回调函数
   * 如果新加入的定时器的触发时间早于timerfd设定的时间，就要resetTimerFd 
   */
  bool is_reset = false;

  ScopeMutex mutex(m_mutex);
  // 队列为空，要重置fd的定时时间
  if (m_pending_timers.empty())
    is_reset = true;
  // 新的定时器的定时时间小于当前fd，也要重置fd的定时时间
  else if (m_pending_timers.top()->getTriggerTime() > timer->getTriggerTime())
    is_reset = true;
  // 其余情况不需要
  else
    is_reset = false;
  m_pending_timers.push(timer);
  mutex.unlock();

  if (is_reset) resetTimerFd();
}

void TimerFdEvent::deleteTimerEvent(TimerEvent::s_ptr timer)
{
  // 标记删除即可
  timer->set_valid(false);
}

// 处理定时任务函数
void TimerFdEvent::handleTimerEvent()
{
  // 先把fd中的数据读出来，防止该fd下次无法触发（ET模式）
  char buf[8];
  while ((read(m_fd, buf, sizeof(buf))) != -1) {/* do nothing */}
  
  std::vector<TimerEvent::s_ptr> trigger_timers;

  /* 临界区,将所有到期的定时器拿出来，并从队列中删除 */
  ScopeMutex mutex(m_mutex);
  int64_t now = get_now_ms();
  while (!m_pending_timers.empty() && m_pending_timers.top()->getTriggerTime() <= now) {
    TimerEvent::s_ptr timer = m_pending_timers.top();
    m_pending_timers.pop();
    if (timer->is_valid()) {
      trigger_timers.push_back(timer);
    }
  }
  mutex.unlock();
  /******************************************/

  /* 将周期性的定时器再次加入队列 */
  for (auto timer : trigger_timers) {
    if (timer->is_periodic()) {
      timer->reset_time();
      addTimerEvent(timer);
    }
  }
  resetTimerFd();

  /* 执行到期定时器的回调函数 */
  for (auto timer : trigger_timers) {
    auto cb = timer->handler();
    if (cb) cb();
  }
}

// 重新设置定时器fd
void TimerFdEvent::resetTimerFd()
{
  // 取出队列中的第一个定时器
  ScopeMutex mutex(m_mutex);
  if (m_pending_timers.empty()) return;
  TimerEvent::s_ptr first_timer = m_pending_timers.top();
  mutex.unlock();

  int64_t now = get_now_ms();
  int64_t new_trigger_time = first_timer->getTriggerTime();
  /* 第一个定时器已经到期的情况
    * 应当较少出现，因为一旦到期会通知fd，进而通过回调函数处理 */
  if (new_trigger_time <= now) {
    handleTimerEvent();
  }
  // 大多数情况
  else {
    int64_t interval = new_trigger_time - now;
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    ts.tv_sec = interval / 1000;
    ts.tv_nsec = (interval % 1000) * 1000000;

    struct itimerspec new_time;
    memset(&new_time, 0, sizeof(new_time));
    new_time.it_value = ts;

    int rt = timerfd_settime(m_fd, 0, &new_time, NULL);
    if (rt != 0) {
      LOG_ERROR << "timerfd_settime failed, errno = " << errno;
    }
  }
}
