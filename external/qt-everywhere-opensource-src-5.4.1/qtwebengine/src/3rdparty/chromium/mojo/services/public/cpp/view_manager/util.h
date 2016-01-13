// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_UTIL_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_UTIL_H_

#include "mojo/services/public/cpp/view_manager/types.h"

// TODO(beng): #$*&@#(@ MacOSX SDK!
#if defined(HiWord)
#undef HiWord
#endif
#if defined(LoWord)
#undef LoWord
#endif

namespace mojo {
namespace view_manager {

inline uint16_t HiWord(uint32_t id) {
  return static_cast<uint16_t>((id >> 16) & 0xFFFF);
}

inline uint16_t LoWord(uint32_t id) {
  return static_cast<uint16_t>(id & 0xFFFF);
}

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_UTIL_H_
