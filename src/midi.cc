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

#warning "/home/mark/midi/MartinGalway/RastanSaga.mid is played too slow!"

#include "audio.hh"

//#define USE_POLL

/*
  miditime_t: Modesto's time resolution in microseconds
  
  TTimeline
    TArrangement
      TMIDITrack, TAudioTrack, TVideoTrack, ...
        TMIDISequence, TAudioSequence, TVideoSequence, ...

  TSequenceFigure
*/

#include "midi.hh"
#include "arrangement.hh"
#include "miditrack.hh"
#include "audiotrack.hh"
#include "mididevice.hh"
#include "midisequence.hh"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cassert>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdexcept>

#include <toad/toad.hh>
#include <toad/dialog.hh>
#include <toad/pushbutton.hh>
#include <toad/fatradiobutton.hh>
#include <toad/figure.hh>
#include <toad/figureeditor.hh>
#include <toad/form.hh>
#include <toad/menubar.hh>
#include <toad/action.hh>
#include <toad/htmlview.hh>
#include <toad/popupmenu.hh>
#include <toad/filedialog.hh>
#include <toad/textfield.hh>
#include <toad/combobox.hh>
#include <toad/tablemodels.hh>
#include <toad/popupmenu.hh>
#include <toad/io/serializable.hh>
#include <toad/io/binstream.hh>

#include "modesto.hh"

FILE *logfile = 0;
#define LOG(CMD) if (logfile) { CMD } fflush(logfile);

// using namespace std;
using namespace toad;

typedef unsigned char byte;

unsigned midicmdlen[16] = {
  0,  // 0x
  0,  // 1x
  0,  // 2x
  0,  // 3x
  0,  // 4x
  0,  // 5x
  0,  // 6x
  0,  // 7x
  2,  // 8x NOTE OFF
  2,  // 9x NOTE ON
  2,  // Ax POLYPHONIC KEY PRESSURE (ie. aftertouch per key)
  2,  // Bx CONTROL CHANGE
  1,  // Cx PROGRAM CHANGE
  1,  // Dx CHANNEL PRESSURE (ie. aftertouch per midi channel)
  2,  // Ex PITCH BENDER
  0   // Fx CHANNEL MODE MESSAGES
};

miditime_t
TMIDISequence::getLength()
{
  miditime_t value;
  byte c;

  if ( (value = getByte()) & 0x80 ) {
    value &= 0x7F;
    do {
      value = (value << 7) + ((c = getByte()) & 0x7F);
    } while (c & 0x80);
  }

  return value;
}

TMIDIDeviceStorage midiinlist;
TMIDIDeviceStorage midioutlist;

class TMySelection:
  public TAbstractTableSelectionModel
{
    vector<TMIDIDevice*> *container;
    TMIDIDevice **ptr;

  public:
    TMySelection(vector<TMIDIDevice*> *container, TMIDIDevice **ptr) {
//cerr << "new TMySelection for " << ptr << endl;
      this->container = container;
      this->ptr       = ptr;
    }
    virtual void setSelection(int col, int row) {
//cerr << "TMySelection::setSelection("<<col<<", "<<row<<")\n";
      *ptr = (*container)[row];
      sigChanged();
    }
    virtual bool isSelected(int col, int row) const {
//cerr << "TMySelection::isSelected("<<col<<", "<<row<<") for " << ptr << "\n";
      return *ptr == (*container)[row];
    }
};

class TMyRenderer:
  public TAbstractTableCellRenderer
{
    vector<TMIDIDevice*> *container;

  public:
    TMyRenderer(vector<TMIDIDevice*> *container) {
      this->container = container;
    }

    virtual int getRows() { 
      return container->size(); 
    }
    virtual int getRowHeight(int row) { return 18; }
    virtual int getColWidth(int col) { return 150; }
    virtual void renderItem(TPen &pen,
                            int col, int row,
                            int w, int h,
                            bool cursor, bool selected, bool focus)
    {
      pen.drawString(2, 2, (*container)[row]->name);
    }
};

/**
 * This class represents a MIDI Sequence in the TTimeline window.
 */
class TSequenceFigure:
  public TFRectangle
{
    TMIDISequence *seq;
  public:
    TSequenceFigure(int x, int y, int h, int w) {
      setShape(x, y, h, w);
      setFillColor(TRGB(226, 145, 145));
    }
    void setSequence(TMIDISequence *s) {
      seq = s;
      unsigned seconds;
      seconds = seq->getStart() / 1000000UL;
      p1.x = seconds;
      seconds = seq->getDuration() / 1000000UL;
      p2.x = p1.x+ seconds*8 - 1;
    }
    void paint(TPenBase& pen, EPaintType type) {
      TFRectangle::paint(pen, type);
      pen.setFont("sans-serif:size=10");
      pen.drawString(p1.x+2, p1.y+2, seq->getName());
    }
};

class TMIDIPlugIn:
  public TPlugIn
{
  public:
    void registerFDs(int *maxfd, fd_set *rd, fd_set *wr, fd_set *ex);
    void read(TMediaEventMotor::EState state, fd_set *rd);
    TMediaEventMotor::EState stateChange(TMediaEventMotor::EState state);
    TMediaEventMotor::EState prepare(TMediaEventMotor::EState state);
    TMediaEventMotor::EState execute(TMediaEventMotor::EState state, miditime_t now);
  protected:
    TMIDITrack *nexttrack;
};

#ifdef USE_POLL
unsigned
TMIDIPlugIn::registerFDs(pollfd *pf)
{
  unsigned count = 0;

  for(unsigned i=0; i<midiinlist.size(); ++i) {
    int fd = midiinlist[i]->fd;
    if (fd!=-1) {
      pf->fd = fd;
      pf->events = POLLIN;
      ++count;
    }
  }
  return count;
}
#else
void
TMIDIPlugIn::registerFDs(int *maxfd, fd_set *rd, fd_set *wr, fd_set *ex)
{
  for(unsigned i=0; i<midiinlist.size(); ++i) {
    int fd = midiinlist[i]->fd;
    if (fd!=-1) {
      FD_SET(fd, rd);
      if (fd>*maxfd)
        *maxfd = fd;
    }
  }
}
#endif

void
TMIDIPlugIn::read(TMediaEventMotor::EState state, fd_set *rd)
{      
  // get all incoming data
  for(unsigned i=0; i<midiinlist.size(); ++i) {
    int fd = midiinlist[i]->fd;
    if (fd==-1)
      continue;
    if (FD_ISSET(fd, rd)) {
      midiinlist[i]->read();
    }
  }
}


/**
 * Adjust to a new state.
 *
 * Iterates of all tracks of it's arrangement and prepares all
 * MIDI tracks it finds.
 */
TMediaEventMotor::EState
TMIDIPlugIn::stateChange(TMediaEventMotor::EState state)
{
  assert(arrangement!=0);

  // silence MIDI channels
#if 0      
  static const byte reset[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
#else      
  static const byte reset[] = {0xB0, 0x7B, 0x00,
                               0xB1, 0x7B, 0x00,
                               0xB2, 0x7B, 0x00,
                               0xB3, 0x7B, 0x00,
                               0xB4, 0x7B, 0x00,
                               0xB5, 0x7B, 0x00,
                               0xB6, 0x7B, 0x00,
                               0xB7, 0x7B, 0x00,
                               0xB8, 0x7B, 0x00,
                               0xB9, 0x7B, 0x00,
                               0xBA, 0x7B, 0x00,
                               0xBB, 0x7B, 0x00,
                               0xBC, 0x7B, 0x00,
                               0xBD, 0x7B, 0x00,
                               0xBE, 0x7B, 0x00,
                                0xBF, 0x7B, 0x00 };
#endif      
  for(unsigned i=0; i<midioutlist.size(); ++i)
    midioutlist[i]->write(reset, sizeof(reset));

  // adjust to new state
  if (state==TMediaEventMotor::STATE_PLAY ||
      state==TMediaEventMotor::STATE_RECORD) 
  {
    next = UINT_MAX;
    // initialize track players (ups, these should be ours!)
    for(unsigned i=0; i<arrangement->tp.size(); ++i) {
      TMIDITrack *track = dynamic_cast<TMIDITrack*>(arrangement->tp[i]);
      if (!track)
        continue;
    
      if (state==TMediaEventMotor::STATE_PLAY) {
        track->seekStart();
        track->nextEvent();
      } else
      if (state==TMediaEventMotor::STATE_RECORD) {
        if (!track->record) {
          track->seekStart();
          track->nextEvent();
        } else {
          track->clear();
        }
      }
    }
  }
  return prepare(state);
}

/**
 * Prepare for the next event and set the plugins 'next' attribute for
 * the TMediaEventMotor
 */
TMediaEventMotor::EState
TMIDIPlugIn::prepare(TMediaEventMotor::EState state)
{
  if (state==TMediaEventMotor::STATE_PLAY ||
      state==TMediaEventMotor::STATE_RECORD) 
  {
    if (next==UINT_MAX) {
      // find the next event to play in our track player list
      nexttrack = 0;
      for(unsigned i=0; i<arrangement->tp.size(); ++i) {
        TMIDITrack *track = dynamic_cast<TMIDITrack*>(arrangement->tp[i]);
        if (!track || track->record)
          continue;
        if (track->ticks<next) {
          nexttrack = track;
          next = track->ticks;
        }
      }
    }
    if (next==UINT_MAX) {
      if (state==TMediaEventMotor::STATE_RECORD) {
        return state;
      }
//      cerr << "midi plugin: no more events, switching to STATE_STOP\n";
      return TMediaEventMotor::STATE_STOP;
    }
  }
  return state;
}

/**
 * Play pre-recorded track data
 */
TMediaEventMotor::EState
TMIDIPlugIn::execute(TMediaEventMotor::EState state, miditime_t now)
{
  if (state==TMediaEventMotor::STATE_PLAY ||
      state==TMediaEventMotor::STATE_RECORD ) 
  {
    if (next<=now) {
      if (nexttrack && !nexttrack->record) {
        nexttrack->play();
        nexttrack->nextEvent();
        next = UINT_MAX;
      }
    }
  }

  for(unsigned i=0; i<midiinlist.size(); ++i) {
    while(true) {
      byte buffer[512];
      int n = midiinlist[i]->read(buffer, sizeof(buffer));
      if (n==0) {
        break;
      }

      if (!arrangement) {
        continue;
      }

      // skip MIDI time events
      if (buffer[0]==0xFE)
        continue;
      
      LOG(
        for(int i=0; i<n; ++i) {
          fprintf(logfile, "%02X ", buffer[i]);
        }
        fprintf(logfile, "\n");
      )

      // handle monitor & record tracks
      for(unsigned i=0; i<arrangement->tp.size(); ++i) {
        TMIDITrack *track = dynamic_cast<TMIDITrack*>(arrangement->tp[i]);
        if (!track)
          continue;

        // map event from input channel to the tracks output channel
        if (buffer[0] & 0x80) {
          buffer[0] = (buffer[0] & 0xF0) | (track->channel - 1);
        }

        if (track->monitor) {
          track->out->write(buffer, n);
        }
        
        if (state==TMediaEventMotor::STATE_RECORD && 
            track->record )
        {
          track->write(now, buffer, n);
        }
      }
    }
  }
  return prepare(state);
}


class TTimeline:
  public TFigureEditor
{
    TArrangement *arrangement;
  public:
    TTimeline(TWindow *parent, const string &title);
    void setArrangement(TArrangement *a);
    TArrangement* getArrangement() const {
      return arrangement;
    }
    void updateArrangement();
};

/**
 * The control panel on the left inside TTimeline, which allows to
 * mute, add, erase, move and name tracks.
 */
class TTrackController:
  public TFigureEditorHeaderRenderer
{
    int width;
    TTimeline *timeline;
  public:
    TTrackController(TTimeline *t);
    
  protected:
    void render(TPen &pen, int pos, int size, TMatrix2D *mat);
    int getSize() { return width; }
    
    void mouseEvent(TMouseEvent &);
};

TTimeline::TTimeline(TWindow *parent, const string &title):
  TFigureEditor(parent, title)
{
  arrangement = 0;
  setRowHeaderRenderer(new TTrackController(this));
}

void
TTimeline::setArrangement(TArrangement *a)
{
  if (arrangement) {
    disconnect(arrangement->sigChanged, this);
  }
  arrangement = a;
  if (arrangement)
    connect(arrangement->sigChanged, this, &TTimeline::updateArrangement);
  updateArrangement();
}

void
TTimeline::updateArrangement()
{
  if (!arrangement)
    return;

  switch(arrangement->reason) {
    case TArrangement::TRACK_REMOVED:
    case TArrangement::TRACK_ADDED: {
      deleteAll(); // delete all figures from a previous track
#if 0
      int y = 2;
      for(TArrangement::TTrackVector::iterator p = arrangement->tp.begin();
           p != arrangement->tp.end();
           ++p)
      {
        TSequenceFigure *r = new TSequenceFigure(2, y, 80, 18);
        r->setSequence((*p)->getFirstSequence());
        addFigure(r);
        y+=20;
      }
#endif
    } break;
    default:
      ;
  }
  invalidateWindow();
}

TTrackController::TTrackController(TTimeline *t)
{
  timeline = t;
  width = 148;
}

void
TTrackController::render(TPen &pen, int pos, int size, TMatrix2D *mat)
{
//cerr << __PRETTY_FUNCTION__ << endl;
  pen.setColor(TColor::BTNFACE);
  pen.fillRectangle(0, pos, width, size);

  if (!timeline->getArrangement())
    return;

  pen.setFont("sans-serif:size=10");
  
  pen.setColor(TColor::BTNTEXT);

  TArrangement::TTrackVector::iterator p, e;
  p = timeline->getArrangement()->tp.begin();
  e = timeline->getArrangement()->tp.end();
  unsigned y = 2;
  while(p!=e) {
    if( (*p)->mute ) {
      pen.setColor(TColor::SLIDER_FACE);
      pen.fillRectangle(2      , y+2, 15, 16);
      pen.setColor(TColor::BTNTEXT);
    }
    pen.draw3DRectangle(2      , y+2, 15, 16, (*p)->mute);
    pen.drawString     (5      , y+3, "M");

    if( (*p)->solo ) {
      pen.setColor(245, 105, 105 /*TColor::SLIDER_FACE*/);
      pen.fillRectangle(2+20   , y+2, 15, 16);
      pen.setColor(TColor::BTNTEXT);
    }
    pen.draw3DRectangle(2+20   , y+2, 15, 16, (*p)->solo);
    pen.drawString     (5+20   , y+3, "S");

    if (*p == timeline->getArrangement()->getSelectedTrack()) {
      pen.setColor(TColor::SLIDER_FACE);
      pen.fillRectangle(40, y+1, 46, 18);
      pen.setColor(TColor::BTNTEXT);
    }
    pen.drawString     (2+20+20, y+3, (*p)->getName());

    if( (*p)->record ) {
      pen.setColor(226, 145, 145);
      pen.fillRectangle(90      , y+2, 15, 16);
      pen.setColor(TColor::BTNTEXT);
    }
    pen.draw3DRectangle(90      , y+2, 15, 16, (*p)->record);
    pen.drawString     (93      , y+3, "O");

    if( (*p)->monitor ) {
      pen.setColor(TColor::SLIDER_FACE);
      pen.fillRectangle(90+20     , y+2, 15, 16);
      pen.setColor(TColor::BTNTEXT);
    }
    pen.draw3DRectangle(90+20   , y+2, 15, 16, (*p)->monitor);
    pen.drawString     (93+20   , y+3, "<");

    if( (*p)->lock ) {
      pen.setColor(TColor::SLIDER_FACE);
      pen.fillRectangle(90+20+20  , y+2, 15, 16);
      pen.setColor(TColor::BTNTEXT);
    }
    pen.draw3DRectangle(90+20+20, y+2, 15, 16, (*p)->lock);
    pen.drawString     (93+20+20, y+3, "L");
    y+=20;
    ++p;
  }
}

void
TTrackController::mouseEvent(TMouseEvent &me)
{
//  cerr << "TTrackController: mouse event at " << me.x << ", " << me.y << endl;

  TArrangement::TTrackVector::iterator p, e;
  p = timeline->getArrangement()->tp.begin();
  e = timeline->getArrangement()->tp.end();
  unsigned y = 2;
  
  TRectangle r;
  
  while(p!=e) {
    r.set(0,y+1,148,18);
    if (r.isInside(me.x, me.y)) {
      r.set(2      , y+2, 15, 16);
      if (r.isInside(me.x, me.y)) {
        (*p)->mute ^= true;
        break;
      }
      r.set(2+20   , y+2, 15, 16);
      if (r.isInside(me.x, me.y)) {
        (*p)->solo ^= true;
        break;
      }
      r.set(90     , y+2, 15, 16);
      if (r.isInside(me.x, me.y)) {
        (*p)->record ^= true;
        break;
      }
      r.set(90+20  , y+2, 15, 16);
      if (r.isInside(me.x, me.y)) {
        (*p)->monitor ^= true;
        break;
      }
      r.set(90+20+20, y+2, 15, 16);
      if (r.isInside(me.x, me.y)) {
        (*p)->lock ^= true;
        break;
      }
      cerr << "selected track '" <<  (*p)->getName() << "'\n";
      timeline->getArrangement()->selectTrack(*p);
      break;
    }
    y+=20;
    ++p;
  }
  if (p!=e) {
    me.window->invalidateWindow(r);
  }
}

/**
 * The transport panel provides an UI to control the TMediaEventMotor.
 */
class TTransportPanel:
  public TDialog
{
    typedef GRadioStateModel<TMediaEventMotor::EState> TCommandState;
    TCommandState *state;
    TMediaEventMotor *mev;
    
  public:
    TTransportPanel(TWindow *parent, const string &title);
    
    void setMotor(TMediaEventMotor *mev) {
      this->mev = mev;
    }
  protected:
    void stateChange(TMediaEventMotor::EState);
};

TTransportPanel::TTransportPanel(TWindow *parent, const string &title):
  TDialog(parent, title)
{
  mev = 0;

  state = new TCommandState();
  state->setValue(TMediaEventMotor::STATE_STOP);

  state->add(new TFatRadioButton(this, "Prev"), TMediaEventMotor::STATE_PREV)
    ->loadBitmap(RESOURCE("icon_prev_frame.png"));

  state->add(new TFatRadioButton(this, "Rewind"), TMediaEventMotor::STATE_REWIND)
    ->loadBitmap(RESOURCE("icon_prev_track.png"));

  state->add(new TFatRadioButton(this, "Forward"), TMediaEventMotor::STATE_FORWARD)
    ->loadBitmap(RESOURCE("icon_next_track.png"));

  state->add(new TFatRadioButton(this, "Next"), TMediaEventMotor::STATE_NEXT)
    ->loadBitmap(RESOURCE("icon_next_frame.png"));

  new TPushButton(this, "Cycle");
//    ->loadBitmap(RESOURCE("icon_stop.png"));

  state->add(new TFatRadioButton(this, "Stop"), TMediaEventMotor::STATE_STOP)
    ->loadBitmap(RESOURCE("icon_stop.png"));

  state->add(new TFatRadioButton(this, "Record"), TMediaEventMotor::STATE_RECORD)
    ->loadBitmap(RESOURCE("icon_record.png"));

  state->add(new TFatRadioButton(this, "Play"), TMediaEventMotor::STATE_PLAY)
    ->loadBitmap(RESOURCE("icon_play.png"));

  CONNECT_VALUE(state->sigChanged,
                this, stateChange,
                state);

  loadLayout(RESOURCE("TransportPanel.atv"));
}

void
TTransportPanel::stateChange(TMediaEventMotor::EState state)
{
//  cerr << "new state is :" << state << endl;
  if (mev)
    mev->setState(state);
}

class TTrackEditor:
  public TWindow
{
    TArrangement *arrangement;
/*
    TMIDITrack *track;
    TTextField *trackname, *volume, *pan, *channel, *bank, *program;
    TComboBox *cb_in, *cb_out;
*/    
    TWindow *current;
  public:
    TTrackEditor(TWindow *parent, const string &title);
    void setArrangement(TArrangement *a);
  protected:
    void updateTrack();
};
    


TTrackEditor::TTrackEditor(TWindow *parent, const string &title):
  TWindow(parent, title)
{
  arrangement = 0;
  current = 0;
//  track = 0;

  setBackground(TColor::DIALOG);
#if 0
  trackname = new TTextField(this, "trackname");
  volume = new TTextField(this, "volume");
  pan = new TTextField(this, "pan");
  cb_in = new TComboBox(this, "in");
  cb_in->setRenderer(
//    new GTableCellRenderer_Text<TMIDIDeviceStorage, 1>(&midiinlist)
    new TMyRenderer(&midiinlist)
  );
//  cb->getSelectionModel()->setSelection(0,0);
  cb_out = new TComboBox(this, "out");
  cb_out->setRenderer(
//    new GTableCellRenderer_Text<TMIDIDeviceStorage, 1>(&midioutlist)
    new TMyRenderer(&midioutlist)
  );
  channel = new TTextField(this, "channel");
  bank = new TTextField(this, "bank");
  program = new TTextField(this, "program");
  
  updateArrangement();

  /*midimap =*/ new TTextField(this, "map");

  loadLayout(RESOURCE("TTrackEditor.atv"));  
#endif
}

void
TTrackEditor::setArrangement(TArrangement *a)
{
  if (arrangement) {
    disconnect(arrangement->sigChanged, this);
  }
  arrangement = 0;
  updateTrack();
  arrangement = a;
  if (arrangement) {
    connect(arrangement->sigChanged, this, &TTrackEditor::updateTrack);
  }
  updateTrack();
}

void
TTrackEditor::updateTrack()
{
  if (current) {
    current->setMapped(false);
  }
  if (!arrangement)
    return;
  TTrack *t = arrangement->getSelectedTrack();
  if (t) {
    current = t->getEditor(this);
    if (current) {
      if (!current->isRealized()) {
        current->setBorder(0);
        current->createWindow();
      } else {
        current->setMapped(true);
      }
      setSize(current->getWidth(), getHeight());
    }
  }
/*
  if (!arrangement) {
    cerr << "  no arrangement" << endl;
    trackname->setModel(0);
    volume->setModel(0);
    pan->setModel(0);
    cb_in->setSelectionModel(0);
    cb_out->setSelectionModel(0);
    channel->setModel(0);
    bank->setModel(0);
    program->setModel(0);
    return;
  }
  switch(arrangement->reason) {
    case TArrangement::TRACK_REMOVED:
    case TArrangement::TRACK_SELECTED: {
      cerr << "track editor: track selected\n";
      TTrack *t = arrangement->getSelectedTrack();
      if (t) {
        trackname->setModel(&t->name);
      } else {
        trackname->setModel(0);
      }

      track = dynamic_cast<TMIDITrack*>(t);
      if (track) {
        volume->setModel(&track->volume);
        pan->setModel(&track->pan);
        cb_in->setSelectionModel(new TMySelection(&midiinlist, &track->in));
        cb_out->setSelectionModel(new TMySelection(&midioutlist, &track->out));
        channel->setModel(&track->channel);
        bank->setModel(&track->bank);
        program->setModel(&track->program);
      } else {
        cerr << __PRETTY_FUNCTION__ << ": track=0" << endl;
        volume->setModel(0);
        pan->setModel(0);
        cb_in->setSelectionModel(0);
        cb_out->setSelectionModel(0);
        channel->setModel(0);
        bank->setModel(0);
        program->setModel(0);
      }
      } break;
    default:
      cerr << "  reason " << arrangement->reason << endl;
      ;
  }
*/
}

class TMainWindow:
  public TForm
{
    TTimeline *timeline;
    TTrackEditor *trackeditor;
    TArrangement *arrangement;
  public:
    TMainWindow(TWindow *parent, const string &title);
    void setArrangement(TArrangement *arrangement) {
      TArrangement *old = this->arrangement;
      cerr << __PRETTY_FUNCTION__ << endl;
      this->arrangement = arrangement;
      timeline->setArrangement(arrangement);
      trackeditor->setArrangement(arrangement);
      if (old)
        delete old;
    }
    TArrangement* getArrangement() const { return arrangement; }
  protected:
    void menuOpen();
    void menuSave();
    void menuSaveAs();
    void menuAbout();
    void menuExit();
};

TMainWindow::TMainWindow(TWindow *parent, const string &title):
  TForm(parent, title)
{
  arrangement = 0;

  TMenuBar *menubar = new TMenuBar(this, "menubar");
  menubar->loadLayout(RESOURCE("menubar.atv"));

  timeline = new TTimeline(this, "timeline");

  trackeditor = new TTrackEditor(this, "trackeditor");

  attach(menubar, TOP|LEFT|RIGHT);
  attach(trackeditor, TOP, menubar);
  attach(trackeditor, LEFT|BOTTOM);
  attach(timeline, LEFT, trackeditor);
  attach(timeline, TOP, menubar);
  attach(timeline, RIGHT|BOTTOM);
  
  TAction *action;
  
  action = new TAction(this, "file|open");
  connect(action->sigActivate, this, &TMainWindow::menuOpen);

  action = new TAction(this, "file|save");
  connect(action->sigActivate, this, &TMainWindow::menuSave);

  action = new TAction(this, "file|save_as");
  connect(action->sigActivate, this, &TMainWindow::menuSaveAs);

  action = new TAction(this, "help|about");
  connect(action->sigActivate, this, &TMainWindow::menuAbout);

  action = new TAction(this, "file|exit");
  connect(action->sigActivate, this, &TMainWindow::menuExit);
  
  action = new TAction(this, "edit|new_track|midi");
  TCLOSURE1(action->sigActivate, wnd, this,
    if (wnd->getArrangement())
      wnd->getArrangement()->newTrack();
  )

  action = new TAction(this, "edit|new_track|audio");
  TCLOSURE1(action->sigActivate, wnd, this,
    if (wnd->getArrangement())
      wnd->getArrangement()->addTrack(new TAudioTrack());
  )
  
  action = new TAction(this, "edit|new_track|video");
  
  action = new TAction(this, "edit|delete_track");
  TCLOSURE1(action->sigActivate, wnd, this,
    if (wnd->getArrangement())
      wnd->getArrangement()->deleteTrack();
  )
}

TMediaEventMotor *motor;
TTransportPanel *panel;

void
TMainWindow::menuOpen()
{
  assert(arrangement!=NULL);
  
  if (arrangement->modified) {
    switch(messageBox(this, 
                     "Warning: Open Arrangement",
                     "The current arrangement has been modified.\n"
                     "Would you like to save it?",
                     TMessageBox::YESNOCANCEL|
                     TMessageBox::ICON_HAND))
    {
      case TMessageBox::CANCEL:
        return;
      case TMessageBox::YES:
        menuSave();
        if (arrangement->modified)
          return;
        break;
      case TMessageBox::NO:
        break;
      default:
        return;
    }
  }
  
  TFileDialog dlg(this, "Open File");
  dlg.addFileFilter("Any (*.moo, *.mid)");
  dlg.addFileFilter("Modesto (*.moo)");
  dlg.addFileFilter("MIDI (*.mid)");
//  dlg.addFileFilter("Audio (*.wav, *.ogg, *.mp3)");
//  dlg.addFileFilter("Video (*.dv)");
  dlg.doModalLoop();
  if (dlg.getResult()==TMessageBox::OK) {
    ifstream in(dlg.getFilename().c_str());
    if (!in) {
      messageBox(this, 
                 "Open File Failed",
                 "I've failed to open the specified file.",
                 TMessageBox::ICON_EXCLAMATION|
                 TMessageBox::OK);
      return;
    }

    string filename = dlg.getFilename();

    if (filename.size()>4 && 
        filename.compare(filename.size()-4, 4, ".moo")==0) 
    {
      TObjectStore store;
      store.registerObject(new TArrangement());
      store.registerObject(new TMIDITrack());
      store.registerObject(new TAudioTrack());
      TInObjectStream is(&in, &store);
      TSerializable *obj = is.restore();
      TArrangement *a = obj ? dynamic_cast<TArrangement*>(obj) : 0;
      if (!a) {
        string msg = "No valid arrangement found:\n" + is.getErrorText();
        messageBox(this, 
                   "Open File Failed",
                   msg,
                   TMessageBox::ICON_EXCLAMATION|
                   TMessageBox::OK);
        if (obj)
          delete obj;
      } else {
        cerr << "got arrangement " << a << endl;
        motor->setArrangement(a);
        setArrangement(a);
      }
      store.unregisterAll();
    } else {
      TArrangement *a = new TArrangement();
      a->load(in);
      motor->setArrangement(a);
      setArrangement(a);
    }
  }
}

void
TMainWindow::menuSave()
{
  assert(arrangement!=NULL);
  if (arrangement->filename.empty()) {
    menuSaveAs();
    return;
  }

  ofstream out(arrangement->filename.c_str());
  if (!out) {
    string str = "I've failed to open the file \"" + arrangement->filename + "\".";
    messageBox(this, 
               "Save Arrangement Failed",
               str,
               TMessageBox::ICON_EXCLAMATION|
               TMessageBox::OK);
  } else {
    out << "// Modesto Arrangement File" << endl
        << "// http://www.mark13.org/modesto/" << endl
        << "// Please keep the first 4 byte of this file for Modesto." << endl;
    TOutObjectStream os(&out);
    os.store(arrangement);
    arrangement->modified = false;
  }
}

void
TMainWindow::menuSaveAs()
{
  assert(arrangement!=NULL);
  TFileDialog dlg(this, "Save Arrangement", TFileDialog::MODE_SAVE);
  dlg.addFileFilter("Modesto (*.moo)");
//  dlg.addFileFilter("MIDI (*.mid)");
//  dlg.addFileFilter("Audio (*.wav, *.ogg, *.mp3)");
//  dlg.addFileFilter("Video (*.dv)");
  dlg.doModalLoop();
  if (dlg.getResult()==TMessageBox::OK) {
    if (dlg.getFilename().empty()) {
      messageBox(this,
                 "Save Arrangement Failed",
                 "No filename was specified.",
                 TMessageBox::ICON_EXCLAMATION|
                 TMessageBox::OK);
    } else {
      arrangement->filename = dlg.getFilename();
      menuSave();
    }
  }
}

void
TMainWindow::menuAbout()
{
  cerr << "about" << endl;
  THTMLView *htmlview = new THTMLView(0, "About Modesto");
  TPopupMenu *menu = new TPopupMenu(htmlview, "popupmenu");
  menu->addFilter();
  htmlview->open(RESOURCE("index.html"));
  htmlview->createWindow();
}

void
TMainWindow::menuExit()
{
  assert(arrangement!=NULL);
  
  if (arrangement->modified) {
    switch(messageBox(this, 
                     "Warning: Exit Modesto",
                     "The current arrangement has been modified.\n"
                     "Would you like to save it?",
                     TMessageBox::YESNOCANCEL|
                     TMessageBox::ICON_HAND))
    {
      case TMessageBox::CANCEL:
        return;
      case TMessageBox::YES:
        menuSave();
        if (arrangement->modified)
          return;
        break;
      case TMessageBox::NO:
        break;
      default:
        return;
    }
  }
  postQuitMessage(EXIT_SUCCESS);
}

void
midi()
{
// logfile = fopen("/tmp/log", "w");

TMIDIDevice *md;

md = new TMIDIDevice("Not Connected", "");
midiinlist.push_back(md);
midioutlist.push_back(md);

md = new TMIDIDevice("TG 100 (midi0)", "/dev/midi");
midiinlist.push_back(md);
midioutlist.push_back(md);
/*
md = new TMIDIDevice("TG 100 (ttyS0)", "/dev/ttyS0");
midiinlist.push_back(md);
midioutlist.push_back(md);
*/

midiinlist.push_back(new TMIDIDevice("All MIDI Inputs", "*"));

  // we're currently using one arrangement, one timeline/main window
  // and one transport panel.
  // in the future we're going to support multiple ones.
  TArrangement *arr = new TArrangement;
/*
  TMIDITrack *track;
  
  for(int i=1; i<=16; ++i) {
    track = new TMIDITrack(new TMIDISequence());
    track->channel = i;
    arr->addTrack(track);
  }
*/
  TMainWindow wnd(0, "Modesto");
  wnd.setSize(800, 480);
  wnd.setArrangement(arr);
  motor = new TMediaEventMotor;
  motor->setArrangement(arr);
  motor->addPlugIn(new TMIDIPlugIn);
  motor->addPlugIn(new TAudioPlugIn);
  motor->start();

  panel = new TTransportPanel(&wnd, "Modesto Transport Panel");
  panel->setMotor(motor);

  toad::mainLoop();

  motor->stop();
  panel->setMotor(0);
  wnd.setArrangement(0);
}
