/* 网络地址的虚基类 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_NET_BASE_ADDR_H
#define ZEST_NET_BASE_ADDR_H

#include <sys/socket.h>

#include <memory>
#include <string>

namespace zest
{
namespace net
{

// 所有网络地址类型的虚基类
class NetBaseAddress
{
 public:
  using s_ptr = std::shared_ptr<NetBaseAddress>;

 public:
  virtual ::sockaddr* sockaddr() = 0;
  virtual socklen_t socklen() const = 0;
  virtual int family() const = 0;
  virtual std::string to_string() const = 0;
  virtual bool check() const = 0;  // 验证地址合法性
  virtual s_ptr copy() const = 0;  // 从拷贝一份自身，并返回智能指针
};
  
} // namespace net
} // namespace zest

#endif // ZEST_NET_BASE_ADDR_H
