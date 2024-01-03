// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_WAKEUP_FD_EVENT_H
#define ZEST_NET_WAKEUP_FD_EVENT_H

#include "zest/net/fd_event.h"
#include "unistd.h"
#include <atomic>
#include "zest/base/logging.h"

namespace zest
{
namespace net
{
  
class WakeUpFdEvent: public FdEvent
{
 public:
  WakeUpFdEvent() = delete;
  ~WakeUpFdEvent() = default;

  WakeUpFdEvent(int fd): FdEvent(fd)
  {
    // wakeup_event 的回调函数就是读取fd中的8字节数据
    listen(
      EPOLLIN | EPOLLET,
      [fd, this]() {
        char buf[8];
        while (read(fd, buf, 8) > 0) {/* do nothing */}
      }
    );
  }

  // 向fd中写入一个字节，用于从epoll_wait中返回
  void wakeup()
  {
    char buf[8] = {'a'};
    int rt = write(m_fd, buf, 8);
    if (rt != 8) {
      LOG_ERROR << "Write to wakeup fd " << m_fd << " failed, errno = " << errno;
    }
    else {
      // LOG_DEBUG << "Write to wakeup fd " << m_fd << " succeed";
    }
  }
};

} // namespace net
} // namespace zest

#endif // ZEST_NET_WAKEUP_FD_EVENT_H
