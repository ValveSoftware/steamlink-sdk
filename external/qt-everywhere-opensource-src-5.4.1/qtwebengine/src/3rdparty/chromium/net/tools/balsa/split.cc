// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/balsa/split.h"

#include <string.h>

#include <vector>

#include "base/strings/string_piece.h"

namespace net {

// Yea, this could be done with less code duplication using
// template magic, I know.
void SplitStringPieceToVector(const base::StringPiece& full,
                              const char* delim,
                              std::vector<base::StringPiece>* vec,
                              bool omit_empty_strings) {
  vec->clear();
  if (full.empty() || delim[0] == '\0')
    return;

  if (delim[1] == '\0') {
    base::StringPiece::const_iterator s = full.begin();
    base::StringPiece::const_iterator e = s;
    for (;e != full.end(); ++e) {
      if (*e == delim[0]) {
        if (e != s || !omit_empty_strings) {
          vec->push_back(base::StringPiece(s, e - s));
        }
        s = e;
        ++s;
      }
    }
    if (s != e) {
      --e;
      if (e != s || !omit_empty_strings) {
        vec->push_back(base::StringPiece(s, e - s));
      }
    }
  } else {
    base::StringPiece::const_iterator s = full.begin();
    base::StringPiece::const_iterator e = s;
    for (;e != full.end(); ++e) {
      bool one_matched = false;
      for (const char *d = delim; *d != '\0'; ++d) {
        if (*d == *e) {
          one_matched = true;
          break;
        }
      }
      if (one_matched) {
        if (e != s || !omit_empty_strings) {
          vec->push_back(base::StringPiece(s, e - s));
        }
        s = e;
        ++s;
      }
    }
    if (s != e) {
      --e;
      if (e != s || !omit_empty_strings) {
        vec->push_back(base::StringPiece(s, e - s));
      }
    }
  }
}

}  // namespace net

