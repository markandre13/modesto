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

#include <toad/io/binstream.hh>

#include "arrangement.hh"
#include "miditrack.hh"
#include "midisequence.hh"

TArrangement::~TArrangement()
{
  clear();
}

template <typename T>
void
storelist(TOutObjectStream &out, const string &name, T p, T last)
{
  out.indent();
  out << name << " = ";
  out.startGroup();
  for(; p!=last; ++p)
  {
    ::store(out, *p);
  }
  out.endGroup();
}

void
TArrangement::store(TOutObjectStream &out) const
{
//  storelist(out, "midiinlist", midiinlist.begin(), midiinlist.end());
//  storelist(out, "midioutlist", midioutlist.begin(), midioutlist.end());

  for(TTrackVector::const_iterator p = tp.begin();
      p!=tp.end();
      ++p)
  {
    ::store(out, *p);
  }
}

bool
TArrangement::restore(TInObjectStream &in)
{
  static TTrack *trk;
  if (in.what==ATV_START) {
    clear();
    return true;
  }
  if (::restorePtr(in, &trk)) {
    tp.push_back(trk);
    return true;
  }
  if (
    super::restore(in)
  ) return true;
  ATV_FAILED(in);
  return false;
}


// play file
#define DEBUG2(CMD)

// read file
#define DEBUG1(CMD) CMD

static void
putLength(TMIDISequence *seq, miditime_t value)
{
  miditime_t buffer;
  buffer = value & 0x7F;

  while ( (value >>= 7) ) {
    buffer <<= 8;
    buffer |= ((value & 0x7F) | 0x80);
  }

  while(true) { 
    seq->putByte(buffer & 0xFF);
    if (buffer & 0x80)
      buffer >>= 8;
    else
      break;
  }
}

static unsigned
readLen(TInBinStream &in)
{
  unsigned cmdlen = 0;
  if ( (cmdlen = in.readByte()) & 0x80 ) {
    cmdlen &= 0x7F;
    byte c;
    do {
      cmdlen = (cmdlen << 7) + ((c = in.readByte()) & 0x7F);
    } while (c & 0x80);
  }
  return cmdlen;
}

static string
readText(TInBinStream &in, unsigned len) {
  string text;
  while(len>0) {
    text += in.readByte();
    --len;
  }
  return text;
}

void
TArrangement::clear()
{
  modified = true;
  TTrackVector copy(tp);
  currenttrack = 0;
  tp.erase(tp.begin(), tp.end());
  reason = TRACK_REMOVED;
  sigChanged();

  for(TTrackVector::iterator p=copy.begin(); p!=copy.end(); ++p) {
    delete *p;
  }
}

void
TArrangement::addTrack(TTrack *track)
{
  modified = true;
  tp.push_back(track);
  reason = TRACK_ADDED;
  sigChanged();
}

/**
 * Append a new track.
 */
void
TArrangement::newTrack()
{
  modified = true;
  cerr << __PRETTY_FUNCTION__ << endl;
  TMIDISequence *seq = new TMIDISequence();
  TMIDITrack *track = new TMIDITrack(seq);
  tp.push_back(track);
  reason = TRACK_ADDED;
  sigChanged();
}

/**
 * Delete the current track.
 */
void
TArrangement::deleteTrack()
{
  modified = true;
  cerr << __PRETTY_FUNCTION__ << endl;
  for(TTrackVector::iterator p=tp.begin();
      p!=tp.end();
      ++p)
  {
    if (currenttrack==*p) {
      currenttrack = 0;
      TTrack *track = *p;
      tp.erase(p);
      reason = TRACK_REMOVED;
      sigChanged();
      delete track;
      return;
    }
  }
}

/**
 * This method currently loads MIDI files. In the future it will load any
 * kind of files into an arrangement.
 */
void
TArrangement::load(istream &is)
{
  TInBinStream in(&is);
  in.setEndian(TInBinStream::BIG);
  
  unsigned next;
  string type;
  unsigned size;

  unsigned format, ntracks;
  int division;
  unsigned tracknumber = 1;

  unsigned tempo = 500000; // 120 BPM
  while(!in.eof()) {
    try {
      type = in.readString(4);
      size = in.readDWord();
    } catch(...) {
      cerr << "exception" << endl;
      break;
    }
    next = in.tellRead() + size;
  
    if (type=="MThd") {
      format   = in.readWord();
      ntracks  = in.readWord();
      division = in.readSWord();
    } else
    
    if (type=="MTrk") {
      TMIDISequence *seq = new TMIDISequence();
      TMIDITrack *track = new TMIDITrack(seq);
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "track %02u", tracknumber);
      tracknumber++;
      track->setName(buffer);
      tp.push_back(track);
    
      bool loop = true;
      byte cmd = 0;
      bool notime = true;
      
      miditime_t midiclock = 0; // clock
      miditime_t clock = 0;     // clock in µs for modesto
      
      unsigned channel = 0;
      unsigned port = 0;
      
      while(loop) {
        miditime_t delta;
        byte c;
        if ( (delta = in.readByte()) & 0x80 ) {
          delta &= 0x7F;
          do {
            delta = (delta << 7) + ((c = in.readByte()) & 0x7F);
          } while (c & 0x80);
        }
        
        midiclock += delta;
        miditime_t oldclock = clock;
        
        // the following calculation should use 'delta' instead of
        // midiclock in case the tempo changes within the track?
        // but this would drop precision, better to adjust the midiclock
        // during a tempo change with a yet unknown formula
        if (division > 0) {
          clock = midiclock * tempo / division;
        } else {
          // frames per second
          unsigned fps = - ( (char) (division >> 8) );
          // units per frame
          unsigned upf = division & 0xFF;
          clock = midiclock * 1000000U / (fps * upf);
        }

        if (notime) {
          seq->start = clock;
          notime = false;
        }
        
        c = in.readByte();
        unsigned cmdlen;
        if (c==0xF0 || c==0xF7) {
          // Skipt SysEx (F0) and SysEx additional parts (F7)
          cmdlen = readLen(in);
          
          while(cmdlen>0) {
            in.readByte();
            --cmdlen;
          }
        } else
        if (c!=0xFF) {
        
          // normal MIDI command
          // seq->putLength(clock - oldclock);
          putLength(seq, clock - oldclock);
          // in case the first byte isn't a command, reuse the previous
          // command
          if (c & 0x80) {
            cmd = c;
            cmdlen = midicmdlen[c>>4];
          } else {
            cmdlen = midicmdlen[cmd>>4] - 1;
          }
          
          seq->putByte(c);
          for(unsigned j=0; j<cmdlen; ++j) {
            c = in.readByte();
            seq->putByte(c);
          }
        } else { 
          // 0xFF: special MIDI file commands:
          c = in.readByte();
          cmdlen = readLen(in);
          string text;
          switch(c) {
            case 0x01: // text event
              text = readText(in, cmdlen);
              break;
            case 0x02: // copyright notice
              text = readText(in, cmdlen);
              break;
            case 0x03: // case sequence/track name
              text = readText(in, cmdlen);
              seq->setName(text);
              break;
            case 0x04: // instrument name
              text = readText(in, cmdlen);
              track->setName(text);
              break;
            case 0x05: // lyric
              text = readText(in, cmdlen);
              break;
            case 0x06: // marker
              text = readText(in, cmdlen);
              break;
            case 0x07: // cue point
              text = readText(in, cmdlen);
              break;
            case 0x20: // MIDI channel prefix
              channel = in.readByte();
              // printf("MIDI channel %u\n", channel);
              break;
            case 0x21: // MIDI port prefix
              port = in.readByte();
              // printf("MIDI port %u\n", port);
              break;
            case 0x2F:
              // printf("End of Track\n");
              seq->duration = clock;
              loop = false;
              break;
            case 0x51:
              tempo = in.readByte();
              tempo<<=8;
              tempo |= in.readByte();
              tempo<<=8;
              tempo |= in.readByte();
              // printf("Tempo (PPQN) %u\n", tempo);
              break;
            case 0x54: {
              unsigned hour, minute, second, frame, ff;
              hour = in.readByte();
              minute = in.readByte();
              second = in.readByte();
              frame = in.readByte();
              ff = in.readByte();
              // printf("SMPTE Offset %u:%u:%u.%u.%u\n", hour, minute, second, frame, ff);
              } break;
            case 0x58: {
              unsigned nn, dd, cc, bb;
              nn = in.readByte();
              dd = in.readByte();
              cc = in.readByte();
              bb = in.readByte();
              // printf("Time Signature %u %u %u %u\n", nn, dd, cc, bb);
              } break;
            case 0x59: {
              unsigned sf, mi;
              sf = in.readByte();
              mi = in.readByte();
              // printf("Key Signature %u %u\n", sf, mi);
            } break;
            default:
              printf("unhandled MIDI file command %02x, len=%u: ", c, cmdlen);
              for(unsigned j=0; j<cmdlen; ++j) {
                c = in.readByte(); 
                printf("%02x ", c);
              }
              printf("\n");
              goto done;
          }
        }
      }  
    }    
    in.seekRead(next);
  }
done:
  reason = TRACK_ADDED;
  sigChanged();
}
