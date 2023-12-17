/* 定时器 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/timer_event.h"

#include "zest/base/logging.h"
#include "zest/base/util.h"

using namespace zest;
using namespace zest::net;

TimerEvent::TimerEvent(uint64_t interval, CallBackFunc cb, bool periodic /*=false*/):
  m_interval(interval), m_trigger_time(get_now_ms()+interval), 
  m_callback(cb), m_periodicity(periodic), m_valid(true)
{
  /* do nothing */
}

TimerEvent::TimerEvent(const TimerEvent &timer_event):
  TimerEvent(timer_event.m_interval, timer_event.m_callback, timer_event.m_periodicity)
{
  /* do nothing */
}

void TimerEvent::reset_time()
{
  m_trigger_time = get_now_ms() + m_interval;
}
