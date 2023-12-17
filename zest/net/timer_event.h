/* 定时器 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_TIMER_EVENT_H
#define ZEST_NET_TIMER_EVENT_H

#include <functional>
#include <memory>
#include <queue>
#include <vector>

namespace zest
{
namespace net
{
  
// 单个定时器任务，记录超时时间和回调函数
class TimerEvent
{
 public:
  using s_ptr = std::shared_ptr<TimerEvent>;
  using CallBackFunc = std::function<void()>;  // 回调函数

  TimerEvent() = delete;
  TimerEvent(uint64_t interval, CallBackFunc cb, bool periodic = false);
  TimerEvent(const TimerEvent &timer_event);
  ~TimerEvent() = default;

  uint64_t getInterval() const {return m_interval;}
  uint64_t getTriggerTime() const {return m_trigger_time;}
  CallBackFunc handler() const {return m_callback;}
  bool is_periodic() const {return m_periodicity;}
  bool is_valid() const {return m_valid;}
  void set_periodic(bool value) {m_periodicity = value;}
  void set_valid(bool value) {m_valid = value;}
  void reset_time();

 private:
  uint64_t m_interval;        // 时间间隔，单位 ms
  uint64_t m_trigger_time;    // 触发时间，单位 ms
  CallBackFunc m_callback;   // 定时器回调函数
  bool m_periodicity;        // 周期性事件
  bool m_valid;              // 是否有效
};

} // namespace net
} // namespace zest

#endif // ZEST_NET_TIMER_EVENT_H
