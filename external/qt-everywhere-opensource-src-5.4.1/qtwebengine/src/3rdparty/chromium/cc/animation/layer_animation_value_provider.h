// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_LAYER_ANIMATION_VALUE_PROVIDER_H_
#define CC_ANIMATION_LAYER_ANIMATION_VALUE_PROVIDER_H_

#include "cc/base/cc_export.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {

// A LayerAnimationValueProvider is used for determining the starting value
// for animations that start at their 'current' value rather than at a
// pre-specified value.
class CC_EXPORT LayerAnimationValueProvider {
 public:
  virtual ~LayerAnimationValueProvider() {}

  virtual gfx::Vector2dF ScrollOffsetForAnimation() const = 0;
};

}  // namespace cc

#endif  // CC_ANIMATION_LAYER_ANIMATION_VALUE_PROVIDER_H_
