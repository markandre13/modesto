# Modesto Makefile

.PHONY: all valgrind

#PROFILEFLAGS=-pg -fprofile-arcs

CXX=`toad-config --cxx` -mmmx
CXXFLAGS=`toad-config --cxxflags` $(PROFILEFLAGS)
LDFLAGS=`toad-config --libs`

CXXFLAGS+=-g -Wall -D_LARGEFILE64_SOURCE -D_REENTRANT
LDFLAGS+=-lpthread -lasound $(PROFILEFLAGS)
#CXXFLAGS+=-g -Wall -D_LARGEFILE64_SOURCE -D_REENTRANT
#LDFLAGS+=-ldv -lraw1394 -lavc1394 -lrom1394 -lpthread

# LDFLAGS+=-lefence

all: modesto

valgrind:
	valgrind --tool=addrcheck --num-callers=20 ./modesto
  

RESOURCE=
SRC=main.cc midi.cc arrangement.cc mididevice.cc miditrack.cc thread.cc \
		mediaeventmotor.cc audio.cc

OBJ=$(SRC:.cc=.o)

depend:
	makedepend -I. -Y $(SRC) 2> /dev/null

resource.cc: $(RESOURCE)
	$(TOADDIR)/bin/toadbin2c $(RESOURCE) > resource.cc

modesto: $(OBJ)
	$(CXX) -o modesto $(OBJ) $(LDFLAGS)

.SUFFIXES: .cc

$(OBJ): %.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f modesto $(OBJ) core*
	find . -name "*~" -exec rm {} \;
# DO NOT DELETE

main.o: modesto.hh thread.hh
midi.o: audio.hh plugin.hh mediaeventmotor.hh thread.hh arrangement.hh
midi.o: modesto.hh midi.hh mididevice.hh segmentqueue.hh miditrack.hh
midi.o: track.hh midisequence.hh audiotrack.hh
arrangement.o: arrangement.hh modesto.hh miditrack.hh track.hh
arrangement.o: midisequence.hh
mididevice.o: mididevice.hh modesto.hh segmentqueue.hh
miditrack.o: miditrack.hh track.hh modesto.hh midisequence.hh midi.hh
miditrack.o: mididevice.hh segmentqueue.hh
thread.o: thread.hh
mediaeventmotor.o: mediaeventmotor.hh thread.hh arrangement.hh modesto.hh
mediaeventmotor.o: plugin.hh
audio.o: audio.hh plugin.hh mediaeventmotor.hh thread.hh arrangement.hh
audio.o: modesto.hh audiotrack.hh track.hh
