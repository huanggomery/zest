/* 用于向TCP连接中存取任意的数据类型 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_NET_TCP_CONTEXT_H
#define ZEST_NET_TCP_CONTEXT_H

#include <functional>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace zest
{
namespace net
{

struct ContextValue
{
  ContextValue(void *value, const std::string &value_type, 
                std::function<void()> &deleter)
    : m_value(value), m_value_type(value_type), m_deleter(deleter)
  {
    /* do nothing */
  }

  ~ContextValue()
  {
    if (m_value) {
      if (m_deleter) m_deleter();
      m_value = nullptr;
    }
  }

  void *m_value;
  std::string m_value_type;
  std::function<void()> m_deleter;
};

class Context
{
 public:
  Context() = default;
  ~Context() = default;

  template <typename ValueType, typename... Args>
  bool Put(const std::string &key, Args&&... args)
  {
    if (m_context_map.find(key) != m_context_map.end())
      return false;

    ValueType *p_value = new ValueType(std::forward<Args>(args)...);
    auto deleter = [p_value](){
      delete p_value;
    };
    std::string value_type = typeid(ValueType).name();
    m_context_map.insert({key, ContextValue(p_value, value_type, deleter)});

    return true;
  }

  template <typename ValueType>
  ValueType* Get() const
  {
    auto it = m_context_map.find(key);
    if (it == m_context_map.end())
      return nullptr;
    if (it->second.m_value_type != typeid(ValueType).name())
      return nullptr;
    return reinterpret_cast<ValueType*>(it->second.m_value);
  }
  
 private:
  std::unordered_map<std::string, ContextValue> m_context_map;
};

  
} // namespace net
} // namespace zest

#endif // ZEST_NET_TCP_CONTEXT_H
