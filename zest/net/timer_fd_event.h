/* 用一个timerfd和一个队列管理多个定时器
 * TimerFd所维护的timerfd实际上是优先队列中第一个（就是最早一个）定时器的触发时间 
 * 每当timerfd触发，就依次检查优先队列中的定时器，执行所有到期的定时器的回调函数
 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_TIMER_FD_EVENT_H
#define ZEST_NET_TIMER_FD_EVENT_H

#include <queue>
#include <memory>

#include "zest/base/sync.h"
#include "zest/net/fd_event.h"
#include "zest/net/timer_event.h"

namespace zest
{
namespace net
{

class TimerFdEvent: public FdEvent
{
  struct CmpTimer  // 用于在优先队列中对定时器进行排序，
  {
    bool operator()(TimerEvent::s_ptr t1, TimerEvent::s_ptr t2) {
      return t1->getTriggerTime() > t2->getTriggerTime();
    }
  };
  using TimerQueue = std::priority_queue<TimerEvent::s_ptr, std::vector<TimerEvent::s_ptr>, CmpTimer>;
  
 public:
  TimerFdEvent();
  ~TimerFdEvent() = default;
  void addTimerEvent(TimerEvent::s_ptr timer);
  void deleteTimerEvent(TimerEvent::s_ptr timer);

 public:
  void handleTimerEvent();  // 处理定时任务函数
  void resetTimerFd();      // 重新设置定时器fd
  
private:
  TimerQueue m_pending_timers;   // 用优先队列管理所有定时器事件
  Mutex m_mutex;
};
  
} // namespace net
} // namespace zest

#endif // ZEST_NET_TIMER_FD_EVENT_H
