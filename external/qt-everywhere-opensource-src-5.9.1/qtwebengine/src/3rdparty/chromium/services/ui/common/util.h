// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_COMMON_UTIL_H_
#define SERVICES_UI_COMMON_UTIL_H_

#include <stdint.h>

#include "services/ui/common/types.h"

// TODO(beng): #$*&@#(@ MacOSX SDK!
#if defined(HiWord)
#undef HiWord
#endif
#if defined(LoWord)
#undef LoWord
#endif

namespace ui {

inline uint16_t HiWord(uint32_t id) {
  return static_cast<uint16_t>((id >> 16) & 0xFFFF);
}

inline uint16_t LoWord(uint32_t id) {
  return static_cast<uint16_t>(id & 0xFFFF);
}

}  // namespace ui

#endif  // SERVICES_UI_COMMON_UTIL_H_
