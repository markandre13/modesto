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

#ifndef _MODESTO_AUDIOTRACK_HH
#define _MODESTO_AUDIOTRACK_HH

#include "track.hh"

// don't do file io
//#define NO_AUDIO_FILE

class TAudioTrack:
  public TTrack
{
    typedef TTrack super;
    SERIALIZABLE_INTERFACE(modesto::, TAudioTrack)

    #ifdef NO_AUDIO_FILE
    int freq;
    int counter;
    #endif

    int fd;
    string filename;
    bool wflag;
    unsigned pos;
    
  public:
    TAudioTrack();
    ~TAudioTrack();
    
    TWindow* getEditor(TWindow*);
    
    void open(bool write);
    void close();
    void write(char *buffer, unsigned n);
    unsigned read(char *buffer, unsigned n);
    
    bool done:1;
    bool temporary:1;
};

#endif

