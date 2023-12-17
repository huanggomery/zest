/* 用于日志系统的缓冲区 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_BASE_FIX_BUFFER_H
#define ZEST_BASE_FIX_BUFFER_H

#include <string.h>
#include "zest/base/noncopyable.h"

namespace zest
{

const int LargeBufferSize = 4000 * 1000;    // 每个前端缓冲区的大小
const int SmallBufferSize = 4000;           // 单条日志的最大长度
const int MaxNumericSize = 48;           // 数值型的最长长度

template <int SIZE>
class FixedBuffer: public noncopyable
{
 public:
  FixedBuffer(): cur(m_data), m_lines(0){}
  ~FixedBuffer() = default;
  void bzero() {memset(m_data, 0, SIZE);}
  int avail() const {return SIZE-(cur-m_data);}
  int size() const {return cur-m_data;}
  int lines() const {return m_lines;}
  bool enough(int len) const {return avail() >= len;}
  void reset() {cur = m_data; m_lines = 0;}
  const char *data() const {return m_data;}

  void append(const char *logline, int len)
  {
    if (enough(len)) {
      memcpy(cur, logline, len);
      cur += len;
      ++m_lines;
    }
  }
    
 private:
  char m_data[SIZE];
  char *cur;    // 尾后指针
  int m_lines;  // 记录条数
};

  
} // namespace zest

#endif // ZEST_BASE_FIX_BUFFER_H
