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

#include "audio.hh"
#include "audiotrack.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

#include <sstream>
#include <toad/io/binstream.hh>

#define DBM(CMD)

#define WITH_CAPTURE
#define WITH_PLAYBACK
//#define WITH_MMAP
//#define WITH_DIRECT_MMAP

class TPCM
{
  public:
    snd_pcm_t *handle;

    string device;
    unsigned rate;
    unsigned realrate;
    int channels;
    int block;
    snd_pcm_stream_t type;
    snd_pcm_access_t access;
    snd_pcm_format_t format;
    unsigned width;
    
    snd_pcm_uframes_t buffersize;
    snd_pcm_uframes_t realbuffersize;
    snd_pcm_uframes_t periodsize;
    snd_pcm_uframes_t realperiodsize;

    int _fd;

    TPCM();
    bool open();
    int fd() const { return _fd; }
    snd_pcm_sframes_t write(const char *buffer, snd_pcm_uframes_t size);
    snd_pcm_sframes_t read(char *buffer, snd_pcm_uframes_t size);
    int prepare() { return snd_pcm_prepare(handle); }
    int start() { return snd_pcm_start(handle); }
    
    void printState();
};

void
TPCM::printState()
{
  static const char *sname[] = {
    "SND_PCM_STATE_OPEN",
    "SND_PCM_STATE_SETUP",
    "SND_PCM_STATE_PREPARED",
    "SND_PCM_STATE_RUNNING",
    "SND_PCM_STATE_XRUN",
    "SND_PCM_STATE_DRAINING",
    "SND_PCM_STATE_PAUSED",  
    "SND_PCM_STATE_SUSPENDED",
    "SND_PCM_STATE_DISCONNECTED"
  };

  snd_pcm_state_t state;
  state = snd_pcm_state(handle);
  cout << sname[state] << endl;
}

TPCM::TPCM()
{
  rate = 44100;
  channels = 2;
  device = "hw:0,0";

#warning "blocking will cause trouble with other plugins"
  block = 0;
  block = SND_PCM_NONBLOCK;
  type = SND_PCM_STREAM_PLAYBACK;
  access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
  format = SND_PCM_FORMAT_S16_LE;
  width = 2;

#if 0
  // this is the slow debugging one
  rate = 5512;
  buffersize = rate*6;
  periodsize = buffersize/16;
#else
  // this is the good one
  buffersize = rate;
  periodsize = buffersize/128;
#endif

  cout << "using a period of " << ((double)periodsize/(double)rate) << " seconds" << endl;
  cout << "  frames per period: " << periodsize << endl;
  cout << "  frames in buffer : " << buffersize << endl;
}

bool
TPCM::open()
{
  snd_pcm_hw_params_t *hw;
  snd_pcm_sw_params_t *sw;

  snd_pcm_hw_params_alloca(&hw);
  snd_pcm_sw_params_alloca(&sw);
  
  for(unsigned i=0; i<15; ++i) {
    int r = 0;
    const char *msg;
    switch(i) {
      case 0:
        r = snd_pcm_open(&handle, device.c_str(), type, block);
        msg = "snd_pcm_open";
        break;
      case 1:
        r = snd_pcm_hw_params_any(handle, hw);
        msg = "failed to get software parameters";
        break;
      case 2:
        r = snd_pcm_hw_params_set_access(handle, hw, access);
        msg = "access type not available";
        break;
      case 3:
        r = snd_pcm_hw_params_set_format(handle, hw, format);
        msg = "failed to set sample format";
        break;
      case 4:
        r = snd_pcm_hw_params_set_channels(handle, hw, channels);
        msg = "failed to set number of channels";
        break;
      case 5:
        realrate = rate;
        r = snd_pcm_hw_params_set_rate_near(handle, hw, &realrate, 0);
        msg = "failed to set rate";
        if (realrate != rate) {
          cout << "using rate of " << realrate << "Hz instead of " << rate << "Hz" << endl;
        }
        break;
      case 6:
        realbuffersize = buffersize;
        r = snd_pcm_hw_params_set_buffer_size_near(handle, hw, &realbuffersize);
        msg = "failed to set buffer size";
        if (realbuffersize != buffersize)
          cout << "using buffer size of " << realbuffersize << " instead of " << buffersize << endl;
        break;
      case 7:
        realperiodsize = periodsize;
        r = snd_pcm_hw_params_set_period_size_near(handle, hw, &realperiodsize, 0);
        msg = "failed to set period size";
        if (realperiodsize != periodsize)
          cout << "using period size of " << realperiodsize << " instead of " << periodsize << endl;
        break;
      case 8:
        r = snd_pcm_hw_params(handle, hw);
        msg = "failed to set hardware parameters";
        break;
      case 9:
        r = snd_pcm_sw_params_current(handle, sw);
        msg = "failed to get software parameters";
        break;
      case 10:
        r = snd_pcm_sw_params_set_start_threshold(handle, sw, 0);
        msg = "failed to set start threshold";
        break;
      case 11:
        r = snd_pcm_sw_params_set_silence_threshold(handle, sw, 0);
        msg = "failed to set silence threshold";
        break;
    }
    if (r<0) {
      cerr << "error: " << msg << endl;
      return false;
    }
  }

  pollfd pf[1];
  snd_pcm_poll_descriptors(handle, pf, 1);
  _fd = pf[0].fd;

  return true;
}

snd_pcm_sframes_t
TPCM::write(const char *buffer, snd_pcm_uframes_t size)
{
  snd_pcm_sframes_t r = 0;
  if (size==0) {
    DBM(cout << "write 0 frames" << endl;)
    return 0;
  }

  int err;
  r = snd_pcm_avail_update(handle);
  DBM(cout << "write " << size << " frames (" << r << ")\n";)
  if (r<(snd_pcm_sframes_t)size) {
    if (r<0) {
      cout << "error write: " << snd_strerror(r) << endl;
      return r;
    } else {
      cout << "warning write: only " << r << " frames space for " << size << endl;
      return 0;
    }
  } else {
    r = size;
  }
  while(r>0) {
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset, frames;
    frames = r;
    err = snd_pcm_mmap_begin(handle, &areas, &offset, &frames);
    if (err<0) {
      cout << "write begin xrun: " << snd_strerror(err) << endl;
      return err;
    }

    memcpy((char*)areas[0].addr+offset*width*channels, buffer, frames*width*channels);

    err = snd_pcm_mmap_commit(handle, offset, frames);
    if (err<0) {
      cout << "write commit xrun: " << snd_strerror(err) << endl;
      return err;
    }
    r-=frames;
    buffer+=frames*width*channels;
  }
  return size;
}

snd_pcm_sframes_t
TPCM::read(char *buffer, snd_pcm_uframes_t size)
{
  snd_pcm_sframes_t r;
  r = snd_pcm_avail_update(handle);
  if (r<(snd_pcm_sframes_t)size) {
    if (r<0) {
      cout << "error read: " << snd_strerror(r) << endl;
      return r;
    } else {
//      cout << "warning read: only " << r << " frames available, want " << size << endl;
      size = r;
    }
  }
  DBM(cout << "read " << size << " frames (" << r << ")\n";)
  r = size;
  while(r>0) {
    const snd_pcm_channel_area_t *areas;
    snd_pcm_uframes_t offset, frames;
    frames = r;
    int err = snd_pcm_mmap_begin(handle, &areas, &offset, &frames);
    if (err<0) {
      cout << "read xrun: " << snd_strerror(err) << endl;
      return err;
    }
    
    memcpy(buffer, (char*)areas[0].addr+offset*width*channels, frames*width*channels);
    
    snd_pcm_mmap_commit(handle, offset, frames);
    r-=frames;
    buffer+=frames*width*channels;
  }
  return size;
}

void
setscheduler()
{
  struct sched_param sched_param;

  if (sched_getparam(0, &sched_param) < 0) {
    printf("Scheduler getparam failed...\n");
    return;
  }
  sched_param.sched_priority = sched_get_priority_max(SCHED_RR);
  if (!sched_setscheduler(0, SCHED_RR, &sched_param)) {
    printf("Scheduler is set to Round Robin with priority %i...\n", sched_param.sched_priority);
    return;
  }
  printf("!!!Scheduler set to Round Robin with priority %i FAILED!!!\n", sched_param.sched_priority);
}

TAudioPlugIn::TAudioPlugIn()
{
//  init();
}

TPCM capture;
TPCM playback;
miditime_t period;
snd_pcm_uframes_t s;

// sometimes we do get two reads, followed by two writes, hence a set of
// buffers used in a ring:
unsigned cb, pb;
static const unsigned mb = 100;
static const unsigned buffersize = 0x8000;
short buffer[mb][buffersize];
short silence[buffersize];
unsigned bsize[mb];
  
void
TAudioPlugIn::init()
{
//  setscheduler();

  capture.type = SND_PCM_STREAM_CAPTURE;
  if (!capture.open())
    exit(EXIT_FAILURE);
  s = max(s, capture.realperiodsize);

#ifdef WITH_PLAYBACK
  if (!playback.open())
    exit(EXIT_FAILURE);
  s = max(s, playback.realperiodsize);
#endif

  cb = pb = 0;
  for(unsigned j=0; j<mb; ++j) {
    bsize[j]=0;
    for(unsigned i=0; i<buffersize; ++i)
      buffer[j][i]=0;
  }
  for(unsigned i=0; i<buffersize; ++i) {
    silence[i]=0;
  }

//  cout << "using a period of " << ((double)capture.realperiodsize/(double)capture.realrate) << " seconds" << endl;
  period = ((miditime_t)1000000 * capture.realperiodsize) / capture.realrate;
  cout << "period takes " << period << "µs\n";

  int r;
#ifdef WITH_PLAYBACK
  playback.write((char*)silence, s);
  playback.write((char*)silence, s);
  playback.write((char*)silence, s);
  playback.write((char*)silence, s);
#if 0
  cout << endl << "start audio" << endl;
  r = playback.start();
  if (r < 0) {
    cerr << "while starting playing: " << snd_strerror(r) << endl;
    exit(EXIT_FAILURE);
  }
#endif
#endif
  r = capture.start();
  if (r < 0) {
    cerr << "while starting capturing: " << snd_strerror(r) << endl;
    exit(EXIT_FAILURE);
  }
    
  cerr << "audio plugin initialized\n";
}

void
TAudioPlugIn::registerFDs(int *maxfd, fd_set *rd, fd_set *wr, fd_set *ex)
{
  init();

  // we register the FDs not to check whether reading or writing is
  // possible but to return earlier from select in case the machine
  // is under load
#if 1
  FD_SET(capture.fd(), rd);
  *maxfd = max(*maxfd, capture.fd());
#endif
#if 0
  FD_SET(playback.fd(), wr);
  *maxfd = max(*maxfd, playback.fd());
#endif
}

void
TAudioPlugIn::read(TMediaEventMotor::EState state, fd_set *rd)
{
}

TMediaEventMotor::EState
TAudioPlugIn::stateChange(TMediaEventMotor::EState state)
{
  for(unsigned i=0; i<arrangement->tp.size(); ++i) {
    TAudioTrack *track = dynamic_cast<TAudioTrack*>(arrangement->tp[i]);
    if (!track)
      continue;
        
    switch(state) {
      case TMediaEventMotor::STATE_PLAY:
        track->open(false);
        break;
      case TMediaEventMotor::STATE_RECORD:
        track->open(track->record);
        break;
      case TMediaEventMotor::STATE_STOP:
        track->close();
        break;
      default:
        return TMediaEventMotor::STATE_STOP;
    }
  }
  return state;
}

TMediaEventMotor::EState
TAudioPlugIn::prepare(TMediaEventMotor::EState state)
{
  next = period;
  return state;
}

bool ready = false;

TMediaEventMotor::EState
TAudioPlugIn::execute(TMediaEventMotor::EState state, miditime_t now)
{
  int r;
  
  next = now + period;
//cout << "next = " << next << endl << endl;
/*
  if (!ready) {
    init();
    ready = true;
    return state;
  }
*/
  //
  // Read
  //
    
  while(true) {
    DBM(cout << "  read @ " << cb << endl;)
    r = capture.read((char*)buffer[cb], buffersize/(capture.width*capture.channels));
    bsize[cb]=r;
    if (r<=0)
      break;
    cb++; if (cb>=mb) cb=0;
  }

  if (r<0) {
    cout << "read error: " << snd_strerror(r) << endl;
    r = capture.prepare();
    if (r<0) {
      cerr << "can't recovery from overrun:" << snd_strerror(r) << endl;
      exit(EXIT_FAILURE);
    }
    
    r = capture.start();
    if (r < 0) {
      cerr << "while starting capturing: " << snd_strerror(r) << endl;
      exit(EXIT_FAILURE);
    }
  }

#ifdef WITH_PLAYBACK
  //
  // Play
  //

  unsigned rb = pb;
  r = 0;
  if (bsize[pb]>0) {

    while(true) {
      if (bsize[pb]==0)
        break;
      DBM(cout << "  write @ " << pb << endl;)

      unsigned w = bsize[pb]*playback.width*playback.channels;
      short buf[buffersize];
      if (state==TMediaEventMotor::STATE_PLAY) {
        // play: don't use input buffer
        memset((char*)buf, 0, w);
      } else {
#if 0
        for(unsigned i=0; i<bsize[pb]*2; i+=2) {
          buffer[pb][i+1] = buffer[pb][i];
        }
#endif
        memcpy((char*)buf, (char*)buffer[pb], w);
      }

      // mix playing tracks into the buffer
      if (state==TMediaEventMotor::STATE_RECORD ||
          state==TMediaEventMotor::STATE_PLAY)
      {
        for(unsigned i=0; i<arrangement->tp.size(); ++i) {
          if (!arrangement->tp[i]->mute &&
              !arrangement->tp[i]->record)
          {
            TAudioTrack *track = dynamic_cast<TAudioTrack*>(arrangement->tp[i]);
            if (track && !track->done) {
              // 16bit; signed integer; low, high
              char b[w+4];
              unsigned s = track->read(b, w);
              if (s>0) {
#if 1
                b[w]=0;
                b[w+1]=0;
                b[w+2]=0;
                b[w+3]=0;

                typedef int v4hi __attribute__ ((mode(V4HI)));
                union vec {
                  v4hi mmx;
                  short int c[4]; 
                };

                vec *b0 = (vec*)buf;
                vec *b1 = (vec*)b;
                s /= playback.channels*4;
                for(unsigned j=0; j<s; ++j) {
                  b0->mmx = __builtin_ia32_paddsw(b0->mmx, b1->mmx);
                  ++b0;
                  ++b1;
                }
#else

                short *b0 = (short*)buf;
                short *b1 = (short*)b;
                s /= playback.channels;
                for(unsigned j=0; j<s; ++j) {
                  *b0 = (*b0 + *b1) >> 1;
                  ++b0;
                  ++b1;
                }
#endif
              }
            }
          }
        }
      }

      r = playback.write((char*)buf, bsize[pb]);
  
      if (r<=0)
        break;
      pb++; if (pb>=mb) pb=0;
    }
  }

  if (r>=0) {
    static bool pbs = false;
    if (!pbs && r>0) {
      cout << "starting playback" << endl;
      r = playback.start();
      if (r < 0) {
        cerr << "while starting playing: " << snd_strerror(r) << endl;
        exit(EXIT_FAILURE);
      }
      pbs=true;
    }
  } else if (r<0) {
    cout << "write error: " << snd_strerror(r) << endl;
    r = playback.prepare();
    if (r<0) {
      cerr << "  can't recover from underun:" << snd_strerror(r) << endl;
      exit(EXIT_FAILURE);
    }

    r = playback.write((char*)silence, s);
    if (r>=0)
      r = playback.write((char*)silence, s);
    if (r>=0)
      r = playback.write((char*)silence, s);
    if (r>=0)
      r = playback.write((char*)silence, s);

    pb = cb;

    if (r<0) {
       cerr << "  failed to prefill playback buffer:" << snd_strerror(r) << endl;
    }
    r = playback.start();
    if (r < 0) {
      cerr << "  failed to start playing: " << snd_strerror(r) << endl;
      exit(EXIT_FAILURE);
    }
  }
#endif

  // recording: write buffer to files
  if (state==TMediaEventMotor::STATE_RECORD) {
    for(unsigned i=0; i<arrangement->tp.size(); ++i) {
      if (arrangement->tp[i]->record) {
        TAudioTrack *track = dynamic_cast<TAudioTrack*>(arrangement->tp[i]);
        if (track) {
          unsigned j = rb;
          while(true) {
            if (j==pb)
              break;
            track->write((char*)buffer[j], bsize[j]*capture.width*capture.channels);
            j++; if (j>=mb) j=0;
          }
        }
      }
    }
  }

  while(true) {
    if (rb==pb)
      break;
    DBM(cout << "  clear @ " << rb << endl;)
    bsize[rb]=0;
    rb++; if (rb>=mb) rb=0;
  }
  
  return state;
}

#ifdef NO_AUDIO_FILE
static int freq = 220;
#endif

TAudioTrack::TAudioTrack()
{
  filename = "";
  temporary = true;
  fd = -1;
#ifdef NO_AUDIO_FILE
  freq = ::freq;
  ::freq*=2;
  counter=0;
#endif
}

TAudioTrack::~TAudioTrack()
{
  if (temporary && !filename.empty()) {
    unlink(filename.c_str());
  }
}

TWindow*
TAudioTrack::getEditor(TWindow *parent)
{
  static TWindow *p = 0;
  static TWindow *w = 0;
  if (p!=0) {
    assert(parent==p);
    return w;
  }
  w = new TWindow(parent, "audio-track-editor");
  w->setBackground(0, 0, 255);
  w->setSize(150, 200);
  return w;
}

void
TAudioTrack::open(bool write)
{
  close();
  
  if (write && filename.empty()) {
    for(unsigned i=0; i<0xffff; ++i) {
      char buffer[20];
      snprintf(buffer, sizeof(buffer), "audio%06d.wav", i);
      struct stat st;
      if (stat(buffer, &st)!=0 && errno==ENOENT) {
        filename = buffer;
        break;
      }
    }
  }
  
  cerr << "open file '" << filename << "' for " << (write?"writing":"reading") << endl;
  
  fd = ::open(filename.c_str(), write ? (O_CREAT|O_RDWR) : O_RDONLY, 0644);
  if (fd<0) {
    perror("open");
    return;
  }
  wflag = write;
  done = false;
  pos = 44;
  lseek(fd, pos, SEEK_SET);
}

void
TAudioTrack::close()
{
  if (fd<0)
    return;
    
  if (wflag) {
cerr << "write wav header" << endl;

    lseek(fd, 0, SEEK_SET);

    string wavheader;
    
    unsigned size = pos;
    unsigned channels = 2;
    unsigned frequency = 44100;
    unsigned bits_per_sample = 16;
  
    ostringstream out;
    TOutBinStream bs(&out);
    bs.setEndian(TBinStreamBase::LITTLE);
  
    bs.writeString("RIFF");
    bs.writeDWord(size - 8);
    bs.writeString("WAVE");
    
    // format chunk
    bs.writeString("fmt ");
    bs.writeDWord(16); // chunk size
    bs.writeWord(1);   // 1=PCM,0x101=mu-law,0x102=a-law,0x103=AVC AD PCM
  
    bs.writeWord(channels);
    bs.writeDWord(frequency);
  
    // average bytes per second:
    bs.writeDWord(channels * bits_per_sample * frequency / 8);
  
    // data block size (aka block align)
    bs.writeWord(channels * bits_per_sample / 8);

    bs.writeWord(bits_per_sample);
  
    // data chunk
    bs.writeString("data");
    bs.writeDWord(size - 44);
  
    if (out.str().size()!=44) {
      cerr << "error: wav header generated by modesto is " << out.str().size() << " bytes instead of 44" << endl;
    } else {
      ::write(fd, out.str().c_str(), out.str().size());
    }

    ftruncate(fd, pos);
  }
  
  ::close(fd);
  fd = -1;
}

void
TAudioTrack::write(char *data, unsigned n)
{
  if (fd<0)
    return;
  ::write(fd, data, n);
  pos+=n;
}

unsigned
TAudioTrack::read(char *data, unsigned n)
{
#ifdef NO_AUDIO_FILE
  short *d = (short*)data;
  for(unsigned i=0; i<n; i+=2) {
    if (counter<freq) {
      *d = 32000;
    } else {
      *d = -32000;
      if (counter>=freq*2)
        counter = 0;
    }
    counter++;
    ++d;
  }
  return n;
#else
  if (fd<0 || done)
    return 0;

  while(true) {
    int r = ::read(fd, data, n);
    if (r<0) {
      if (errno==EINTR)
        continue;
      perror("read");
      done = true;
      return 0;
    }
    if (r!=n) {
      cerr << "track end" << endl;
      done = true;
    }
    pos+=r;
    return r;
  }
#endif
}

void
TAudioTrack::store(TOutObjectStream &out) const
{
  const_cast<TAudioTrack*>(this)->temporary = false;
  super::store(out);
  ::store(out, "filename", filename);
}

bool
TAudioTrack::restore(TInObjectStream &in)
{
  temporary = false;
  if (
    ::restore(in, "filename", &filename) ||
    super::restore(in)
  ) return true;
  ATV_FAILED(in);
  return false;
}
