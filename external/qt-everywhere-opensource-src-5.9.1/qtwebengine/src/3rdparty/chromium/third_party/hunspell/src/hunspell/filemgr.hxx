/* file manager class - read lines of files [filename] OR [filename.hz] */
#ifndef _FILEMGR_HXX_
#define _FILEMGR_HXX_

#include "hunvisapi.h"

#include "hunzip.hxx"
#include <stdio.h>

#ifdef HUNSPELL_CHROME_CLIENT
namespace hunspell {
class LineIterator;
}  // namespace hunspell

// A class which encapsulates operations of reading a BDICT file.
// Chrome uses a BDICT file to compress hunspell dictionaries. A BDICT file is
// a binary file converted from a DIC file and an AFF file. (See
// "bdict_reader.h" for its format.)
// This class encapsulates the operations of reading a BDICT file and emulates
// the original FileMgr operations for AffixMgr so that it can read a BDICT
// file without so many changes.
class FileMgr {
 public:
  FileMgr(hunspell::LineIterator* iterator);
  ~FileMgr();
  char* getline();
  int getlinenum();

 protected:
  hunspell::LineIterator* iterator_;
  char line_[BUFSIZE + 50]; // input buffer
};
#else
class LIBHUNSPELL_DLL_EXPORTED FileMgr
{
protected:
    FILE * fin;
    Hunzip * hin;
    char in[BUFSIZE + 50]; // input buffer
    int fail(const char * err, const char * par);
    int linenum;

public:
    FileMgr(const char * filename, const char * key = NULL);
    ~FileMgr();
    char * getline();
    int getlinenum();
};
#endif
#endif
