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

#ifndef _MODESTO_MIDIDEVICE_HH
#define _MODESTO_MIDIDEVICE_HH

#include <toad/io/serializable.hh>

#include "modesto.hh"
#include "segmentqueue.hh"

class TMIDIDevice:
  public TSerializable
{
    typedef TSerializable super;
    SERIALIZABLE_INTERFACE(modesto::, TMIDIDevice)
  public:
    int fd;
    TMIDIDevice(const string &name, const string &device);
    ~TMIDIDevice();
  
    int write(const byte *buffer, int size);
    void read();
    int read(byte *buffer, int size);

    const char *toText(int) const { return name.c_str(); }
    string name;
    string device;
    
    byte cmd;
    unsigned cmdlen;
    unsigned remain;
    byte buffer[4096];
  
    typedef segmentqueue<byte, 64> midibuffer;
    midibuffer inbuffer;
};

#endif
