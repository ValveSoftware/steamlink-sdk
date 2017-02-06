// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_MUTABLE_PROPERTIES_H_
#define CC_ANIMATION_MUTABLE_PROPERTIES_H_

#include <stdint.h>

namespace cc {

struct MutableProperty {
  enum : uint32_t { kNone = 0 };
  enum : uint32_t { kOpacity = 1 << 0 };
  enum : uint32_t { kScrollLeft = 1 << 1 };
  enum : uint32_t { kScrollTop = 1 << 2 };
  enum : uint32_t { kTransform = 1 << 3 };

  enum : int { kNumProperties = 4 };
};

}  // namespace cc

#endif  // CC_ANIMATION_MUTABLE_PROPERTIES_H_
