// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODINGS_COMPACT_LANG_DET_WIN_CLD_HTMLUTILS_H_
#define ENCODINGS_COMPACT_LANG_DET_WIN_CLD_HTMLUTILS_H_

// Src points to '&'
// Writes entity value to dst. Returns take(src), put(dst) byte counts
void EntityToBuffer(const char* src, int len, char* dst,
                    int* tlen, int* plen);

#endif  // ENCODINGS_COMPACT_LANG_DET_WIN_CLD_HTMLUTILS_H_
