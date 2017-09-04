// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomCompositorAnimationManager_h
#define CustomCompositorAnimationManager_h

#include "core/CoreExport.h"
#include "platform/graphics/CompositorMutation.h"
#include "platform/graphics/CompositorMutationsTarget.h"

namespace blink {

class CORE_EXPORT CustomCompositorAnimationManager
    : public CompositorMutationsTarget {
  WTF_MAKE_NONCOPYABLE(CustomCompositorAnimationManager);

 public:
  CustomCompositorAnimationManager();
  ~CustomCompositorAnimationManager() override;
  void applyMutations(CompositorMutations*) override;
};

}  // namespace blink

#endif  // CustomCompositorAnimationManager_h
