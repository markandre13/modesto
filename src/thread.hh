/*
 *  Modesto -  A non-linear video-, audio- and midi editor
 *  Copyright (C) 2002, 2003 Mark-André Hopf <mhopf@mark13.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/**
 * \class TThread
 * Note: The TThread class and the other classes assiciated with it
 * do not provide access to the complete POSIX Thread API. They just
 * provide what was needed during a certain time of development.
 * Enhancements and patches are welcome.
 */

#ifndef TThread
#define TThread TThread

#include <pthread.h>

class TThread
{
  public:
    pthread_t _thread;
    static void* _thread_entry(void*);

  public:
    static void initialize(bool withX=true);
  
    virtual ~TThread();
    void start();
    void stop();
    void join();
    static TThread* whoAmI();
    static int kill(TThread*, int);
    
  protected:
    virtual void main() = 0;
};

class TThreadMutex
{
    friend class TThreadCondition;
  protected:
    pthread_mutex_t mutex;
  public:
    TThreadMutex() {
      pthread_mutex_init(&mutex, NULL);
    }
    void lock() {
      pthread_mutex_lock(&mutex);
    }
    bool trylock() {
      return pthread_mutex_trylock(&mutex) ? false : true;
    }
    void unlock() {
      pthread_mutex_unlock(&mutex);
    }
};

class TThreadLock
{
    TThreadMutex &mutex;
  public:
    TThreadLock(TThreadMutex &m):mutex(m) {
      mutex.lock();
    }
    ~TThreadLock() {
      mutex.unlock();
    }
};

class TThreadCondition
{
    pthread_cond_t condition;
  public:
    TThreadCondition() {
      pthread_cond_init(&condition, NULL);
    }
    ~TThreadCondition() {
      pthread_cond_destroy(&condition);
    }
    //! wake one of the waiting threads
    void signal() {
      pthread_cond_signal(&condition);
    }
    //! wake all waiting threads
    void broadcast() {
      pthread_cond_broadcast(&condition);
    }
    // wait for a signal or broadcast
    void wait(TThreadMutex& mutex) {
      pthread_cond_wait(&condition, &mutex.mutex);
    }
};

class TThreadLoop:
  protected TThread
{
  protected:
    bool running;
  public:
    TThreadLoop() {
      running = false;
    }
    void start() {
      if (running)
        return;
      running = true;
      TThread::start();
    }
    void stop() {
      if (!running)
        return;
      running = false;
      join();
    }
};

#endif
