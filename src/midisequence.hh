/*
 *  Modesto -  A non-linear video-, audio- and midi editor
 *  Copyright (C) 2002-2004 Mark-André Hopf <mhopf@mark13.de>
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

#ifndef _MODESTO_MIDISEQUENCE_HH
#define _MODESTO_MIDISEQUENCE_HH

#include "modesto.hh"

class TArrangement;

class TMIDISequence
{
    friend class TArrangement;
    //! Name of this Sequence
    string name;
    
    unsigned start;
    
    //! Duration of this Sequence (calculated for display envents);
    unsigned duration;
    
  public:
    TMIDISequence() {
      ipos = 0;
      start = 0;
      duration = 0;
    }
    void putByte(byte b) {
      data.append(1, b);
    }
    void putBytes(char *bytes, unsigned count) {
      data.append(bytes, count);
    }
    
    void setName(const string &name) { this->name = name; }
    const string& getName() const { return name; }
    miditime_t getStart() const {
      return start;
    }
    miditime_t getDuration() const { return duration; }
    const string& getData() const { return data; }
    
    /**
     * Methods for retrieving events from the sequence
     */
    void startRead() { ipos = 0; }
    bool isEnd() const { return ipos >= data.size(); }
    miditime_t getLength();
    byte getByte() { return data[ipos++]; }
    byte peekByte() const { return data[ipos]; }
    void nextByte() { ++ipos; }
    
    void clear() {
      data.clear();
    }
    
  protected:
    string data;
    unsigned ipos;
};

#endif
