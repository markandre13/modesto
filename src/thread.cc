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


#include <X11/Xlib.h>

#include <signal.h>
#include "thread.hh"
#include <iostream>

using namespace std;

static pthread_key_t who_am_i;
static pthread_t main_thread;

void
TThread::initialize(bool withX)
{
//  if (withX && !XInitThreads()) {
//    cerr << "XInitThreads failed, better update your Xlib" << endl;
//  }
  
  #ifdef _MIT_POSIX_THREADS
  pthread_init();
  #endif
  
  pthread_key_create(&who_am_i, NULL);
  pthread_setspecific(who_am_i, NULL);
  main_thread = pthread_self();
//  cout << "POSIX threads initialized..." << endl;
}

TThread::~TThread()
{
}

void TThread::join()
{
  void* dummy;
  pthread_join(_thread, &dummy);
}

void TThread::start()
{
  pthread_create(&_thread, NULL, _thread_entry, (void*)this);
}

void TThread::stop()
{
  pthread_cancel(_thread);
}

TThread* TThread::whoAmI()
{
  return (TThread*) pthread_getspecific(who_am_i);
}

int TThread::kill(TThread *thread, int sig)
{
  if (thread==NULL) {
    return pthread_kill(main_thread, sig);
  } else {
    return pthread_kill(thread->_thread, sig);
  }
}

void* TThread::_thread_entry(void *obj)
{
  pthread_setspecific(who_am_i, obj);
  ((TThread*)obj)->main();
  // pthread_exit(NULL);
  return NULL;
}

