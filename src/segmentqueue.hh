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

#ifndef _MODESTO_SEGMENTQUEUE_HH
#define _MODESTO_SEGMENTQUEUE_HH

#include <iostream>
#include <cassert>
#include <cstring>

using namespace std;

template <typename T, unsigned S>
class segmentqueue
{
    struct segment {
      segment() {
        size = 0;
        next = 0;
      }
      unsigned size;
      segment *next;
      T data[S];
    };
    segment *rdhead, *wrhead, *freelist;
    unsigned pos;


  public:
//unsigned nrd, nwr;
    segmentqueue() {
//nrd = nwr = 0;
      rdhead = wrhead = freelist = 0;
      pos = 0;
    }
    
    ~segmentqueue() {
    }
    
    /**
     * An iterator for reading.
     *
     * It is intended to be used for look-ahead operations
     * before actually retrieving elements with 
     * getReadSegment;
     */
    class iterator
    {
        friend class segmentqueue;
        segment *s;
        unsigned pos;
      public:
        iterator operator++() {
          ++pos;
          if (pos>=s->size) {
            assert(s!=0);
            s = s->next;
            if (s && s->size==0)
              s=0;
            pos = 0;
          }
          return *this;
        }
        iterator operator++(int) {
          ++pos;
          if (pos>=s->size) {
            assert(s!=0);
            s = s->next;
            pos = 0;
            if (s && s->size==0) {
              s = 0;
            }
          }
          return *this;
        }
        bool operator!=(const iterator &i) {
//cerr << "(" << s << ", " << pos << ") != (" << i.s << ", " << i.pos << ") ?\n";
          return s!=i.s || pos!=i.pos;
        }
        bool operator==(const iterator &i) {
//cerr << "(" << s << ", " << pos << ") == (" << i.s << ", " << i.pos << ") ?\n";
          return s==i.s && pos==i.pos;
        }
        T& operator*() {
          assert(s!=0);
          return s->data[pos];
        }
    };
    iterator begin() {
      iterator i;
      if (rdhead && wrhead == rdhead &&rdhead->size == pos) {
        i.s = 0;
        i.pos = 0;
      } else {
        i.s = rdhead;
        i.pos = pos;
      }
      return i;
    }
    iterator end() {
      iterator i;
      i.s = 0;
      i.pos = 0;
      return i;
    }
    
    /**
     * Get a segment for writing.
     *
     * \param p
     *   Returns pointer to the segments head.
     * \param n
     *   Returns the number of elements available in the segment.
     */
    void getWriteSegment(T **p, unsigned *n) {
      if (!wrhead) {
        rdhead = wrhead = new segment();
//cerr << "allocate first segment at " << rdhead << endl;
      } else
      if (wrhead->size == S) {
        if (freelist) {
//cerr << "freelist get" << endl;
          wrhead->next = freelist;
          wrhead = freelist;
          freelist = freelist->next;
          wrhead->next = 0;
        } else {
          wrhead->next = new segment();
          wrhead = wrhead->next;
//cerr << "allocate additional segment at " << wrhead << endl;
        }
      }
      *p = wrhead->data + wrhead->size;
      *n = S - wrhead->size;
//nwr = *n;
    }
    
    /**
     * Consume segment space for writing.
     *
     * \param n
     *   The number of elements used for writing in the
     *   last segment returned by getWriteSegment.
     */
    void consumeWrite(unsigned n) {
//if (n>nwr) {
//  cerr << "ERROR: consume " << n << " bytes but getWriteSegment returned " << nwr << endl;
//}
//nwr-=n;
//nrd+=n;
//cerr << "consumeWrite(" << n << ")\n";
      assert(wrhead!=0);
      assert(wrhead->size + n <= S);
      wrhead->size += n;
    }
    
    /**
     * Allocate a segment for reading.
     *
     * \param p
     *   Elements to be read.
     * \param n
     *   In:  Maximal number of elements to be read.
     *   Out: Available number of elements which can
     *        be read.
     */
    void getReadSegment(T **p, unsigned *n) {
      if (rdhead==0) {
        *n = 0;
        return;
      }
      assert(pos<=rdhead->size);
      if (pos == rdhead->size) {
        if (!rdhead->next) {
          *n = 0;
          return;
        }
//cerr << "freelist add" << endl;
        segment *s = rdhead;
        rdhead = rdhead->next;
        pos = 0;
        s->size = 0;
        s->next = freelist;
        freelist = s;
      }
      if (*n > rdhead->size - pos) {
        *n = rdhead->size - pos;
      }
      *p = rdhead->data + pos;
      pos+=*n;
    }

    void consumeRead(unsigned n) {
/*
if (n>nrd) {
  cerr << "ERROR: consume " << n << " bytes but available are just " << nrd << endl;
  fprintf(stderr, "end s=%p, pos=%u\n", end().s, end().pos);
  for(iterator p=begin(); p!=end(); ++p) {
    fprintf(stderr, "iterator s=%p, pos=%u\n", p.s, p.pos);
  }
  assert(false);
}
nrd-=n;
*/
//cerr << "consumeRead(" << n << ")\n";
      if (!rdhead)
        return;
/*
cerr << "consumeRead " << n << endl
     << "  pos = " << pos << endl
     << "  rdhead->size = " << rdhead->size << endl;
*/
      while(n>0) {
        if (pos+n>rdhead->size) {
          assert(rdhead->next);
          n -= rdhead->size - pos;
          segment *s = rdhead;
          rdhead = rdhead->next;
//cerr << "new rdhead " << rdhead << endl;
          pos = 0;
          s->next = freelist;
          freelist = s;
          freelist->size = 0;
        } else {
          pos += n;
          n = 0;
//cerr << "new pos " << pos << endl;
          
          if (pos==S) {
            if (rdhead->next==0) {
              assert(rdhead == wrhead);
//cerr << "read at end, no more data, reuse this segment for read & write\n";
              pos = 0;
              rdhead->size = 0;
            } else {
              segment *s = rdhead;
              rdhead = rdhead->next;
//cerr << "new rdhead " << rdhead << endl;
              pos = 0;
              s->next = freelist;
              freelist = s;
              freelist->size = 0;
            }
          }
          
        }
      }
    }
    
};

#ifdef TEST1
typedef segmentqueue<char, 4096> midibuffer;

int
main()
{
  midibuffer buffer;
  char *p;
  char c1 = 'a';
  char c2 = 'a';
  unsigned n;
  
  while(true) {
    int x = (int)(11.0*rand()/(RAND_MAX+1.0));
    if (x<5) {
      buffer.getWriteSegment(&p, &n);
cerr << "got write space of " << n << endl;
if (n>10) n=10;
      unsigned m=1+(int) (static_cast<double>(n)*rand()/(RAND_MAX+1.0));
cerr << "  using " << m << endl;
      for(unsigned i=0; i<m; ++i) {
        p[i]=c1;
        if (c1=='z') c1='a'; else c1++;
      }
      buffer.consume(m);
    } else {
#if 1
      for(midibuffer::iterator p=buffer.begin();
          p!=buffer.end();
          ++p)
      {
        cerr << *p;
      }
      cerr << endl;
#else
      unsigned m=1+(int) (10.0*rand()/(RAND_MAX+1.0));
//cerr << "want to read " << m << endl;
      buffer.getReadSegment(&p, &m);
//cerr << "  got " << m << endl;
      for(unsigned i=0; i<m; ++i) {
        assert(p[i]==c2);
        if (c2=='z') c2='a'; else c2++;
      }
#endif
    }
  }
}
#endif

#ifdef TEST2
typedef segmentqueue<char, 4> midibuffer;

int
main()
{
  midibuffer buffer;
  char *p;
  unsigned n;
  char c1 = 'a';
  
  while(true) {
    buffer.getWriteSegment(&p, &n);
cerr << "got write space of " << n << endl;
    p[0]=c1;
    if (c1=='z') c1='a'; else c1++;
    buffer.consumeWrite(1);
    
    midibuffer::iterator p, e;
    p = buffer.begin();
    e = buffer.end();
    while(p!=e) {
      cerr << "read '" << *p << "'\n";
      ++p;
    }
    
    buffer.consumeRead(1);
  }
}
#endif

#ifdef TEST3
typedef segmentqueue<char, 3> midibuffer;

int
main()
{
  midibuffer buffer;
  char *p;
  unsigned n;
  char c1 = 'a';
  
  while(true) {
    buffer.getWriteSegment(&p, &n);
cerr << "---------------------------------------------------------\n" 
     << "got write space of " << n << endl;
    p[0]=c1;
    if (c1=='z') c1='a'; else c1++;
    if (n>1) {
      p[1]=c1;
      buffer.consumeWrite(2);
    } else {
      buffer.consumeWrite(1);
      buffer.getWriteSegment(&p, &n);
      p[0]=c1;
      buffer.consumeWrite(1);
    }
    if (c1=='z') c1='a'; else c1++;
    
    midibuffer::iterator p, e;
    p = buffer.begin();
    e = buffer.end();
    while(p!=e) {
      cerr << "read '" << *p << "'\n";
      ++p;
    }
    
    buffer.consumeRead(2);
  }
}
#endif

#endif
