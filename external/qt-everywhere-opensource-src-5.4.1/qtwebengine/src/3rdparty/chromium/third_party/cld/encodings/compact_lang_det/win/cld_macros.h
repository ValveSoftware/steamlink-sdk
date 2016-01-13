// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODINGS_COMPACT_LANG_DET_WIN_CLD_MACROS_H_
#define ENCODINGS_COMPACT_LANG_DET_WIN_CLD_MACROS_H_

#include "base/macros.h"

// Checks for Win32 result and if it indicates failure, returns it.
#define RETURN_IF_ERROR(cmd) \
  do { \
    DWORD result_ = (cmd); \
    if (0 != result_) \
      return result_; \
  } \
  while (0);

#endif  // ENCODINGS_COMPACT_LANG_DET_WIN_CLD_MACROS_H_
