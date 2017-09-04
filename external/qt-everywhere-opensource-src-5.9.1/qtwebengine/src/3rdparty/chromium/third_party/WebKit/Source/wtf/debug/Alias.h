// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_Alias_h
#define WTF_Alias_h

#include "base/debug/alias.h"

namespace WTF {
namespace debug {

inline void alias(const void* var) {
  base::debug::Alias(var);
}

}  // namespace debug
}  // namespace WTF

#endif  // WTF_Alias_h
