#include "license.hunspell"
#include "license.myspell"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "filemgr.hxx"

#ifdef HUNSPELL_CHROME_CLIENT
#include "third_party/hunspell/google/bdict_reader.h"

FileMgr::FileMgr(hunspell::LineIterator* iterator) : iterator_(iterator) {
}

FileMgr::~FileMgr() {
}

char * FileMgr::getline() {
  // Read one line from a BDICT file and store the line to our line buffer.
  // To emulate the original FileMgr::getline(), this function returns
  // the pointer to our line buffer if we can read a line without errors.
  // Otherwise, this function returns NULL.
  bool result = iterator_->AdvanceAndCopy(line_, BUFSIZE - 1);
  return result ? line_ : NULL;
}

int FileMgr::getlinenum() {
  // This function is used only for displaying a line number that causes a
  // parser error. For a BDICT file, providing a line number doesn't help
  // identifying the place where causes a parser error so much since it is a
  // binary file. So, we just return 0.
  return 0;
}
#else
int FileMgr::fail(const char * err, const char * par) {
    fprintf(stderr, err, par);
    return -1;
}

FileMgr::FileMgr(const char * file, const char * key) {
    linenum = 0;
    hin = NULL;
    fin = fopen(file, "r");
    if (!fin) {
        // check hzipped file
        char * st = (char *) malloc(strlen(file) + strlen(HZIP_EXTENSION) + 1);
        if (st) {
            strcpy(st, file);
            strcat(st, HZIP_EXTENSION);
            hin = new Hunzip(st, key);
            free(st);
        }
    }    
    if (!fin && !hin) fail(MSG_OPEN, file);
}

FileMgr::~FileMgr()
{
    if (fin) fclose(fin);
    if (hin) delete hin;
}

char * FileMgr::getline() {
    const char * l;
    linenum++;
    if (fin) return fgets(in, BUFSIZE - 1, fin);
    if (hin && ((l = hin->getline()) != NULL)) return strcpy(in, l);
    linenum--;
    return NULL;
}

int FileMgr::getlinenum() {
    return linenum;
}
#endif
