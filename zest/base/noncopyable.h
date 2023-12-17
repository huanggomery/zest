/* 禁止拷贝的基类 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_BASE_NONCOPYABLE_H
#define ZEST_BASE_NONCOPYABLE_H

namespace zest
{
    
class noncopyable
{
 public:
  // 拷贝赋值和拷贝构造都是删除的，因此子类也不能拷贝赋值和拷贝构造
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;

 protected:
  // 默认构造函数和析构函数都是protected，所以子类可以调用，但类外不能调用
  noncopyable() = default;
  ~noncopyable() = default;
};

} // namespace zest

#endif // ZEST_BASE_NONCOPYABLE_H
