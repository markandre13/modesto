/*
 *  Modesto -  A non-linear video-, audio- and midi editor
 *  Copyright (C) 2002-2004 Mark-André Hopf <mhopf@mark13.org>
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
 * \class TMediaEventMotor
 *
 * This class is responsible to handle all media related events. To do
 * it creates a thread which is running during the whole lifetime of
 * Modesto.
 *
 * One can add plug-ins to the event motor, ie.
 * \li play arrangement
 * \li record
 * \li read MIDI event, modify it and put it back again
 *   (this is the first one i'm going to implement)
 * \li ???
 *
 * One an TMediaEventMotor object is created, it will run its background
 * thread. The public methods of this class are intended to control that
 * thread from the main thread (the one with the GUI).
 */

#include "mediaeventmotor.hh"
#include "plugin.hh"
#include "modesto.hh"

#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

TMediaEventMotor::TMediaEventMotor()
{
  if (pipe(selfpipe)!=0) {
    perror("TMediaEventMotor: pipe command failed...\n");
  }
  fcntl(selfpipe[0], F_SETFL, O_NONBLOCK);
  
  cerr << "media event motor: start\n";
  newstate = STATE_STOP;
  arrangement = 0;
}

TMediaEventMotor::~TMediaEventMotor()
{
  cerr << "media event motor: stop\n";
  running = false;
  awake();
  cerr << "media event motor: join\n";
  join();
  cerr << "media event motor: destroy\n";
  
  close(selfpipe[0]);
  close(selfpipe[1]);
}

void
TMediaEventMotor::addPlugIn(TPlugIn *p) {
cerr << "add plugin " << p << " to motor " << this << endl;
  p->arrangement = arrangement;
  plugin.push_back(p);
}

void
TMediaEventMotor::setArrangement(TArrangement *a)
{
cerr << "***" << endl;
  arrangement = a;
  for (TPlugInVector::iterator p = plugin.begin();
       p != plugin.end();
       ++p)
  {
cerr << "set plugin " << *p << " with new arrangement "<<a<<" in motor " << this << endl;
    (*p)->arrangement = a;
  }
}

void
TMediaEventMotor::awake()
{
  char a;
  write(selfpipe[1], &a, 1);
}

/**
 * Return the current time in microseconds.
 */
miditime_t
miditime_current()
{
  timeval current;
  gettimeofday(&current, 0);
  return static_cast<uint64_t>(current.tv_sec) * 1000000UL +
          static_cast<uint64_t>(current.tv_usec);
}

void
TMediaEventMotor::main()
{
  cerr << "media event motor: entered main\n";

  struct sched_param sched_param;
  pthread_attr_t attr;
  int policy;
        
  if (sched_getparam(0, &sched_param) < 0) {
    printf("Scheduler getparam failed...\n");
  } else {
    sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
    if (pthread_setschedparam(_thread, SCHED_RR, &sched_param)==0) {
      perror("pthread_setschedparam");
    }
  }
  
//  setuid(getuid());
//  setgid(getgid());

  miditime_t start_time, now;

  int maxfd;
  fd_set rd0, wr0, ex0;
  
  FD_ZERO(&rd0); FD_ZERO(&wr0); FD_ZERO(&ex0);
  FD_SET(selfpipe[0], &rd0);
  maxfd = selfpipe[0];

  for(TPlugInVector::iterator p=plugin.begin();
      p!=plugin.end();
      ++p)
  {
    cerr << "register FDs for plugin " << *p << endl;
    (*p)->registerFDs(&maxfd, &rd0, &wr0, &ex0);
  }
  
  maxfd++;
  
  EState state = newstate;
  EState s1;

  start_time = miditime_current();

  while(running) {
    // check for a state change and prepare the next state
    if (state!=newstate) {
      cerr << "state change" << endl;
      
      state = newstate;
      s1 = state;
      for(TPlugInVector::iterator p=plugin.begin();
          p!=plugin.end();
          ++p)
      {
        (*p)->arrangement = arrangement;
        EState s = (*p)->stateChange(state);
        if (p==plugin.begin()) {
          s1 = s;
        } else {
          if (s1!=s) {
//            if (s1!=state) cerr << "execute: not all plugins wan't the same state, keep current state" << endl;
            s1 = state;
          }
        }
      }
      if (s1!=state) {
        newstate = s1;
      }

      if (state==STATE_PLAY || state==STATE_RECORD) {
        now = 0;
        start_time = miditime_current();
      }
      if (state==STATE_RECORD)
        arrangement->modified = true;
    }
    
    bool infwait = true;
    struct timeval tv;  // µs

    TPlugIn *nextplugin = 0;

    if (state!=STATE_STOP) {
      now = miditime_current() - start_time;
      
      // find the minimum time for the next event
      miditime_t min_next = UINT_MAX;
      for(TPlugInVector::iterator p=plugin.begin();
          p!=plugin.end();
          ++p)
      {
        if ((*p)->next>now) {
          if ((*p)->next < min_next) {
            min_next = (*p)->next;
            nextplugin = *p;
          }
        } else {
          min_next = 0; // don't wait, act at once!
          nextplugin = *p;
          break;
        }
      }
      
      if (min_next) {
        if (min_next != UINT_MAX) {
          miditime_t d = min_next - now;
//          cerr << "wait " << d << " µs" << endl;
          tv.tv_sec  = d / 1000000UL;
          tv.tv_usec = (d % 1000000UL);
          infwait = false;
//        } else {
//          cerr << "wait oo µs" << endl;
        }
      } else {
//        cerr << "wait 0 µs" << endl;
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        infwait = false;
      }
    } else {
//      cerr << "wait oo µs" << endl;
    }

    // wait for the next event
    fd_set rd, wr, ex;
    rd = rd0; wr=wr0; ex=ex0;
    int ret = select(maxfd, &rd, &wr, &ex, infwait ? 0 : &tv );

    now = miditime_current() - start_time;
//cout << "select returned at " <<  (now / 1000000UL) << "." << (now % 1000000UL) << endl;
    if (ret>0) {
      // consume selfpipe signal
      if (FD_ISSET(selfpipe[0], &rd)) {
        // cerr << "got self pipe signal\n";
        char buffer[20];
        while(true) {
          int n = read(selfpipe[0], buffer, sizeof(buffer));
          if (n==0) break;
          if (n==-1) {
            if (errno==EAGAIN) break;
            cerr << "errno: " << errno << endl;
            break;
          }
        }
      }
      
      for(TPlugInVector::iterator p=plugin.begin();
          p!=plugin.end();
          ++p)
      {
        (*p)->read(state, &rd);
      }
    }
    
    s1 = state;
    for(TPlugInVector::iterator p=plugin.begin();
        p!=plugin.end();
        ++p)
    {
      (*p)->arrangement = arrangement;
      EState s = (*p)->execute(state, now);
      if (p==plugin.begin()) {
        s1 = s;
      } else {
        if (s1!=s) {
//          if (s1!=state) cerr << "execute: not all plugins wan't the same state, keep current state" << endl;
          s1 = state;
        }
      }
    }
    if (s1!=state) {
      if (s1==STATE_STOP) {
        cerr << "***** STOP *****" << endl;
      }
      newstate = s1;
    }

  }
//  delete plugin;
  cerr << "media event motor: leaving main\n";
}
