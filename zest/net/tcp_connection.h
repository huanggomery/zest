/* 封装TCP连接，需要有读写操作、业务处理以及修改epoll监听事件 */

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is a public header file, it must only include public header files.

#ifndef ZEST_NET_TCP_CONNECTION_H
#define ZEST_NET_TCP_CONNECTION_H

#include <functional>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "zest/base/noncopyable.h"
#include "zest/net/base_addr.h"
#include "zest/net/inet_addr.h"

namespace zest
{
namespace net
{

class EventLoop;
class FdEvent;
class TcpBuffer;
class TcpConnection;
class TimerEvent;

enum TcpState {
  NotConnected = 1,
  Connected = 2,
  HalfClosing = 3,
  Closed = 4,
  Failed = 5,
};


class TcpConnection : public noncopyable
{
 private:
  using ConnectionCallbackFunc = std::function<void(TcpConnection&)>;
  using EventLoopPtr = std::shared_ptr<EventLoop>;
  using NetAddrPtr = std::shared_ptr<NetBaseAddress>;
  using FdEventPtr = std::shared_ptr<FdEvent>;
  using BufferPtr = std::shared_ptr<TcpBuffer>;
  using TimerPtr = std::shared_ptr<TimerEvent>;

  /*********************** 定义两个内嵌类 ***********************/
  struct ContextValue
  {
    ContextValue(void *value, const std::string &value_type, std::function<void()> deleter)
      : m_value(value), m_value_type(value_type), m_deleter(deleter) { /* do nothing */}
    
    // 定义了移动拷贝构造函数，因此拷贝构造函数是删除的
    ContextValue(ContextValue &&tmp_value) noexcept
      : m_value(tmp_value.m_value), m_value_type(tmp_value.m_value_type), m_deleter(tmp_value.m_deleter)
    {
      tmp_value.m_value = nullptr;
      tmp_value.m_deleter = nullptr;
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
    ValueType* Get(const std::string &key) const
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
  /*************************************************************/

 public:
  using s_ptr = std::shared_ptr<TcpConnection>;

  TcpConnection(int fd, EventLoopPtr eventloop, NetAddrPtr peer_addr);
  ~TcpConnection();

  void waitForMessage();              // 等待数据到达
  std::string data() const;           // 获取接收缓存中的数据
  std::size_t dataSize() const;       // 接收缓存中数据量
  void send(const std::string &str);  // 发送数据
  void send(const char *str);
  void send(const char *str, std::size_t len);
  void clearData();                // 清空接收缓存
  void clearBytesData(int bytes);  // 丢弃接收缓存中bytes个字节的数据
  void shutdown();                 // 半关闭
  void close();                    // 断开连接

  int socketfd() const {return m_sockfd;}

  NetBaseAddress &peerAddress() const {return *m_peer_addr;}

  void setState(TcpState s) {m_state = s;}

  TcpState getState() const {return m_state;}

  void setMessageCallback(const ConnectionCallbackFunc &cb)
  { m_message_callback = cb;}

  void setWriteCompleteCallback(const ConnectionCallbackFunc &cb)
  { m_write_complete_callback = cb;}

  void setCloseCallback(const ConnectionCallbackFunc &cb)
  { m_close_callback = cb;}

  template <typename ValueType, typename... Args>
  bool Put(const std::string &key, Args&&... args);

  template <typename ValueType>
  ValueType* Get(const std::string &key) const;

  void addTimer(const std::string &timer_name, uint64_t interval, 
                ConnectionCallbackFunc cb, bool periodic = false);

  void resetTimer(const std::string &timer_name);

  void resetTimer(const std::string &timer_name, uint64_t interval);

  void cancelTimer(const std::string &timer_name);

  void deleteFromEventLoop();
  
 private:
  void handleRead();
  void handleWrite();
  
 private:
  int m_sockfd;
  EventLoopPtr m_eventloop;
  NetAddrPtr m_peer_addr;

  BufferPtr m_in_buffer;
  BufferPtr m_out_buffer;
  TcpState m_state;
  FdEventPtr m_fd_event;
  Context m_context;
  std::unordered_map<std::string, TimerPtr> m_timer_map;

  ConnectionCallbackFunc m_message_callback {nullptr};
  ConnectionCallbackFunc m_write_complete_callback {nullptr};
  ConnectionCallbackFunc m_close_callback {nullptr};
};

/* 向TCP连接中放置对象
 * 参数: 
 * 1. 该对象的名称
 * 2. 构造该对象需要的数据 */
template <typename ValueType, typename... Args>
bool TcpConnection::Put(const std::string &key, Args&&... args)
{
  return m_context.Put<ValueType>(key, std::forward<Args>(args)...);
}

// 根据对象名称获取对象指针
template <typename ValueType>
ValueType* TcpConnection::Get(const std::string &key) const
{
  return m_context.Get<ValueType>(key);
}
  
} // namespace net 
} // namespace zest

#endif // ZEST_NET_TCP_CONNECTION_H
