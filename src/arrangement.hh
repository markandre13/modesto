/*
 *  Modesto -  A non-linear video-, audio- and midi editor
 *  Copyright (C) 2002-2004 Mark-Andr� Hopf <mhopf@mark13.org>
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

#ifndef _MODESTO_ARRANGEMENT_HH
#define _MODESTO_ARRANGEMENT_HH

#include <vector>
#include <toad/connect.hh>
#include <toad/io/serializable.hh>

//#include <toad/textmodel.hh>
//#include <toad/boundedrangemodel.hh>

#include "modesto.hh"
// #include "midisequence.hh"

class TTrack;

/**
 * Objects of this class hold a complete set of MIDI, Audio and Video
 * tracks.
 */
class TArrangement:
  public TSerializable
{
    typedef TSerializable super;
    SERIALIZABLE_INTERFACE(modesto::, TArrangement)

    TTrack *currenttrack;
    
  public:
    string filename;
    bool modified;
  
    TArrangement() {
      currenttrack = 0;
      modified = false;
    }
    TArrangement(const TArrangement &) {
      currenttrack = 0;
      modified = false;
    }
    ~TArrangement();
  
    typedef vector<TTrack*> TTrackVector;
    TTrackVector tp;
    
    void addTrack(TTrack *track);
    void newTrack();
    void deleteTrack();
    void selectTrack(TTrack *track) {
      if (currenttrack == track)
        return;
      currenttrack = track;
      reason = TRACK_SELECTED;
      sigChanged();
    }
    TTrack *getSelectedTrack() const { return currenttrack; }

    void clear();
    void load(istream &in);
    
    enum { 
      TRACK_SELECTED, 
      TRACK_ADDED,
      TRACK_REMOVED,
    } reason;
    TSignal sigChanged;
};

#endif
