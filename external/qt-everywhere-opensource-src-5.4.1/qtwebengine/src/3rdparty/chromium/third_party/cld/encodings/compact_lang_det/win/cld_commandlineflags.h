// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ENCODINGS_COMPACT_LANG_DET_WIN_CLD_COMMANDLINEFLAGS_H_
#define ENCODINGS_COMPACT_LANG_DET_WIN_CLD_COMMANDLINEFLAGS_H_

#if !defined(CLD_WINDOWS)

#include "base/commandlineflags.h"

#else

#undef DEFINE_bool
#define DEFINE_bool(name, default_value, comment) \
    const bool FLAGS_##name = default_value;
#undef DEFINE_int32
#define DEFINE_int32(name, default_value, comment) \
    const int32 FLAGS_##name = default_value;

#undef DECLARE_bool
#define DECLARE_bool(name) extern const bool FLAGS_##name;
#undef DECLARE_int32
#define DECLARE_int32(name) extern int32 FLAGS_##name;

#endif

#endif  // ENCODINGS_COMPACT_LANG_DET_WIN_CLD_COMMANDLINEFLAGS_H_
