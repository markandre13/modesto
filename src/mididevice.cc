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

#include "mididevice.hh"

#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define LOG(CMD)

TMIDIDevice::TMIDIDevice(const string &name, const string &device)
{
  cmd = 0;
  cmdlen = 0;
  remain = 0;
  
  fd = -1;
  this->name = name;
  this->device = device;

  if (device.empty() || device=="*")
    return;

  fd = open(device.c_str(), O_RDWR);
  if (fd==-1) {
    cerr << "failed to open '" << device << "'\n";
    return;
  }

  // just in case this is RS 232 instead of real MIDI, ie. a Yamaha TG 100
  termios t;
  tcgetattr(fd, &t);
  t.c_iflag = 0;
  t.c_oflag = 0;
  t.c_lflag = 0;
  t.c_cc[VMIN]=0;
  t.c_cc[VTIME]=0;
  t.c_cflag = CREAD | CLOCAL | CS8 | B38400 | HUPCL;
  tcsetattr(fd, TCSANOW, &t);
  fcntl(fd, F_SETFL, O_NONBLOCK);
}

TMIDIDevice::~TMIDIDevice()
{
  if (fd!=-1)
    close(fd);
}

int
TMIDIDevice::write(const byte *buffer, int size)
{
  if (fd==-1) return size;
  return ::write(fd, buffer, size);
}

/**
 * This method copies the data from the file descriptor into
 * the inbuffer
 */
void
TMIDIDevice::read()
{
  if (fd==-1) return;

  // try to read from file descriptor
  while(true) {
    byte *buf;
    unsigned n;
    int m;
    inbuffer.getWriteSegment(&buf, &n);
    m = ::read(fd, buf, n);
    if (m<=0) {
      if (m<0 && errno!=EAGAIN) {
        cerr << "errno: " << errno << endl;
      }
      break;
    } else {
      inbuffer.consumeWrite(m);
      // cerr << "got " << m << " bytes" << endl;
    }
    LOG(fprintf(logfile, "got %u bytes\n", m);)
  }
}

/**
 * This method will return data from the MIDI device with the
 * following properties to ease the internal handling of MIDI commands:
 *
 * \li
 *   The first byte will always be a MIDI command.
 * \li
 *   The content is uncompressed (every command has a command byte)
 * \li
 *   The last command is complete. (This also means that this
 *   method will/must skip SysEx messages, unless we change the buffer
 *   from 'byte*' to 'string'...)
 * \li
 *   MIDI Clock events are ignored.
 */
int
TMIDIDevice::read(byte *out, int max)
{
  // try to find a complete command
  unsigned n;
  midibuffer::iterator p = inbuffer.begin(), e = inbuffer.end();
  
  if (p==e) {
//    LOG(fprintf(logfile, "buffer is empty\n");)
    return 0;
  }
    
  LOG(fprintf(logfile, "\nstart looking for command\n");)
  n = 0;
  byte *o = out;
  while(p!=e) {
    LOG(fprintf(logfile, "    peek %02X\n", *p);)
    if (n==0) {
      byte c = *p;
      if (c&0x80) {
        LOG(fprintf(logfile, "  command change to %02X\n", c);)
        cmdlen = midicmdlen[c>>4]+1;
        cmd = c;
      } else {
        LOG(fprintf(logfile, "  reuse command %02X and first byte\n", cmd);)
        cmdlen = midicmdlen[cmd>>4];
        if (cmdlen==0) {
          LOG(fprintf(logfile, "  FUCKED UP\n");)
          inbuffer.consumeRead(1);
          p = inbuffer.begin();
          e = inbuffer.end();
          continue;
        }
        *o = cmd;
        ++o;
      }
      *o = c;
      ++o;
    } else
  
    if (*p & 0x80) {
      // ouch! incomplete command... skip previous and try again
      cmd = *p;
      inbuffer.consumeRead(n);
      p = inbuffer.begin();
      e = inbuffer.end();
      n = 0;
      o = out;
      continue;
    } else {
      *o = *p;
      ++o;
    }
    ++n;
    --cmdlen;
    if (cmdlen==0) {
      break;
    }
    ++p;
  }
  if (cmdlen>0) {
     LOG(fprintf(logfile, "  incomplete command\n\n");)
     return 0;
  }
  if (inbuffer.begin()==inbuffer.end()) {
    LOG(fprintf(logfile, "  UPS: buffer is empty\n");)
    return 0;
  }
  LOG(fprintf(logfile, "command %02X of size %u, consume %u\n\n", cmd, o-out, n);)
//  if (cmd!=0xFE) fprintf(stderr, "command %02X of size %u, consume %u\n\n", cmd, o-out, n);
  inbuffer.consumeRead(n);
  return o-out;
}

void
TMIDIDevice::store(TOutObjectStream &out) const
{
  ::store(out, "name", name);
  ::store(out, "device", device);
}

bool
TMIDIDevice::restore(TInObjectStream &in)
{
  if (
    ::restore(in, "name", &name) ||
    ::restore(in, "device", &device) ||
    super::restore(in)
  ) return true;
  ATV_FAILED(in);
  return false;
}
