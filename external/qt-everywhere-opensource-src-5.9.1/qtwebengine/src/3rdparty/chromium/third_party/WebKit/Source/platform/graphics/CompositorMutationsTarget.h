// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutationsTarget_h
#define CompositorMutationsTarget_h

#include "platform/PlatformExport.h"
#include "platform/graphics/CompositorMutation.h"

namespace blink {

class PLATFORM_EXPORT CompositorMutationsTarget {
 public:
  virtual void applyMutations(CompositorMutations*) = 0;
  virtual ~CompositorMutationsTarget() {}
};

}  // namespace blink

#endif  // CompositorMutationsTarget_h
