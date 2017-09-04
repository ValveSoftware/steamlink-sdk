// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SquashingDisallowedReasons_h
#define SquashingDisallowedReasons_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include <stdint.h>

namespace blink {

enum SquashingDisallowedReason {
  SquashingDisallowedReasonsNone = 0,
  SquashingDisallowedReasonScrollsWithRespectToSquashingLayer = 1 << 0,
  SquashingDisallowedReasonSquashingSparsityExceeded = 1 << 1,
  SquashingDisallowedReasonClippingContainerMismatch = 1 << 2,
  SquashingDisallowedReasonOpacityAncestorMismatch = 1 << 3,
  SquashingDisallowedReasonTransformAncestorMismatch = 1 << 4,
  SquashingDisallowedReasonFilterMismatch = 1 << 5,
  SquashingDisallowedReasonWouldBreakPaintOrder = 1 << 6,
  SquashingDisallowedReasonSquashingVideoIsDisallowed = 1 << 7,
  SquashingDisallowedReasonSquashedLayerClipsCompositingDescendants = 1 << 8,
  SquashingDisallowedReasonSquashingLayoutPartIsDisallowed = 1 << 9,
  SquashingDisallowedReasonSquashingBlendingIsDisallowed = 1 << 10,
  SquashingDisallowedReasonNearestFixedPositionMismatch = 1 << 11,
  SquashingDisallowedReasonScrollChildWithCompositedDescendants = 1 << 12,
  SquashingDisallowedReasonSquashingLayerIsAnimating = 1 << 13,
  SquashingDisallowedReasonRenderingContextMismatch = 1 << 14,
  SquashingDisallowedReasonNonTranslationTransform = 1 << 15,
  SquashingDisallowedReasonFragmentedContent = 1 << 16,
};

typedef unsigned SquashingDisallowedReasons;

struct SquashingDisallowedReasonStringMap {
  STACK_ALLOCATED();
  SquashingDisallowedReasons reason;
  const char* shortName;
  const char* description;
};

PLATFORM_EXPORT extern const SquashingDisallowedReasonStringMap
    kSquashingDisallowedReasonStringMap[];
PLATFORM_EXPORT extern const size_t kNumberOfSquashingDisallowedReasons;

}  // namespace blink

#endif  // SquashingDisallowedReasons_h
