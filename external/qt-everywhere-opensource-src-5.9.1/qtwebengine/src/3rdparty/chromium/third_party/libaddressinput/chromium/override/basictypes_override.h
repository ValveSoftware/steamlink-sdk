// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_BASICTYPES_OVERRIDE_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_BASICTYPES_OVERRIDE_H_

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"

// There is no more base/basictypes.h in Chromium. Even though libaddressinput
// has its own "basictypes.h" that defines a whole host of custom integers of
// specific lengths, the only one actually used is |uint32| in util/md5.cc. So
// here you go:

typedef uint32_t uint32;

// TODO: Switch libaddressinput to |uint32_t|.

// Also, Chromium uses the C++11 |static_assert|, so here's a definition for the
// old COMPILE_ASSERT:

#define COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)

// TODO: Switch libaddressinput to |static_assert|.

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_BASICTYPES_OVERRIDE_H_
