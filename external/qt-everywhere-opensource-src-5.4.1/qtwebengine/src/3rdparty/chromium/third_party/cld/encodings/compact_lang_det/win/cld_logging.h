// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOGGING_H_
#define ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOGGING_H_

#if !defined(CLD_WINDOWS)

#include "base/logging.h"

#else

#undef CHECK
#define CHECK(expr)
#undef DCHECK
#define DCHECK(expr)

#endif

#endif  // ENCODINGS_COMPACT_LANG_DET_WIN_CLD_LOGGING_H_
