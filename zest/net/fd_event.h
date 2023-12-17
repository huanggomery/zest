/* epoll监听IO事件的封装 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_FD_EVENT_H
#define ZEST_NET_FD_EVENT_H

#include <sys/epoll.h>

#include <atomic>
#include <functional>
#include <memory>

#include "zest/base/noncopyable.h"

namespace zest
{
namespace net
{
  
// IO事件类的基类
class FdEvent: public noncopyable
{
 public:
  using s_ptr = std::shared_ptr<FdEvent>;
  using CallBackFunc = std::function<void()>;
  enum TriggerEvent {
    IN_EVENT = EPOLLIN,
    OUT_EVENT = EPOLLOUT,
    ERROR_EVENT = EPOLLERR
  };

 public:
  FdEvent() = delete;
  ~FdEvent() = default;
  FdEvent(int fd);

  // 为IO事件设置回调函数
  void listen(uint32_t ev_type, const CallBackFunc &cb, const CallBackFunc &err_cb = nullptr);

  // 获取IO事件的回调函数
  CallBackFunc handler(TriggerEvent type) const;

  // 获取文件描述符
  int getFd() const {return m_fd;}

  // 获取epoll_event结构体
  epoll_event getEpollEvent() const {return m_event;}

  // 将监听的fd设置为非阻塞
  void set_non_blocking();
  
 protected:
  int m_fd {-1};
  struct epoll_event m_event;
  CallBackFunc m_read_callback {nullptr};
  CallBackFunc m_write_callback {nullptr};
  CallBackFunc m_error_callback {nullptr};
};

} // namespace net
} // namespace zest


#endif // ZEST_NET_FD_EVENT_H
