// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef BASE_STRING_UTIL_H_
#define BASE_STRING_UTIL_H_

#include <string.h>

namespace base {

#ifdef WIN32
// Compare the two strings s1 and s2 without regard to case using
// the current locale; returns 0 if they are equal, 1 if s1 > s2, and -1 if
// s2 > s1 according to a lexicographic comparison.
inline int strcasecmp(const char* s1, const char* s2) {
  return _stricmp(s1, s2);
}
#else
inline int strcasecmp(const char* s1, const char* s2) {
  return ::strcasecmp(s1, s2);
}
#endif

}

#endif  // BASE_STRING_UTIL_H_
