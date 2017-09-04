// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/SquashingDisallowedReasons.h"

#include "wtf/StdLibExtras.h"

namespace blink {

const SquashingDisallowedReasonStringMap kSquashingDisallowedReasonStringMap[] =
    {
        {SquashingDisallowedReasonScrollsWithRespectToSquashingLayer,
         "scrollsWithRespectToSquashingLayer",
         "Cannot be squashed since this layer scrolls with respect to the "
         "squashing layer"},
        {SquashingDisallowedReasonSquashingSparsityExceeded,
         "squashingSparsityExceeded",
         "Cannot be squashed as the squashing layer would become too sparse"},
        {SquashingDisallowedReasonClippingContainerMismatch,
         "squashingClippingContainerMismatch",
         "Cannot be squashed because this layer has a different clipping "
         "container than the squashing layer"},
        {SquashingDisallowedReasonOpacityAncestorMismatch,
         "squashingOpacityAncestorMismatch",
         "Cannot be squashed because this layer has a different opacity "
         "ancestor than the squashing layer"},
        {SquashingDisallowedReasonTransformAncestorMismatch,
         "squashingTransformAncestorMismatch",
         "Cannot be squashed because this layer has a different transform "
         "ancestor than the squashing layer"},
        {SquashingDisallowedReasonFilterMismatch,
         "squashingFilterAncestorMismatch",
         "Cannot be squashed because this layer has a different filter "
         "ancestor than the squashing layer, or this layer has a filter"},
        {SquashingDisallowedReasonWouldBreakPaintOrder,
         "squashingWouldBreakPaintOrder",
         "Cannot be squashed without breaking paint order"},
        {SquashingDisallowedReasonSquashingVideoIsDisallowed,
         "squashingVideoIsDisallowed", "Squashing video is not supported"},
        {SquashingDisallowedReasonSquashedLayerClipsCompositingDescendants,
         "squashedLayerClipsCompositingDescendants",
         "Squashing a layer that clips composited descendants is not "
         "supported."},
        {SquashingDisallowedReasonSquashingLayoutPartIsDisallowed,
         "squashingLayoutPartIsDisallowed",
         "Squashing a frame, iframe or plugin is not supported."},
        {SquashingDisallowedReasonSquashingBlendingIsDisallowed,
         "squashingBlendingDisallowed",
         "Squashing a layer with blending is not supported."},
        {SquashingDisallowedReasonNearestFixedPositionMismatch,
         "squashingNearestFixedPositionMismatch",
         "Cannot be squashed because this layer has a different nearest fixed "
         "position layer than the squashing layer"},
        {SquashingDisallowedReasonScrollChildWithCompositedDescendants,
         "scrollChildWithCompositedDescendants",
         "Squashing a scroll child with composited descendants is not "
         "supported."},
        {SquashingDisallowedReasonSquashingLayerIsAnimating,
         "squashingLayerIsAnimating",
         "Cannot squash into a layer that is animating."},
        {SquashingDisallowedReasonRenderingContextMismatch,
         "squashingLayerRenderingContextMismatch",
         "Cannot squash layers with different 3D contexts."},
        {SquashingDisallowedReasonNonTranslationTransform,
         "SquashingDisallowedReasonNonTranslationTransform",
         "Cannot squash layers with transforms that are not identity or "
         "translation."},
        {SquashingDisallowedReasonFragmentedContent,
         "SquashingDisallowedReasonFragmentedContent",
         "Cannot squash layers that are inside fragmentation contexts."},
};

const size_t kNumberOfSquashingDisallowedReasons =
    WTF_ARRAY_LENGTH(kSquashingDisallowedReasonStringMap);

}  // namespace blink
