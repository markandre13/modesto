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

#include "modesto.hh"
#include "thread.hh"

#include <stdlib.h>
#include <unistd.h>
#include <fstream>

#include <toad/toad.hh>

#include <toad/filedialog.hh>

using namespace toad;

extern void createMemoryFiles();
extern void midi();

class TResourceFile
{
    string filename;
    string comment;
  public:
  
    TFileDialog::TResource *resource;
  
    TResourceFile(const string &filename, const string &comment);
    ~TResourceFile();
};

TResourceFile::TResourceFile(const string &filename, const string &comment)
{
cerr << "reading resource file '" << filename << "'\n";
  this->filename = filename;
  this->comment  = comment;
  
  ifstream in(filename.c_str());
  if (!in) {
    cerr << "failed to open resouce file for reading\n";
    resource = new TFileDialog::TResource;
    return;
  }

#if 0
  TObjectStore os;
  os.registerObject(new TResource);
  TInObjectStream is(&in, &os);
#else
  atv::getDefaultStore().registerObject(new TFileDialog::TResource);
  TInObjectStream is(&in);
#endif
  
  TSerializable *s = is.restore();
  if (!s || !in) {
    cerr << "failed to restore the resource:\n" << is.getErrorText() << endl;
    if (s)
      delete s;
    resource = new TFileDialog::TResource;
    return;
  }
  resource = dynamic_cast<TFileDialog::TResource*>(s);
  if (!resource) {
    cerr << "resource wasn't the right type\n";
    delete s;
    resource = new TFileDialog::TResource;
  }
}

TResourceFile::~TResourceFile()
{
cerr << "writing resource file '" << filename << "'\n";
  ofstream out(filename.c_str());
  if (!out) {
    cerr << "failed to open resource file for writing\n";
    return;
  }
  
  out << "// " << comment;
  TOutObjectStream oout(&out);
  oout.store(resource);
  out << endl;
}

namespace toad {
  typedef GSTLRandomAccess<deque<string>, string> TPreviousDirs;
  extern TPreviousDirs previous_cwds;
} // namespace toad

static void
usage()
{
  cout << 
    "Modesto -- First Steps\n"
    "Copyright (C) 2004 by Mark-André Hopf <mhopf@mark13.org>\n"
    "See http://www.mark13.org/modesto/ for furthur information.\n"
    "\n"
    "Available command line options:\n"
    "  (none)\n";
}

int
main(int argc, char **argv, char **envv)
{
  TThread::initialize(true); // TThread and X11 thread support
  toad::initialize(argc, argv, envv);

//  createMemoryFiles();

  string rcfilename = getenv("HOME");
  rcfilename+="/.modestorc";

  TResourceFile rc(rcfilename, "Modesto Resource File");
  
  midi();
  
  toad::terminate();
  return 0;
}
