/* 封装线程同步变量，互斥锁、条件变量和信号量*/

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

// This is an internal header file, you should not include this.

#ifndef ZEST_BASE_SYNC_H
#define ZEST_BASE_SYNC_H

#include <pthread.h>
#include <semaphore.h>


namespace zest
{

// 最普通的互斥锁，需要手动获取和释放
class Mutex
{
 public:
  Mutex();
  ~Mutex();
  void lock();
  void unlock();
  pthread_mutex_t *getMutex();
  
 private:
  pthread_mutex_t m_mutex;
};

// RAII机制的互斥锁
class ScopeMutex
{
 public:
  ScopeMutex() = delete;
  explicit ScopeMutex(Mutex &mutex);
  ~ScopeMutex();
  void lock();
  void unlock();

 private:
  Mutex &m_mutex;
  bool m_is_locked;
};

// 条件变量
class Condition
{
 public:
  Condition() = delete;
  explicit Condition(Mutex &mutex);
  ~Condition();
  void wait();
  void waitForMilliSeconds(double ms);
  void signal();
  void broadcast();
  
 private:
  pthread_cond_t m_cond;
  Mutex &m_mutex;
};

// POSIX信号量
class Sem
{
public:
  Sem(int n = 0);
  ~Sem();
  void wait();
  void post();

private:
  sem_t m_sem;
};

} // namespace zest

#endif // ZEST_BASE_SYNC_H
