// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasons_h
#define CompositingReasons_h

#include "wtf/MathExtras.h"
#include <stdint.h>

namespace WebCore {

const uint64_t CompositingReasonNone                                     = 0;
const uint64_t CompositingReasonAll                                      = ~static_cast<uint64_t>(0);

// Intrinsic reasons that can be known right away by the layer
const uint64_t CompositingReason3DTransform                              = UINT64_C(1) << 0;
const uint64_t CompositingReasonVideo                                    = UINT64_C(1) << 1;
const uint64_t CompositingReasonCanvas                                   = UINT64_C(1) << 2;
const uint64_t CompositingReasonPlugin                                   = UINT64_C(1) << 3;
const uint64_t CompositingReasonIFrame                                   = UINT64_C(1) << 4;
const uint64_t CompositingReasonBackfaceVisibilityHidden                 = UINT64_C(1) << 5;
const uint64_t CompositingReasonActiveAnimation                          = UINT64_C(1) << 6;
const uint64_t CompositingReasonTransitionProperty                       = UINT64_C(1) << 7;
const uint64_t CompositingReasonFilters                                  = UINT64_C(1) << 8;
const uint64_t CompositingReasonPositionFixed                            = UINT64_C(1) << 9;
const uint64_t CompositingReasonOverflowScrollingTouch                   = UINT64_C(1) << 10;
const uint64_t CompositingReasonOverflowScrollingParent                  = UINT64_C(1) << 11;
const uint64_t CompositingReasonOutOfFlowClipping                        = UINT64_C(1) << 12;
const uint64_t CompositingReasonVideoOverlay                             = UINT64_C(1) << 13;
const uint64_t CompositingReasonWillChangeCompositingHint                = UINT64_C(1) << 14;

// Overlap reasons that require knowing what's behind you in paint-order before knowing the answer
const uint64_t CompositingReasonAssumedOverlap                           = UINT64_C(1) << 15;
const uint64_t CompositingReasonOverlap                                  = UINT64_C(1) << 16;
const uint64_t CompositingReasonNegativeZIndexChildren                   = UINT64_C(1) << 17;
const uint64_t CompositingReasonScrollsWithRespectToSquashingLayer       = UINT64_C(1) << 18;
const uint64_t CompositingReasonSquashingSparsityExceeded                = UINT64_C(1) << 19;
const uint64_t CompositingReasonSquashingClippingContainerMismatch       = UINT64_C(1) << 20;
const uint64_t CompositingReasonSquashingOpacityAncestorMismatch         = UINT64_C(1) << 21;
const uint64_t CompositingReasonSquashingTransformAncestorMismatch       = UINT64_C(1) << 22;
const uint64_t CompositingReasonSquashingFilterAncestorMismatch          = UINT64_C(1) << 23;
const uint64_t CompositingReasonSquashingWouldBreakPaintOrder            = UINT64_C(1) << 24;
const uint64_t CompositingReasonSquashingVideoIsDisallowed               = UINT64_C(1) << 25;
const uint64_t CompositingReasonSquashedLayerClipsCompositingDescendants = UINT64_C(1) << 26;

// Subtree reasons that require knowing what the status of your subtree is before knowing the answer
const uint64_t CompositingReasonTransformWithCompositedDescendants       = UINT64_C(1) << 27;
const uint64_t CompositingReasonOpacityWithCompositedDescendants         = UINT64_C(1) << 28;
const uint64_t CompositingReasonMaskWithCompositedDescendants            = UINT64_C(1) << 29;
const uint64_t CompositingReasonReflectionWithCompositedDescendants      = UINT64_C(1) << 30;
const uint64_t CompositingReasonFilterWithCompositedDescendants          = UINT64_C(1) << 31;
const uint64_t CompositingReasonBlendingWithCompositedDescendants        = UINT64_C(1) << 32;
const uint64_t CompositingReasonClipsCompositingDescendants              = UINT64_C(1) << 33;
const uint64_t CompositingReasonPerspectiveWith3DDescendants             = UINT64_C(1) << 34;
const uint64_t CompositingReasonPreserve3DWith3DDescendants              = UINT64_C(1) << 35;
const uint64_t CompositingReasonReflectionOfCompositedParent             = UINT64_C(1) << 36;
const uint64_t CompositingReasonIsolateCompositedDescendants             = UINT64_C(1) << 37;

// The root layer is a special case that may be forced to be a layer, but also it needs to be
// a layer if anything else in the subtree is composited.
const uint64_t CompositingReasonRoot                                     = UINT64_C(1) << 38;

// CompositedLayerMapping internal hierarchy reasons
const uint64_t CompositingReasonLayerForAncestorClip                     = UINT64_C(1) << 39;
const uint64_t CompositingReasonLayerForDescendantClip                   = UINT64_C(1) << 40;
const uint64_t CompositingReasonLayerForPerspective                      = UINT64_C(1) << 41;
const uint64_t CompositingReasonLayerForHorizontalScrollbar              = UINT64_C(1) << 42;
const uint64_t CompositingReasonLayerForVerticalScrollbar                = UINT64_C(1) << 43;
const uint64_t CompositingReasonLayerForScrollCorner                     = UINT64_C(1) << 44;
const uint64_t CompositingReasonLayerForScrollingContents                = UINT64_C(1) << 45;
const uint64_t CompositingReasonLayerForScrollingContainer               = UINT64_C(1) << 46;
const uint64_t CompositingReasonLayerForSquashingContents                = UINT64_C(1) << 47;
const uint64_t CompositingReasonLayerForSquashingContainer               = UINT64_C(1) << 48;
const uint64_t CompositingReasonLayerForForeground                       = UINT64_C(1) << 49;
const uint64_t CompositingReasonLayerForBackground                       = UINT64_C(1) << 50;
const uint64_t CompositingReasonLayerForMask                             = UINT64_C(1) << 51;
const uint64_t CompositingReasonLayerForClippingMask                     = UINT64_C(1) << 52;
const uint64_t CompositingReasonLayerForScrollingBlockSelection          = UINT64_C(1) << 53;

// Various combinations of compositing reasons are defined here also, for more intutive and faster bitwise logic.
const uint64_t CompositingReasonComboAllDirectReasons =
    CompositingReason3DTransform
    | CompositingReasonVideo
    | CompositingReasonCanvas
    | CompositingReasonPlugin
    | CompositingReasonIFrame
    | CompositingReasonBackfaceVisibilityHidden
    | CompositingReasonActiveAnimation
    | CompositingReasonTransitionProperty
    | CompositingReasonFilters
    | CompositingReasonPositionFixed
    | CompositingReasonOverflowScrollingTouch
    | CompositingReasonOverflowScrollingParent
    | CompositingReasonOutOfFlowClipping
    | CompositingReasonVideoOverlay
    | CompositingReasonWillChangeCompositingHint;

const uint64_t CompositingReasonComboAllStyleDeterminedReasons =
    CompositingReason3DTransform
    | CompositingReasonBackfaceVisibilityHidden
    | CompositingReasonActiveAnimation
    | CompositingReasonTransitionProperty
    | CompositingReasonFilters
    | CompositingReasonWillChangeCompositingHint;

const uint64_t CompositingReasonComboReasonsThatRequireOwnBacking =
    CompositingReasonComboAllDirectReasons
    | CompositingReasonOverlap
    | CompositingReasonAssumedOverlap
    | CompositingReasonNegativeZIndexChildren
    | CompositingReasonScrollsWithRespectToSquashingLayer
    | CompositingReasonSquashingSparsityExceeded
    | CompositingReasonSquashingClippingContainerMismatch
    | CompositingReasonSquashingOpacityAncestorMismatch
    | CompositingReasonSquashingTransformAncestorMismatch
    | CompositingReasonSquashingFilterAncestorMismatch
    | CompositingReasonSquashingWouldBreakPaintOrder
    | CompositingReasonSquashingVideoIsDisallowed
    | CompositingReasonSquashedLayerClipsCompositingDescendants
    | CompositingReasonTransformWithCompositedDescendants
    | CompositingReasonOpacityWithCompositedDescendants
    | CompositingReasonMaskWithCompositedDescendants
    | CompositingReasonFilterWithCompositedDescendants
    | CompositingReasonBlendingWithCompositedDescendants
    | CompositingReasonIsolateCompositedDescendants
    | CompositingReasonPreserve3DWith3DDescendants; // preserve-3d has to create backing store to ensure that 3d-transformed elements intersect.

const uint64_t CompositingReasonComboSquashableReasons =
    CompositingReasonOverlap
    | CompositingReasonAssumedOverlap
    | CompositingReasonOverflowScrollingParent;

typedef uint64_t CompositingReasons;

// Any reasons other than overlap or assumed overlap will require the layer to be separately compositing.
inline bool requiresCompositing(CompositingReasons reasons)
{
    return reasons & ~CompositingReasonComboSquashableReasons;
}

// If the layer has overlap or assumed overlap, but no other reasons, then it should be squashed.
inline bool requiresSquashing(CompositingReasons reasons)
{
    return !requiresCompositing(reasons) && (reasons & CompositingReasonComboSquashableReasons);
}

struct CompositingReasonStringMap {
    CompositingReasons reason;
    const char* shortName;
    const char* description;
};

// FIXME: This static data shouldn't be in a header. When it's in the header
// it's copied into every compilation unit that includes the header.
static const CompositingReasonStringMap compositingReasonStringMap[] = {
    { CompositingReasonNone,
        "Unknown",
        "No reason given" },
    { CompositingReason3DTransform,
        "transform3D",
        "Has a 3d transform" },
    { CompositingReasonVideo,
        "video",
        "Is an accelerated video" },
    { CompositingReasonCanvas,
        "canvas",
        "Is an accelerated canvas" },
    { CompositingReasonPlugin,
        "plugin",
        "Is an accelerated plugin" },
    { CompositingReasonIFrame,
        "iFrame",
        "Is an accelerated iFrame" },
    { CompositingReasonBackfaceVisibilityHidden,
        "backfaceVisibilityHidden",
        "Has backface-visibility: hidden" },
    { CompositingReasonActiveAnimation,
        "activeAnimation",
        "Has an active accelerated animation or transition" },
    { CompositingReasonTransitionProperty,
        "transitionProperty",
        "Has an acceleratable transition property (active or inactive)" },
    { CompositingReasonFilters,
        "filters",
        "Has an accelerated filter" },
    { CompositingReasonPositionFixed,
        "positionFixed",
        "Is fixed position" },
    { 0, 0, 0 }, // Available.
    { CompositingReasonOverflowScrollingTouch,
        "overflowScrollingTouch",
        "Is a scrollable overflow element" },
    { CompositingReasonOverflowScrollingParent,
        "overflowScrollingParent",
        "Scroll parent is not an ancestor" },
    { CompositingReasonOutOfFlowClipping,
        "outOfFlowClipping",
        "Has clipping ancestor" },
    { CompositingReasonVideoOverlay,
        "videoOverlay",
        "Is overlay controls for video" },
    { CompositingReasonWillChangeCompositingHint,
        "willChange",
        "Has a will-change compositing hint" },
    { 0, 0, 0 }, // Available.
    { CompositingReasonAssumedOverlap,
        "assumedOverlap",
        "Might overlap other composited content" },
    { CompositingReasonOverlap,
        "overlap",
        "Overlaps other composited content" },
    { CompositingReasonNegativeZIndexChildren,
        "negativeZIndexChildren",
        "Parent with composited negative z-index content" },
    { CompositingReasonScrollsWithRespectToSquashingLayer,
        "scrollsWithRespectToSquashingLayer",
        "Cannot be squashed since this layer scrolls with respect to the squashing layer" },
    { CompositingReasonSquashingSparsityExceeded,
        "squashingSparsityExceeded",
        "Cannot be squashed as the squashing layer would become too sparse" },
    { CompositingReasonSquashingClippingContainerMismatch,
        "squashingClippingContainerMismatch",
        "Cannot be squashed because this layer has a different clipping container than the squashing layer" },
    { CompositingReasonSquashingOpacityAncestorMismatch,
        "squashingOpacityAncestorMismatch",
        "Cannot be squashed because this layer has a different opacity ancestor than the squashing layer" },
    { CompositingReasonSquashingTransformAncestorMismatch,
        "squashingTransformAncestorMismatch",
        "Cannot be squashed because this layer has a different transform ancestor than the squashing layer" },
    { CompositingReasonSquashingFilterAncestorMismatch,
        "squashingFilterAncestorMismatch",
        "Cannot be squashed because this layer has a different filter ancestor than the squashing layer" },
    { CompositingReasonSquashingWouldBreakPaintOrder,
        "squashingWouldBreakPaintOrder",
        "Cannot be squashed without breaking paint order" },
    { CompositingReasonSquashingVideoIsDisallowed,
        "squashingVideoIsDisallowed",
        "Squashing video is not supported" },
    { CompositingReasonSquashedLayerClipsCompositingDescendants,
        "squashedLayerClipsCompositingDescendants",
        "Squashing a layer that clips composited descendants is not supported." },
    { CompositingReasonTransformWithCompositedDescendants,
        "transformWithCompositedDescendants",
        "Has a transform that needs to be known by compositor because of composited descendants" },
    { CompositingReasonOpacityWithCompositedDescendants,
        "opacityWithCompositedDescendants",
        "Has opacity that needs to be applied by compositor because of composited descendants" },
    { CompositingReasonMaskWithCompositedDescendants,
        "maskWithCompositedDescendants",
        "Has a mask that needs to be known by compositor because of composited descendants" },
    { CompositingReasonReflectionWithCompositedDescendants,
        "reflectionWithCompositedDescendants",
        "Has a reflection that needs to be known by compositor because of composited descendants" },
    { CompositingReasonFilterWithCompositedDescendants,
        "filterWithCompositedDescendants",
        "Has a filter effect that needs to be known by compositor because of composited descendants" },
    { CompositingReasonBlendingWithCompositedDescendants,
        "blendingWithCompositedDescendants",
        "Has a blenidng effect that needs to be known by compositor because of composited descendants" },
    { CompositingReasonClipsCompositingDescendants,
        "clipsCompositingDescendants",
        "Has a clip that needs to be known by compositor because of composited descendants" },
    { CompositingReasonPerspectiveWith3DDescendants,
        "perspectiveWith3DDescendants",
        "Has a perspective transform that needs to be known by compositor because of 3d descendants" },
    { CompositingReasonPreserve3DWith3DDescendants,
        "preserve3DWith3DDescendants",
        "Has a preserves-3d property that needs to be known by compositor because of 3d descendants" },
    { CompositingReasonReflectionOfCompositedParent,
        "reflectionOfCompositedParent",
        "Is a reflection of a composited layer" },
    { CompositingReasonIsolateCompositedDescendants,
        "isolateCompositedDescendants",
        "Should isolate descendants to apply a blend effect" },
    { CompositingReasonRoot,
        "root",
        "Is the root layer" },
    { CompositingReasonLayerForAncestorClip,
        "layerForAncestorClip",
        "Secondary layer, applies a clip due to a sibling in the compositing tree" },
    { CompositingReasonLayerForDescendantClip,
        "layerForDescendantClip",
        "Secondary layer, to clip descendants of the owning layer" },
    { CompositingReasonLayerForPerspective,
        "layerForPerspective",
        "Secondary layer, to house the perspective transform for all descendants" },
    { CompositingReasonLayerForHorizontalScrollbar,
        "layerForHorizontalScrollbar",
        "Secondary layer, the horizontal scrollbar layer" },
    { CompositingReasonLayerForVerticalScrollbar,
        "layerForVerticalScrollbar",
        "Secondary layer, the vertical scrollbar layer" },
    { CompositingReasonLayerForScrollCorner,
        "layerForScrollCorner",
        "Secondary layer, the scroll corner layer" },
    { CompositingReasonLayerForScrollingContents,
        "layerForScrollingContents",
        "Secondary layer, to house contents that can be scrolled" },
    { CompositingReasonLayerForScrollingContainer,
        "layerForScrollingContainer",
        "Secondary layer, used to position the scolling contents while scrolling" },
    { CompositingReasonLayerForSquashingContents,
        "layerForSquashingContents",
        "Secondary layer, home for a group of squashable content" },
    { CompositingReasonLayerForSquashingContainer,
        "layerForSquashingContainer",
        "Secondary layer, no-op layer to place the squashing layer correctly in the composited layer tree" },
    { CompositingReasonLayerForForeground,
        "layerForForeground",
        "Secondary layer, to contain any normal flow and positive z-index contents on top of a negative z-index layer" },
    { CompositingReasonLayerForBackground,
        "layerForBackground",
        "Secondary layer, to contain acceleratable background content" },
    { CompositingReasonLayerForMask,
        "layerForMask",
        "Secondary layer, to contain the mask contents" },
    { CompositingReasonLayerForClippingMask,
        "layerForClippingMask",
        "Secondary layer, for clipping mask" },
    { CompositingReasonLayerForScrollingBlockSelection,
        "layerForScrollingBlockSelection",
        "Secondary layer, to house block selection gaps for composited scrolling with no scrolling contents" },
};

} // namespace WebCore

#endif // CompositingReasons_h
