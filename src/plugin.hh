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

#ifndef _MODESTO_PLUGIN_HH
#define _MODESTO_PLUGIN_HH

#include "mediaeventmotor.hh"
#include <unistd.h>

class TPlugIn
{
  public:
    TPlugIn() {
      arrangement = 0;
      next = UINT_MAX;
    }
#ifdef USE_POLL
    virtual unsigned registerFDs(pollfd*) = 0;
#else
    virtual void registerFDs(int *maxfd, fd_set *rd, fd_set *wr, fd_set *ex) = 0;
    virtual void read(TMediaEventMotor::EState state, fd_set *rd) = 0;
#endif
    virtual TMediaEventMotor::EState stateChange(TMediaEventMotor::EState state) = 0;
    virtual TMediaEventMotor::EState execute(TMediaEventMotor::EState state, miditime_t now) = 0;

    TArrangement *arrangement;
    miditime_t next;
};

#endif
