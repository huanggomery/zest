/* epoll监听事件的封装 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include "zest/net/fd_event.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "zest/base/logging.h"
#include "zest/base/util.h"

using namespace zest;
using namespace zest::net;


FdEvent::FdEvent(int fd): m_fd(fd)
{
  memset(&m_event, 0, sizeof(m_event));
}

// 为IO事件设置回调函数
void FdEvent::listen(uint32_t ev_type, const CallBackFunc &cb, const CallBackFunc &err_cb /*=nullptr*/)
{
  m_event.events = ev_type;
  if (ev_type & EPOLLIN)
    m_read_callback = cb;
  else
    m_write_callback = cb;
  if (err_cb)
    m_error_callback = err_cb;
  m_event.data.fd = m_fd;
}

// 获取IO事件的回调函数
std::function<void()> FdEvent::handler(TriggerEvent type) const
{
  if (type == IN_EVENT)
    return m_read_callback;
  else if (type == OUT_EVENT)
    return m_write_callback;
  else if (type == ERROR_EVENT)
    return m_error_callback;
  else
    return nullptr;
}

// 将监听的fd设置为非阻塞
void FdEvent::set_non_blocking()
{
  zest::set_non_blocking(m_fd);   // 使用的是utils.h中定义的函数
}
