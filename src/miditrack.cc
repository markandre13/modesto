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

#include "miditrack.hh"
#include "midi.hh"

void
programchange(TMIDITrack *p)
{
  cerr << "program change on track '" << p->name << "'\n";
  cerr << "  set channel " << p->channel << " to program " << p->program << endl;
  byte buffer[2];
  buffer[0] = 0xC0 | (p->channel - 1);
  buffer[1] = (p->program - 1);
  p->out->write(buffer, 2);
}

TTrack::TTrack()
{
  name = "(unnamed)";
  mute = solo = record = monitor = lock = false;
}

TMIDITrack::TMIDITrack(TMIDISequence *s)
{
  assert(s!=NULL);
  init();
  seq = s;
}

TMIDITrack::TMIDITrack()
{
  init();
}

void
TMIDITrack::init()
{
  volume = 128;
  pan = 64;
  channel = 1;
  bank = 1;
  program = 1;
  in = midiinlist[1];
  out = midioutlist[1];

  channel.setMinimum(1);
  channel.setMaximum(16);

  program.setMinimum(1);
  program.setMaximum(128);

  connect(program.sigChanged, programchange, this);
  
  seq = 0;
  bufferlen = 8;
  buffer = (byte*)malloc(bufferlen);
}

TWindow*
TMIDITrack::getEditor(TWindow *parent)
{
  static TWindow *p = 0;
  static TWindow *w = 0;
  if (p!=0) {
    assert(parent==p);
    return w;
  }
  w = new TWindow(parent, "midi-track-editor");
  w->setBackground(255, 0, 0);
  w->setSize(100, 120);
  return w;
}

void
TMIDITrack::seekStart() {
  assert(seq!=NULL);
//cerr << "seekStart for sequence " << seq << endl;
  seq->startRead();
  ticks = 0;
  cmd = 0;
  cmdlen = 0;
}

void
TMIDITrack::nextEvent()
{
  if (seq->isEnd()) {
    ticks = UINT_MAX;
    cmdlen = 0;
    return;
  }
  ticks += seq->getLength();
  byte b = seq->getByte();
  unsigned i;
  if (b & 0x80) {
    buffer[0] = b;
    cmd = b;
    cmdlen = midicmdlen[cmd>>4]+1;
    i = 1;
  } else {
    buffer[0] = cmd;
    buffer[1] = b;
    cmdlen = midicmdlen[cmd>>4]+1;
    i = 2;
  }
  allocbuffer(cmdlen);
  while(i<cmdlen) {
    buffer[i] = seq->getByte();
    ++i;
  }
}

void
TMIDITrack::play()    
{
  // suppress all non 'program change' events on muted tracks
  if (!mute || (buffer[0] & 0xF0) == 0xC0) {
    out->write(buffer, cmdlen);
  }
}

void
TMIDITrack::write(miditime_t now, byte *data, unsigned n)
{
  miditime_t value = now - ticks;
  ticks = now;
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

  for(unsigned i=0; i<n; ++i)
    seq->putByte(data[i]);
}

void
TTrack::store(TOutObjectStream &out) const
{
  ::store(out, "name", name);
  ::store(out, "mute", mute);
  ::store(out, "solo", solo);
  ::store(out, "record", record);
  ::store(out, "monitor", monitor);
  ::store(out, "lock", lock);
}

bool
TTrack::restore(TInObjectStream &in)
{
  if (
    ::restore(in, "name", &name) ||
    ::restore(in, "mute", &mute) ||
    ::restore(in, "solo", &solo) ||
    ::restore(in, "record", &record) ||
    ::restore(in, "monitor", &monitor) ||
    ::restore(in, "lock", &lock) ||
//    ::restore(in, "in", &u) ||
//    ::restore(in, "out", &u) ||
    super::restore(in)
  ) return true;
  ATV_FAILED(in);
  return false;
}

void
TMIDITrack::store(TOutObjectStream &out) const
{
  super::store(out);
  ::store(out, "volume", volume);
  ::store(out, "pan", pan);
  ::store(out, "channel", channel);
  ::store(out, "bank", bank);
  ::store(out, "program", program);

  unsigned i;

  i = 0;
  for(TMIDIDeviceStorage::iterator p=midiinlist.begin();
      p!=midiinlist.end();
      ++p, ++i)
  {
    if (in == *p) {
      ::store(out, "in", i);
      break;
    }
  }

  i = 0;
  for(TMIDIDeviceStorage::iterator p=midioutlist.begin();
      p!=midioutlist.end();
      ++p, ++i)
  {
    if (this->out == *p) {
      ::store(out, "out", i);
      break;
    }
  }

  // this somehow doubles the size compared to a regular MIDI file, but
  // gzip'ed it just 1/3 of a regular MIDI file...
  ::storeRaw(out, "sequence", seq->getData().c_str(), seq->getData().size());
}

bool
restore(TInObjectStream &in, bool volatile *value)
{
  if (in.what != ATV_VALUE)
    return false;
  *value = in.value.compare("true")==0;
  return true;
}



bool
TMIDITrack::restore(TInObjectStream &in)
{
//cerr << "track: restore " << in.attribute << endl;

  string sequence;
  unsigned u;

  #warning "restoreRaw requires static parameters..."
  static char *rawdata;
  static unsigned rawlen;

  if (in.what==ATV_FINISHED) {
    seq=new TMIDISequence();
    seq->putBytes(rawdata, rawlen);
    free(rawdata);
    return true;
  }

  if (
    ::restore(in, "volume", &volume) ||
    ::restore(in, "pan", &pan) ||
    ::restore(in, "channel", &channel) ||
    ::restore(in, "bank", &bank) ||
    ::restore(in, "program", &program) ||
    ::restore(in, "in", &u) ||
    ::restore(in, "out", &u) ||
//    ::restoreRaw(in, "sequence", &sequence) ||
    ::restoreRaw(in, "sequence", &rawdata, &rawlen) ||
    super::restore(in)
  ) return true;
  ATV_FAILED(in);
  return false;
}
