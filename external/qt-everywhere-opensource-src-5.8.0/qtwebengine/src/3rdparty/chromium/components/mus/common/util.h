// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_UTIL_H_
#define COMPONENTS_MUS_COMMON_UTIL_H_

#include <stdint.h>

#include "components/mus/common/types.h"

// TODO(beng): #$*&@#(@ MacOSX SDK!
#if defined(HiWord)
#undef HiWord
#endif
#if defined(LoWord)
#undef LoWord
#endif

namespace mus {

inline uint16_t HiWord(uint32_t id) {
  return static_cast<uint16_t>((id >> 16) & 0xFFFF);
}

inline uint16_t LoWord(uint32_t id) {
  return static_cast<uint16_t>(id & 0xFFFF);
}

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_UTIL_H_
