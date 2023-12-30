/* 定时器容器，把定时器存储成key-value的形式 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_TIMER_CONTAINER_H
#define ZEST_NET_TIMER_CONTAINER_H

#include <functional>
#include <map>
#include <memory>

#include "zest/base/noncopyable.h"
#include "zest/net/eventloop.h"
#include "zest/net/timer_event.h"

namespace zest
{
namespace net
{

template <typename KeyType>
class TimerContainer: public noncopyable
{
 public:
  TimerContainer(EventLoop::s_ptr eventloop);
  ~TimerContainer() = default;
 
  void addTimer(const KeyType &key, uint64_t interval,
                std::function<void()> cb, bool periodic = false);
  void resetTimer(const KeyType &key);
  void resetTimer(const KeyType &key, uint64_t interval);
  void cancelTimer(const KeyType &key);
  void clearTimer();

 private:
  EventLoop::s_ptr m_eventloop;
  std::map<KeyType, TimerEvent::s_ptr> m_timer_map;
};


/************************* 以下是 TimerContainer 的实现 *******************************/

template <typename KeyType>
TimerContainer<KeyType>::TimerContainer(EventLoop::s_ptr eventloop): m_eventloop(eventloop)
{
  /* do nonthig */
}

template <typename KeyType>
void TimerContainer<KeyType>::addTimer(const KeyType &key, uint64_t interval,
                                       std::function<void()> cb, bool periodic /*=false*/)
{
  if (m_eventloop->isThisThread()) {
    auto it = m_timer_map.find(key);
    if (it != m_timer_map.end() && it->second->is_valid())
      return;
    std::shared_ptr<TimerEvent> timer_event = std::make_shared<TimerEvent>(
      interval,
      [this, key, cb](){
        // 如果定时器不是周期性的，在触发后，从容器中删除节约内存
        if (this->m_timer_map[key]->is_periodic() == false)
          this->m_timer_map.erase(key);
        cb();
      },
      periodic
    );
    m_timer_map.insert({key, timer_event});
    m_eventloop->addTimerEvent(timer_event);
  }
  else {
    m_eventloop->runInLoop(std::bind(&TimerContainer::addTimer, this, key, interval, cb, periodic));
  }
}

template <typename KeyType>
void TimerContainer<KeyType>::resetTimer(const KeyType &key)
{
  if (m_eventloop->isThisThread()) {
    auto old_timer = m_timer_map.find(key);
    if (old_timer == m_timer_map.end())
      return;
    old_timer->second->set_valid(false);  // 将原先的定时器取消

    // 创建新的定时器
    auto new_timer = std::make_shared<TimerEvent>(*(old_timer->second));
    m_timer_map[key] = new_timer;
    m_eventloop->addTimerEvent(new_timer);
  }
  else {
    auto cb = [this, key](){this->resetTimer(key);};
    m_eventloop->runInLoop(cb);
  }
}

template <typename KeyType>
void TimerContainer<KeyType>::resetTimer(const KeyType &key, uint64_t interval)
{
  if (m_eventloop->isThisThread()) {
    auto old_timer = m_timer_map.find(key);
    if (old_timer == m_timer_map.end())
      return;
    old_timer->second->set_valid(false);  // 将原先的定时器取消

    // 创建新的定时器
    auto new_timer = std::make_shared<TimerEvent>(
      interval, old_timer->second->handler(), old_timer->second->is_periodic()
    );
    m_timer_map[key] = new_timer;
    m_eventloop->addTimerEvent(new_timer);
  }
  else {
    auto cb = [this, key, interval](){this->resetTimer(key, interval);};
    m_eventloop->runInLoop(cb);
  }
}

template <typename KeyType>
void TimerContainer<KeyType>::cancelTimer(const KeyType &key)
{
  if (m_eventloop->isThisThread()) {
    auto timer = m_timer_map.find(key);
    if (timer == m_timer_map.end())
      return;
    timer->second->set_valid(false);
    m_timer_map.erase(timer);
  }
  else {
    m_eventloop->runInLoop(std::bind(&TimerContainer::cancelTimer, this, key));
  }
}

template <typename KeyType>
void TimerContainer<KeyType>::clearTimer()
{
  if (m_eventloop->isThisThread()) {
    auto it = m_timer_map.begin();
    while (it != m_timer_map.end()) {
      it->second->set_valid(false);
      it = m_timer_map.erase(it);
    }
  }
  else {
    m_eventloop->runInLoop(std::bind(&TimerContainer::clearTimer, this));
  }
}

} // namespace net
} // namespace zest

#endif // ZEST_NET_TIMER_CONTAINER_H
