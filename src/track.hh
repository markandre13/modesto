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

#ifndef _MODESTO_TRACK_HH
#define _MODESTO_TRACK_HH

#include <toad/window.hh>
#include <toad/io/serializable.hh>
#include <toad/textmodel.hh>
#include <toad/boundedrangemodel.hh>

#include "modesto.hh"

class TTrack:
  public TSerializable
{
    typedef TSerializable super;
//    SERIALIZABLE_INTERFACE(modesto::, TTrack)
  protected:
    void store(TOutObjectStream &out) const;
    bool restore(TInObjectStream &in);
  public:
    TTrack();
  
    volatile bool mute, solo, record, monitor, lock;

    void setName(const string &name) {
      this->name = name;
    }
    const string& getName() const {
      return name;
    }
    virtual TWindow *getEditor(TWindow*)=0;

    //! name of this track/instrument
    TTextModel name;
};

#endif
