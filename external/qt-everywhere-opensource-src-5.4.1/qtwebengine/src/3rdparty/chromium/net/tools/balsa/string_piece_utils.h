// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_BALSA_STRING_PIECE_UTILS_H_
#define NET_TOOLS_BALSA_STRING_PIECE_UTILS_H_

#include <ctype.h>

#include "base/port.h"
#include "base/strings/string_piece.h"

namespace net {

#if defined(COMPILER_MSVC)
struct StringPieceCaseCompare {
  static const size_t bucket_size = 4;

  size_t operator()(const base::StringPiece& sp) const {
    // based on __stl_string_hash in http://www.sgi.com/tech/stl/string
    size_t hash_val = 0;
    for (base::StringPiece::const_iterator it = sp.begin();
         it != sp.end(); ++it) {
      hash_val = 5 * hash_val + tolower(*it);
    }
    return hash_val;
  }

  bool operator()(const base::StringPiece& sp1,
                  const base::StringPiece& sp2) const {
    size_t len1 = sp1.length();
    size_t len2 = sp2.length();
    bool sp1_shorter = len1 < len2;
    size_t len = sp1_shorter ? len1 : len2;
    int rv = _memicmp(sp1.data(), sp2.data(), len);
    if (rv == 0) {
      return sp1_shorter;
    }
    return rv < 0;
  }
};
#else  // COMPILER_MSVC
struct StringPieceCaseHash {
  size_t operator()(const base::StringPiece& sp) const {
    // based on __stl_string_hash in http://www.sgi.com/tech/stl/string
    size_t hash_val = 0;
    for (base::StringPiece::const_iterator it = sp.begin();
         it != sp.end(); ++it) {
      hash_val = 5 * hash_val + tolower(*it);
    }
    return hash_val;
  }
};
#endif  // COMPILER_MSVC

struct StringPieceUtils {
  static bool EqualIgnoreCase(const base::StringPiece& piece1,
                              const base::StringPiece& piece2) {
    base::StringPiece::const_iterator p1i = piece1.begin();
    base::StringPiece::const_iterator p2i = piece2.begin();
    if (piece1.empty() && piece2.empty()) {
      return true;
    } else if (piece1.size() != piece2.size()) {
      return false;
    }
    while (p1i != piece1.end() && p2i != piece2.end()) {
      if (tolower(*p1i) != tolower(*p2i))
        return false;
      ++p1i;
      ++p2i;
    }
    return true;
  }

  static void RemoveWhitespaceContext(base::StringPiece* piece1) {
    base::StringPiece::const_iterator c = piece1->begin();
    base::StringPiece::const_iterator e = piece1->end();
    while (c != e && isspace(*c)) {
      ++c;
    }
    if (c == e) {
      *piece1 = base::StringPiece(c, e-c);
      return;
    }
    --e;
    while (c != e &&isspace(*e)) {
      --e;
    }
    ++e;
    *piece1 = base::StringPiece(c, e-c);
  }

  static bool StartsWithIgnoreCase(const base::StringPiece& text,
                                   const base::StringPiece& starts_with) {
    if (text.size() < starts_with.size())
      return false;
    return EqualIgnoreCase(text.substr(0, starts_with.size()), starts_with);
  }
};
struct StringPieceCaseEqual {
  bool operator()(const base::StringPiece& piece1,
                  const base::StringPiece& piece2) const {
    return StringPieceUtils::EqualIgnoreCase(piece1, piece2);
  }
};



}  // namespace net

#endif  // NET_TOOLS_BALSA_STRING_PIECE_UTILS_H_

