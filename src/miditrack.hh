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

#ifndef _MODESTO_MIDITRACK_HH
#define _MODESTO_MIDITRACK_HH

#include "track.hh"
#include "midisequence.hh"

class TMIDIDevice;

class TMIDITrack:
  public TTrack
{
    typedef TTrack super;
    SERIALIZABLE_INTERFACE(modesto::, TMIDITrack)
  public:
    TBoundedRangeModel volume, pan, channel, bank, program;

  protected:
    byte cmd; // last command
    TMIDISequence *seq;
    
    void allocbuffer(unsigned min) {
      if (bufferlen < min) {
        bufferlen = min;
        buffer = (byte*)realloc(buffer, bufferlen);
      }
    }
    
  public:
    TMIDIDevice *in, *out;

    TMIDITrack();
    TMIDITrack(const TMIDITrack&) {
      init();
    }
    TMIDITrack(TMIDISequence *s);
    void init();
    
    ~TMIDITrack() {
      if (buffer)
        free(buffer);
    }

    TWindow* getEditor(TWindow*);


    TMIDISequence* getFirstSequence() const {
      return seq;
    }
    
    miditime_t ticks;
private:
    byte* buffer;
    unsigned bufferlen;
    unsigned cmdlen;
public:

    void clear() {
      seq->clear();
      ticks=0;
    }
    
    // for playing
    void seekStart();
    void nextEvent();
    void play();

    // for recording
    void write(miditime_t now, byte *data, unsigned n);
};

#endif
