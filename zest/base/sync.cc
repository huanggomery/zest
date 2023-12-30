/* 封装线程同步变量，互斥锁、条件变量和信号量*/

// Copyright 2023, Huanggomery. All rights reserved.
// Author: Huanggomery (huanggomery@gmail.com)

#include <zest/base/sync.h>
#include <time.h>

using namespace zest;

// Mutex
Mutex::Mutex() {pthread_mutex_init(&m_mutex, NULL);}
Mutex::~Mutex() {pthread_mutex_destroy(&m_mutex);}
void Mutex::lock() {pthread_mutex_lock(&m_mutex);}
void Mutex::unlock() {pthread_mutex_unlock(&m_mutex);}
pthread_mutex_t *Mutex::getMutex() {return &m_mutex;}

// ScopeMutex
ScopeMutex::ScopeMutex(Mutex &mutex): m_mutex(mutex), m_is_locked(false)
{
  m_mutex.lock();
  m_is_locked = true;
}

ScopeMutex::~ScopeMutex()
{
  if (m_is_locked) {
    m_is_locked = false;
    m_mutex.unlock();
  }
}

void ScopeMutex::lock()
{
  if (!m_is_locked) {
    m_mutex.lock();
    m_is_locked = true;
  }
}

void ScopeMutex::unlock()
{
  if (m_is_locked) {
    m_is_locked = false;
    m_mutex.unlock();
  }
}


// Condition
Condition::Condition(Mutex &mutex):m_mutex(mutex) {pthread_cond_init(&m_cond, NULL);}
Condition::~Condition() {pthread_cond_destroy(&m_cond);}
void Condition::wait() {pthread_cond_wait(&m_cond, m_mutex.getMutex());}
void Condition::signal() {pthread_cond_signal(&m_cond);}
void Condition::broadcast() {pthread_cond_broadcast(&m_cond);}

void Condition::waitForMilliSeconds(double ms)
{
  struct timespec abstime;
  clock_gettime(CLOCK_REALTIME, &abstime);
  double seconds = ms / 1000;
  const int64_t kNanoSecondsPerSecond = 1000000000;
  int64_t nanoseconds = static_cast<int64_t>(seconds * kNanoSecondsPerSecond);
  abstime.tv_sec += static_cast<time_t>((abstime.tv_nsec + nanoseconds) / kNanoSecondsPerSecond);
  abstime.tv_nsec = static_cast<long>((abstime.tv_nsec + nanoseconds) % kNanoSecondsPerSecond);
  pthread_cond_timedwait(&m_cond, m_mutex.getMutex(), &abstime);
}


// Sem
Sem::Sem(int n) {sem_init(&m_sem, 0, n);}
Sem::~Sem() {sem_destroy(&m_sem);}
void Sem::wait() {sem_wait(&m_sem);}
void Sem::post() {sem_post(&m_sem);}
