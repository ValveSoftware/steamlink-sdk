// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code is not actually used, it was copied here for the reference only.
// See cld_htmlutils_windows.cc for Windows version of this code.

#include "i18n/encodings/compact_lang_det/cld_unilib.h"

#include "util/utf8/unilib.h"

namespace cld_UniLib {

int OneCharLen(const char* src) {
  return UniLib::OneCharLen(src);
}

}  // namespace cld_UniLib
