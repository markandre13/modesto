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

#ifndef _MODESTO_MEDIAEVENTMOTOR_HH
#define _MODESTO_MEDIAEVENTMOTOR_HH

#include "thread.hh"
#include "arrangement.hh"

class TPlugIn;

class TMediaEventMotor:
  public TThreadLoop
{
  protected:
    int selfpipe[2];
    typedef vector<TPlugIn*> TPlugInVector;

  public:
    TMediaEventMotor();
    ~TMediaEventMotor();

    enum EState {
      STATE_PREV,
      STATE_REWIND,
      STATE_FORWARD,
      STATE_NEXT,
      STATE_STOP,
      STATE_RECORD,
      STATE_PLAY
//      STATE_NEWARRANGEMENT
    };
    void setState(EState state) {
      newstate = state;
      awake();
    }
    void setArrangement(TArrangement *arrangement);
    TArrangement* getArrangement() const {
      return arrangement;
    }
    
    void stop() {
      if (!running)
        return;
      running = false;
      awake();
      TThreadLoop::join();
    }

    void addPlugIn(TPlugIn *plugin);

  protected:
    volatile EState newstate; // a new state the user wants
    TArrangement *arrangement;
public:
    void main();
protected:
    void awake();

    TPlugInVector plugin;
};

#endif
