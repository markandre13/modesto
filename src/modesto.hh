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

#ifndef _MODESTO_MODESTO_HH
#define _MODESTO_MODESTO_HH

// for uint64_t
#include <stdint.h>
#include <toad/types.hh>

using namespace toad;

// microseconds in unsigned 64 bit
typedef uint64_t miditime_t;

extern unsigned midicmdlen[16];

//#include <toad/io/atvparser.hh>

#define RESOURCE(file) "file://resource/" file
// #define RESOURCE(file) "file:///usr/share/lib/modesto/0.0.0/"
// #define RESOURCE(file) "memory://resource/"

#endif
