// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/PaintInvalidationState.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/PaintLayer.h"

namespace blink {

static bool supportsCachedOffsets(const LayoutObject& object)
{
    return !object.hasTransformRelatedProperty()
        && !object.hasReflection()
        && !object.hasFilterInducingProperty()
        && !object.isLayoutFlowThread()
        && !object.isLayoutMultiColumnSpannerPlaceholder()
        && !object.styleRef().isFlippedBlocksWritingMode()
        && !(object.isLayoutBlock() && object.isSVG());
}

PaintInvalidationState::PaintInvalidationState(const LayoutView& layoutView, Vector<const LayoutObject*>& pendingDelayedPaintInvalidations)
    : m_currentObject(layoutView)
    , m_forcedSubtreeInvalidationFlags(0)
    , m_clipped(false)
    , m_clippedForAbsolutePosition(false)
    , m_cachedOffsetsEnabled(true)
    , m_cachedOffsetsForAbsolutePositionEnabled(true)
    , m_paintInvalidationContainer(&layoutView.containerForPaintInvalidation())
    , m_paintInvalidationContainerForStackedContents(m_paintInvalidationContainer)
    , m_containerForAbsolutePosition(layoutView)
    , m_pendingDelayedPaintInvalidations(pendingDelayedPaintInvalidations)
    , m_paintingLayer(*layoutView.layer())
#if ENABLE(ASSERT)
    , m_didUpdateForChildren(false)
#endif
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    , m_canCheckFastPathSlowPathEquality(layoutView == m_paintInvalidationContainer)
#endif
{
    if (!supportsCachedOffsets(layoutView)) {
        m_cachedOffsetsEnabled = false;
        return;
    }

    FloatPoint point = layoutView.localToAncestorPoint(FloatPoint(), m_paintInvalidationContainer, TraverseDocumentBoundaries | InputIsInFrameCoordinates);
    m_paintOffset = LayoutSize(point.x(), point.y());
    m_paintOffsetForAbsolutePosition = m_paintOffset;
}

PaintInvalidationState::PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutObject& currentObject)
    : m_currentObject(currentObject)
    , m_forcedSubtreeInvalidationFlags(parentState.m_forcedSubtreeInvalidationFlags)
    , m_clipped(parentState.m_clipped)
    , m_clippedForAbsolutePosition(parentState.m_clippedForAbsolutePosition)
    , m_clipRect(parentState.m_clipRect)
    , m_clipRectForAbsolutePosition(parentState.m_clipRectForAbsolutePosition)
    , m_paintOffset(parentState.m_paintOffset)
    , m_paintOffsetForAbsolutePosition(parentState.m_paintOffsetForAbsolutePosition)
    , m_cachedOffsetsEnabled(parentState.m_cachedOffsetsEnabled)
    , m_cachedOffsetsForAbsolutePositionEnabled(parentState.m_cachedOffsetsForAbsolutePositionEnabled)
    , m_paintInvalidationContainer(parentState.m_paintInvalidationContainer)
    , m_paintInvalidationContainerForStackedContents(parentState.m_paintInvalidationContainerForStackedContents)
    , m_containerForAbsolutePosition(currentObject.canContainAbsolutePositionObjects() ? currentObject : parentState.m_containerForAbsolutePosition)
    , m_svgTransform(parentState.m_svgTransform)
    , m_pendingDelayedPaintInvalidations(parentState.m_pendingDelayedPaintInvalidations)
    , m_paintingLayer(currentObject.hasLayer() && toLayoutBoxModelObject(currentObject).hasSelfPaintingLayer() ? *toLayoutBoxModelObject(currentObject).layer() : parentState.m_paintingLayer)
#if ENABLE(ASSERT)
    , m_didUpdateForChildren(false)
#endif
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
    , m_canCheckFastPathSlowPathEquality(parentState.m_canCheckFastPathSlowPathEquality)
#endif
{
    ASSERT(&m_paintingLayer == currentObject.paintingLayer());

    if (currentObject == parentState.m_currentObject) {
        // Sometimes we create a new PaintInvalidationState from parentState on the same object
        // (e.g. LayoutView, and the HorriblySlowRectMapping cases in LayoutBlock::invalidatePaintOfSubtreesIfNeeded()).
        // TODO(wangxianzhu): Avoid this for RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled().
#if ENABLE(ASSERT)
        m_didUpdateForChildren = parentState.m_didUpdateForChildren;
#endif
        return;
    }

    ASSERT(parentState.m_didUpdateForChildren);

    if (currentObject.isPaintInvalidationContainer()) {
        m_paintInvalidationContainer = toLayoutBoxModelObject(&currentObject);
        if (currentObject.styleRef().isStackingContext())
            m_paintInvalidationContainerForStackedContents = toLayoutBoxModelObject(&currentObject);
    } else if (currentObject.isLayoutView()) {
        // m_paintInvalidationContainerForStackedContents is only for stacked descendants in its own frame,
        // because it doesn't establish stacking context for stacked contents in sub-frames.
        // Contents stacked in the root stacking context in this frame should use this frame's paintInvalidationContainer.
        m_paintInvalidationContainerForStackedContents = m_paintInvalidationContainer;
    } else if (currentObject.styleRef().isStacked()
        // This is to exclude some objects (e.g. LayoutText) inheriting stacked style from parent but aren't actually stacked.
        && currentObject.hasLayer()
        && m_paintInvalidationContainer != m_paintInvalidationContainerForStackedContents) {
        // The current object is stacked, so we should use m_paintInvalidationContainerForStackedContents as its
        // paint invalidation container on which the current object is painted.
        m_paintInvalidationContainer = m_paintInvalidationContainerForStackedContents;
        // We are changing paintInvalidationContainer to m_paintInvalidationContainerForStackedContents. Must disable
        // cached offsets because we didn't track paint offset from m_paintInvalidationContainerForStackedContents.
        // TODO(wangxianzhu): There are optimization opportunities:
        // - Like what we do for fixed-position, calculate the paint offset in slow path and enable fast path for
        //   descendants if possible; or
        // - Track offset between the two paintInvalidationContainers.
        m_cachedOffsetsEnabled = false;
        if (m_forcedSubtreeInvalidationFlags & FullInvalidationForStackedContents)
            m_forcedSubtreeInvalidationFlags |= FullInvalidation;
    }

    if (!currentObject.isBoxModelObject() && !currentObject.isSVG())
        return;

    if (m_cachedOffsetsEnabled || currentObject == m_paintInvalidationContainer)
        m_cachedOffsetsEnabled = supportsCachedOffsets(currentObject);

    if (currentObject.isSVG()) {
        if (currentObject.isSVGRoot()) {
            m_svgTransform = toLayoutSVGRoot(currentObject).localToBorderBoxTransform();
            // Don't early return here, because the SVGRoot object needs to execute the later code
            // as a normal LayoutBox.
        } else {
            ASSERT(currentObject != m_paintInvalidationContainer);
            m_svgTransform *= currentObject.localToSVGParentTransform();
            return;
        }
    }

    if (currentObject == m_paintInvalidationContainer) {
        // When we hit a new paint invalidation container, we don't need to
        // continue forcing a check for paint invalidation, since we're
        // descending into a different invalidation container. (For instance if
        // our parents were moved, the entire container will just move.)
        if (currentObject != m_paintInvalidationContainerForStackedContents) {
            // However, we need to keep the FullInvalidationForStackedContents flag
            // if the current object isn't the paint invalidation container of
            // stacked contents.
            m_forcedSubtreeInvalidationFlags &= FullInvalidationForStackedContents;
        } else {
            m_forcedSubtreeInvalidationFlags = 0;
            if (currentObject != m_containerForAbsolutePosition
                && m_cachedOffsetsForAbsolutePositionEnabled
                && m_cachedOffsetsEnabled) {
                // The current object is the new paintInvalidationContainer for absolute-position descendants but is not their container.
                // Call updateForCurrentObject() before resetting m_paintOffset to get paint offset of the current object
                // from the original paintInvalidationContainerForStackingContents, then use this paint offset to adjust
                // m_paintOffsetForAbsolutePosition.
                updateForCurrentObject(parentState);
                m_paintOffsetForAbsolutePosition -= m_paintOffset;
                if (m_clippedForAbsolutePosition)
                    m_clipRectForAbsolutePosition.move(-m_paintOffset);
            }
        }

        m_clipped = false; // Will be updated in updateForChildren().
        m_paintOffset = LayoutSize();
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
        m_canCheckFastPathSlowPathEquality = true;
#endif
        return;
    }

    updateForCurrentObject(parentState);
}

void PaintInvalidationState::updateForCurrentObject(const PaintInvalidationState& parentState)
{
    if (!m_cachedOffsetsEnabled)
        return;

    if (m_currentObject.isLayoutView()) {
        ASSERT(&parentState.m_currentObject == toLayoutView(m_currentObject).frame()->ownerLayoutObject());
        m_paintOffset += toLayoutBox(parentState.m_currentObject).contentBoxOffset();
        // a LayoutView paints with a defined size but a pixel-rounded offset.
        m_paintOffset = LayoutSize(roundedIntSize(m_paintOffset));
        return;
    }

    EPosition position = m_currentObject.styleRef().position();

    if (position == FixedPosition) {
        if (m_paintInvalidationContainer != m_currentObject.view() && m_paintInvalidationContainer->view() == m_currentObject.view()) {
            // TODO(crbug.com/598762): localToAncestorPoint() is incorrect for fixed-position when paintInvalidationContainer
            // is under the containing LayoutView.
            m_cachedOffsetsEnabled = false;
            return;
        }
        // Use slow path to get the offset of the fixed-position, and enable fast path for descendants.
        FloatPoint fixedOffset = m_currentObject.localToAncestorPoint(FloatPoint(), m_paintInvalidationContainer, TraverseDocumentBoundaries);
        m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y());
        // In the above way to get paint offset, we can't get accurate clip rect, so just assume no clip.
        // Clip on fixed-position is rare, in case that paintInvalidationContainer crosses frame boundary
        // and the LayoutView is clipped by something in owner document.
        if (m_clipped) {
            m_clipped = false;
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
            m_canCheckFastPathSlowPathEquality = false;
#endif
        }
        return;
    }

    if (position == AbsolutePosition) {
        m_cachedOffsetsEnabled = m_cachedOffsetsForAbsolutePositionEnabled;
        if (!m_cachedOffsetsEnabled)
            return;

        m_paintOffset = m_paintOffsetForAbsolutePosition;
        m_clipped = m_clippedForAbsolutePosition;
        m_clipRect = m_clipRectForAbsolutePosition;

        // Handle absolute-position block under relative-position inline.
        const LayoutObject& container = parentState.m_containerForAbsolutePosition;
        if (container.isInFlowPositioned() && container.isLayoutInline())
            m_paintOffset += toLayoutInline(container).offsetForInFlowPositionedInline(toLayoutBox(m_currentObject));
    }

    if (m_currentObject.isBox())
        m_paintOffset += toLayoutBox(m_currentObject).locationOffset();

    if (m_currentObject.isInFlowPositioned() && m_currentObject.hasLayer())
        m_paintOffset += toLayoutBoxModelObject(m_currentObject).layer()->offsetForInFlowPosition();
}

void PaintInvalidationState::updateForChildren(PaintInvalidationReason reason)
{
#if ENABLE(ASSERT)
    ASSERT(!m_didUpdateForChildren);
    m_didUpdateForChildren = true;
#endif

    switch (reason) {
    case PaintInvalidationDelayedFull:
        m_pendingDelayedPaintInvalidations.append(&m_currentObject);
        break;
    case PaintInvalidationSubtree:
        m_forcedSubtreeInvalidationFlags |= (FullInvalidation | FullInvalidationForStackedContents);
        break;
    case PaintInvalidationSVGResourceChange:
        setForceSubtreeInvalidationCheckingWithinContainer();
        break;
    default:
        break;
    }

    updateForNormalChildren();

    if (m_currentObject == m_containerForAbsolutePosition) {
        if (m_paintInvalidationContainer == m_paintInvalidationContainerForStackedContents) {
            m_cachedOffsetsForAbsolutePositionEnabled = m_cachedOffsetsEnabled;
            if (m_cachedOffsetsEnabled) {
                m_paintOffsetForAbsolutePosition = m_paintOffset;
                m_clippedForAbsolutePosition = m_clipped;
                m_clipRectForAbsolutePosition = m_clipRect;
            }
        } else {
            // Cached offsets for absolute-position are from m_paintInvalidationContainer,
            // which can't be used if the absolute-position descendants will use a different
            // paintInvalidationContainer.
            // TODO(wangxianzhu): Same optimization opportunities as under isStacked() condition
            // in the PaintInvalidationState::PaintInvalidationState(... LayoutObject&...).
            m_cachedOffsetsForAbsolutePositionEnabled = false;
        }
    }
}

void PaintInvalidationState::updateForNormalChildren()
{
    if (!m_cachedOffsetsEnabled)
        return;

    if (!m_currentObject.isBoxModelObject() && !m_currentObject.isSVG())
        return;

    if (m_currentObject.isLayoutView()) {
        if (!m_currentObject.document().settings() || !m_currentObject.document().settings()->rootLayerScrolls()) {
            if (m_currentObject != m_paintInvalidationContainer) {
                m_paintOffset -= toLayoutView(m_currentObject).frameView()->scrollOffset();
                addClipRectRelativeToPaintOffset(toLayoutView(m_currentObject).viewRect());
            }
            return;
        }
    } else if (m_currentObject.isSVGRoot()) {
        const LayoutSVGRoot& svgRoot = toLayoutSVGRoot(m_currentObject);
        if (svgRoot.shouldApplyViewportClip())
            addClipRectRelativeToPaintOffset(LayoutRect(LayoutPoint(), LayoutSize(svgRoot.pixelSnappedSize())));
    } else if (m_currentObject.isTableRow()) {
        // Child table cell's locationOffset() includes its row's locationOffset().
        m_paintOffset -= toLayoutBox(m_currentObject).locationOffset();
    }

    if (!m_currentObject.hasOverflowClip())
        return;

    const LayoutBox& box = toLayoutBox(m_currentObject);

    // Do not clip scroll layer contents because the compositor expects the whole layer
    // to be always invalidated in-time.
    if (box == m_paintInvalidationContainer && box.scrollsOverflow())
        ASSERT(!m_clipped); // The box establishes paint invalidation container, so no m_clipped inherited.
    else
        addClipRectRelativeToPaintOffset(box.overflowClipRect(LayoutPoint()));

    m_paintOffset -= box.scrolledContentOffset();

    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

static FloatPoint slowLocalToAncestorPoint(const LayoutObject& object, const LayoutBoxModelObject& ancestor, const FloatPoint& point)
{
    if (object.isLayoutView())
        return toLayoutView(object).localToAncestorPoint(point, &ancestor, TraverseDocumentBoundaries | InputIsInFrameCoordinates);
    return object.localToAncestorPoint(point, &ancestor, TraverseDocumentBoundaries);
}

LayoutPoint PaintInvalidationState::computePositionFromPaintInvalidationBacking() const
{
    ASSERT(!m_didUpdateForChildren);

    FloatPoint point;
    if (m_paintInvalidationContainer != &m_currentObject) {
        if (m_cachedOffsetsEnabled) {
            if (m_currentObject.isSVG() && !m_currentObject.isSVGRoot())
                point = m_svgTransform.mapPoint(point);
            point += FloatPoint(m_paintOffset);
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
            // TODO(wangxianzhu): We can't enable this ASSERT for now because of crbug.com/597745.
            // ASSERT(point == slowLocalOriginToAncestorPoint(m_currentObject, m_paintInvalidationContainer, FloatPoint());
#endif
        } else {
            point = slowLocalToAncestorPoint(m_currentObject, *m_paintInvalidationContainer, FloatPoint());
        }
    }

    if (m_paintInvalidationContainer->layer()->groupedMapping())
        PaintLayer::mapPointInPaintInvalidationContainerToBacking(*m_paintInvalidationContainer, point);

    return LayoutPoint(point);
}

LayoutRect PaintInvalidationState::computePaintInvalidationRectInBacking() const
{
    ASSERT(!m_didUpdateForChildren);

    if (m_currentObject.isSVG() && !m_currentObject.isSVGRoot())
        return computePaintInvalidationRectInBackingForSVG();

    LayoutRect rect = m_currentObject.localOverflowRectForPaintInvalidation();
    mapLocalRectToPaintInvalidationBacking(rect);
    return rect;
}

LayoutRect PaintInvalidationState::computePaintInvalidationRectInBackingForSVG() const
{
    LayoutRect rect;
    if (m_cachedOffsetsEnabled) {
        FloatRect svgRect = SVGLayoutSupport::localOverflowRectForPaintInvalidation(m_currentObject);
        rect = SVGLayoutSupport::transformPaintInvalidationRect(m_currentObject, m_svgTransform, svgRect);
        rect.move(m_paintOffset);
        if (m_clipped)
            rect.intersect(m_clipRect);
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
        LayoutRect slowPathRect = SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(m_currentObject, *m_paintInvalidationContainer);
        assertFastPathAndSlowPathRectsEqual(rect, slowPathRect);
#endif
    } else {
        // TODO(wangxianzhu): Sometimes m_cachedOffsetsEnabled==false doesn't mean we can't use cached
        // m_svgTransform. We can use hybrid fast path (for SVG) and slow path (for things above the SVGRoot).
        rect = SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(m_currentObject, *m_paintInvalidationContainer);
    }

    if (m_paintInvalidationContainer->layer()->groupedMapping())
        PaintLayer::mapRectInPaintInvalidationContainerToBacking(*m_paintInvalidationContainer, rect);
    return rect;
}

static void slowMapToVisualRectInAncestorSpace(const LayoutObject& object, const LayoutBoxModelObject& ancestor, LayoutRect& rect)
{
    if (object.isLayoutView())
        toLayoutView(object).mapToVisualRectInAncestorSpace(&ancestor, rect, InputIsInFrameCoordinates, DefaultVisualRectFlags);
    else
        object.mapToVisualRectInAncestorSpace(&ancestor, rect);
}

void PaintInvalidationState::mapLocalRectToPaintInvalidationContainer(LayoutRect& rect) const
{
    ASSERT(!m_didUpdateForChildren);

    if (m_cachedOffsetsEnabled) {
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
        LayoutRect slowPathRect(rect);
        slowMapToVisualRectInAncestorSpace(m_currentObject, *m_paintInvalidationContainer, slowPathRect);
#endif
        rect.move(m_paintOffset);
        if (m_clipped)
            rect.intersect(m_clipRect);
#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY
        assertFastPathAndSlowPathRectsEqual(rect, slowPathRect);
#endif
    } else {
        slowMapToVisualRectInAncestorSpace(m_currentObject, *m_paintInvalidationContainer, rect);
    }
}

void PaintInvalidationState::mapLocalRectToPaintInvalidationBacking(LayoutRect& rect) const
{
    mapLocalRectToPaintInvalidationContainer(rect);

    if (m_paintInvalidationContainer->layer()->groupedMapping())
        PaintLayer::mapRectInPaintInvalidationContainerToBacking(*m_paintInvalidationContainer, rect);
}

void PaintInvalidationState::addClipRectRelativeToPaintOffset(const LayoutRect& localClipRect)
{
    LayoutRect clipRect = localClipRect;
    clipRect.move(m_paintOffset);
    if (m_clipped) {
        m_clipRect.intersect(clipRect);
    } else {
        m_clipRect = clipRect;
        m_clipped = true;
    }
}

PaintLayer& PaintInvalidationState::paintingLayer() const
{
    ASSERT(&m_paintingLayer == m_currentObject.paintingLayer());
    return m_paintingLayer;
}

#ifdef CHECK_FAST_PATH_SLOW_PATH_EQUALITY

static bool mayHaveBeenSaturated(LayoutUnit value)
{
    // This is not accurate, just to avoid too big values.
    return value.abs() >= LayoutUnit::max() / 2;
}

static bool mayHaveBeenSaturated(const LayoutRect& rect)
{
    return mayHaveBeenSaturated(rect.x()) || mayHaveBeenSaturated(rect.y()) || mayHaveBeenSaturated(rect.width()) || mayHaveBeenSaturated(rect.height());
}

void PaintInvalidationState::assertFastPathAndSlowPathRectsEqual(const LayoutRect& fastPathRect, const LayoutRect& slowPathRect) const
{
    if (!m_canCheckFastPathSlowPathEquality)
        return;

    // TODO(crbug.com/597903): Fast path and slow path should generate equal empty rects.
    if (fastPathRect.isEmpty() && slowPathRect.isEmpty())
        return;

    if (fastPathRect == slowPathRect)
        return;

    // LayoutUnit uses saturated arithmetic operations. If any interim or final result is saturated,
    // the same operations in different order produce different results. Don't compare results
    // if any of them may have been saturated.
    if (mayHaveBeenSaturated(fastPathRect) || mayHaveBeenSaturated(slowPathRect))
        return;

    // Tolerate the difference between the two paths when crossing frame boundaries.
    if (m_currentObject.view() != m_paintInvalidationContainer->view()) {
        LayoutRect inflatedFastPathRect = fastPathRect;
        inflatedFastPathRect.inflate(1);
        if (inflatedFastPathRect.contains(slowPathRect))
            return;
        LayoutRect inflatedSlowPathRect = slowPathRect;
        inflatedSlowPathRect.inflate(1);
        if (inflatedSlowPathRect.contains(fastPathRect))
            return;
    }

    WTFLogAlways("Fast path paint invalidation rect differs from slow path: fast: %s vs slow: %s",
        fastPathRect.toString().ascii().data(), slowPathRect.toString().ascii().data());
    showLayoutTree(&m_currentObject);

    ASSERT_NOT_REACHED();
}

#endif // CHECK_FAST_PATH_SLOW_PATH_EQUALITY

} // namespace blink
