// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SquashingDisallowedReasons_h
#define SquashingDisallowedReasons_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include <stdint.h>

namespace blink {

const uint64_t SquashingDisallowedReasonsNone = 0;

const uint64_t SquashingDisallowedReasonScrollsWithRespectToSquashingLayer       = UINT64_C(1) << 0;
const uint64_t SquashingDisallowedReasonSquashingSparsityExceeded                = UINT64_C(1) << 1;
const uint64_t SquashingDisallowedReasonClippingContainerMismatch                = UINT64_C(1) << 2;
const uint64_t SquashingDisallowedReasonOpacityAncestorMismatch                  = UINT64_C(1) << 3;
const uint64_t SquashingDisallowedReasonTransformAncestorMismatch                = UINT64_C(1) << 4;
const uint64_t SquashingDisallowedReasonFilterMismatch                           = UINT64_C(1) << 5;
const uint64_t SquashingDisallowedReasonWouldBreakPaintOrder                     = UINT64_C(1) << 6;
const uint64_t SquashingDisallowedReasonSquashingVideoIsDisallowed               = UINT64_C(1) << 7;
const uint64_t SquashingDisallowedReasonSquashedLayerClipsCompositingDescendants = UINT64_C(1) << 8;
const uint64_t SquashingDisallowedReasonSquashingLayoutPartIsDisallowed          = UINT64_C(1) << 9;
const uint64_t SquashingDisallowedReasonSquashingReflectionIsDisallowed          = UINT64_C(1) << 10;
const uint64_t SquashingDisallowedReasonSquashingBlendingIsDisallowed            = UINT64_C(1) << 11;
const uint64_t SquashingDisallowedReasonNearestFixedPositionMismatch             = UINT64_C(1) << 12;
const uint64_t SquashingDisallowedReasonScrollChildWithCompositedDescendants     = UINT64_C(1) << 13;
const uint64_t SquashingDisallowedReasonSquashingLayerIsAnimating                = UINT64_C(1) << 14;
const uint64_t SquashingDisallowedReasonRenderingContextMismatch                 = UINT64_C(1) << 15;
const uint64_t SquashingDisallowedReasonNonTranslationTransform                  = UINT64_C(1) << 16;
const uint64_t SquashingDisallowedReasonFragmentedContent                        = UINT64_C(1) << 17;

typedef uint64_t SquashingDisallowedReasons;

struct SquashingDisallowedReasonStringMap {
    STACK_ALLOCATED();
    SquashingDisallowedReasons reason;
    const char* shortName;
    const char* description;
};

PLATFORM_EXPORT extern const SquashingDisallowedReasonStringMap kSquashingDisallowedReasonStringMap[];
PLATFORM_EXPORT extern const size_t kNumberOfSquashingDisallowedReasons;

} // namespace blink

#endif // SquashingDisallowedReasons_h
