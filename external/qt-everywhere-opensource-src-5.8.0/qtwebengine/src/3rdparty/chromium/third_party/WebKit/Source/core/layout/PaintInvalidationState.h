// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidationState_h
#define PaintInvalidationState_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LayoutBoxModelObject;
class LayoutObject;
class LayoutSVGModelObject;
class LayoutView;
class PaintLayer;

enum VisualRectFlags {
    DefaultVisualRectFlags = 0,
    EdgeInclusive = 1
};

// PaintInvalidationState is an optimization used during the paint
// invalidation phase.
//
// This class is extremely close to LayoutState so see the documentation
// of LayoutState for the class existence and performance benefits.
//
// The main difference with LayoutState is that it was customized for the
// needs of the paint invalidation systems (keeping visual rectangles
// instead of layout specific information).
//
// See Source/core/paint/README.md#Paint-invalidation for more details.

class CORE_EXPORT PaintInvalidationState {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(PaintInvalidationState);
public:
    PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutObject&);

    // For root LayoutView, or when sub-frame LayoutView's invalidateTreeIfNeeded() is called directly from
    // FrameView::invalidateTreeIfNeededRecursive() instead of the owner LayoutPart.
    // TODO(wangxianzhu): Eliminate the latter case.
    PaintInvalidationState(const LayoutView&, Vector<const LayoutObject*>& pendingDelayedPaintInvalidations);

    // When a PaintInvalidationState is constructed, it can be used to map points/rects in the object's
    // local space (border box space for LayoutBoxes). After invalidation of the current object,
    // before invalidation of the subtrees, this method must be called to apply clip and scroll offset
    // etc. for creating child PaintInvalidationStates.
    void updateForChildren(PaintInvalidationReason);

    bool hasForcedSubtreeInvalidationFlags() const { return m_forcedSubtreeInvalidationFlags; }

    bool forcedSubtreeInvalidationCheckingWithinContainer() const { return m_forcedSubtreeInvalidationFlags & InvalidationChecking; }
    void setForceSubtreeInvalidationCheckingWithinContainer() { m_forcedSubtreeInvalidationFlags |= InvalidationChecking; }

    bool forcedSubtreeFullInvalidationWithinContainer() const { return m_forcedSubtreeInvalidationFlags & FullInvalidation; }

    bool forcedSubtreeInvalidationRectUpdateWithinContainerOnly() const { return m_forcedSubtreeInvalidationFlags == InvalidationRectUpdate; }
    void setForceSubtreeInvalidationRectUpdateWithinContainer() { m_forcedSubtreeInvalidationFlags |= InvalidationRectUpdate; }

    const LayoutBoxModelObject& paintInvalidationContainer() const { return *m_paintInvalidationContainer; }

    // Computes the position of the current object ((0,0) in the space of the object)
    // in the space of paint invalidation backing.
    LayoutPoint computePositionFromPaintInvalidationBacking() const;

    // Returns the rect bounds needed to invalidate paint of this object,
    // in the space of paint invalidation backing.
    LayoutRect computePaintInvalidationRectInBacking() const;

    void mapLocalRectToPaintInvalidationBacking(LayoutRect&) const;

    PaintLayer& paintingLayer() const;

#if ENABLE(ASSERT)
    const LayoutObject& currentObject() const { return m_currentObject; }
#endif

private:
    friend class VisualRectMappingTest;

    void mapLocalRectToPaintInvalidationContainer(LayoutRect&) const;

    void updateForCurrentObject(const PaintInvalidationState& parentState);
    void updateForNormalChildren();

    LayoutRect computePaintInvalidationRectInBackingForSVG() const;

    void addClipRectRelativeToPaintOffset(const LayoutRect& localClipRect);

    const LayoutObject& m_currentObject;

    enum ForcedSubtreeInvalidationFlag {
        InvalidationChecking = 1 << 0,
        InvalidationRectUpdate = 1 << 1,
        FullInvalidation = 1 << 2,
        FullInvalidationForStackedContents = 1 << 3,
    };
    unsigned m_forcedSubtreeInvalidationFlags;

    bool m_clipped;
    bool m_clippedForAbsolutePosition;

    // Clip rect from paintInvalidationContainer if m_cachedOffsetsEnabled is true.
    LayoutRect m_clipRect;
    LayoutRect m_clipRectForAbsolutePosition;

    // x/y offset from the paintInvalidationContainer if m_cachedOffsetsEnabled is true.
    // It includes relative positioning and scroll offsets.
    LayoutSize m_paintOffset;
    LayoutSize m_paintOffsetForAbsolutePosition;

    // Whether m_paintOffset[XXX] and m_clipRect[XXX] are valid and can be used
    // to map a rect from space of the current object to space of paintInvalidationContainer.
    bool m_cachedOffsetsEnabled;
    bool m_cachedOffsetsForAbsolutePositionEnabled;

    // The following two fields are never null. Declare them as pointers because we need some
    // logic to initialize them in the body of the constructor.

    // The current paint invalidation container for normal flow objects.
    // It is the enclosing composited object.
    const LayoutBoxModelObject* m_paintInvalidationContainer;

    // The current paint invalidation container for stacked contents (stacking contexts or positioned objects).
    // It is the nearest ancestor composited object which establishes a stacking context.
    // See Source/core/paint/README.md ### PaintInvalidationState for details on how stacked contents'
    // paint invalidation containers differ.
    const LayoutBoxModelObject* m_paintInvalidationContainerForStackedContents;

    const LayoutObject& m_containerForAbsolutePosition;

    // Transform from the initial viewport coordinate system of an outermost
    // SVG root to the userspace _before_ the relevant element. Combining this
    // with |m_paintOffset| yields the "final" offset.
    AffineTransform m_svgTransform;

    // Records objects needing paint invalidation on the next frame. See the definition of PaintInvalidationDelayedFull for more details.
    Vector<const LayoutObject*>& m_pendingDelayedPaintInvalidations;

    PaintLayer& m_paintingLayer;

#if ENABLE(ASSERT)
    bool m_didUpdateForChildren;
#endif

#if ENABLE(ASSERT) && !defined(NDEBUG)
// #define CHECK_FAST_PATH_SLOW_PATH_EQUALITY
#endif

#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    void assertFastPathAndSlowPathRectsEqual(const LayoutRect& fastPathRect, const LayoutRect& slowPathRect) const;
    bool m_canCheckFastPathSlowPathEquality;
#endif
};

} // namespace blink

#endif // PaintInvalidationState_h
