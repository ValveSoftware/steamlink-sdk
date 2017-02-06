// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "SnapCoordinator.h"

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutView.h"
#include "core/style/ComputedStyle.h"
#include "platform/LengthFunctions.h"

namespace blink {

SnapCoordinator::SnapCoordinator()
    : m_snapContainers()
{
}

SnapCoordinator::~SnapCoordinator() { }

SnapCoordinator* SnapCoordinator::create()
{
    return new SnapCoordinator();
}

// Returns the scroll container that can be affected by this snap area.
static LayoutBox* findSnapContainer(const LayoutBox& snapArea)
{
    // According to the new spec https://drafts.csswg.org/css-scroll-snap/#snap-model
    // "Snap positions must only affect the nearest ancestor (on the element’s
    // containing block chain) scroll container".
    Element* viewportDefiningElement = snapArea.node()->document().viewportDefiningElement();
    LayoutBox* box = snapArea.containingBlock();
    while (box && !box->hasOverflowClip() && !box->isLayoutView() && box->node() != viewportDefiningElement)
        box = box->containingBlock();

    // If we reach to viewportDefiningElement then we dispatch to viewport
    if (box->node() == viewportDefiningElement)
        return snapArea.node()->document().layoutView();

    return box;
}

void SnapCoordinator::snapAreaDidChange(LayoutBox& snapArea, const Vector<LengthPoint>& snapCoordinates)
{
    if (snapCoordinates.isEmpty()) {
        snapArea.setSnapContainer(nullptr);
        return;
    }

    if (LayoutBox* snapContainer = findSnapContainer(snapArea)) {
        snapArea.setSnapContainer(snapContainer);
    } else {
        // TODO(majidvp): keep track of snap areas that do not have any
        // container so that we check them again when a new container is
        // added to the page.
    }
}

void SnapCoordinator::snapContainerDidChange(LayoutBox& snapContainer, ScrollSnapType scrollSnapType)
{
    if (scrollSnapType == ScrollSnapTypeNone) {
        // TODO(majidvp): Track and report these removals to CompositorWorker
        // instance responsible for snapping
        m_snapContainers.remove(&snapContainer);
        snapContainer.clearSnapAreas();
    } else {
        m_snapContainers.add(&snapContainer);
    }

    // TODO(majidvp): Add logic to correctly handle orphaned snap areas here.
    // 1. Removing container: find a new snap container for its orphan snap
    // areas (most likely nearest ancestor of current container) otherwise add
    // them to an orphan list.
    // 2. Adding container: may take over snap areas from nearest ancestor snap
    // container or from existing areas in orphan pool.
}

// Translate local snap coordinates into snap container's scrolling content
// coordinate space.
static Vector<FloatPoint> localToContainerSnapCoordinates(const LayoutBox& containerBox, const LayoutBox& snapArea)
{
    Vector<FloatPoint> result;
    LayoutPoint scrollOffset(containerBox.scrollLeft(), containerBox.scrollTop());

    const Vector<LengthPoint>& snapCoordinates = snapArea.style()->scrollSnapCoordinate();
    for (auto& coordinate : snapCoordinates) {
        FloatPoint localPoint = floatPointForLengthPoint(coordinate, FloatSize(snapArea.size()));
        FloatPoint containerPoint = snapArea.localToAncestorPoint(localPoint, &containerBox);
        containerPoint.moveBy(scrollOffset);
        result.append(containerPoint);
    }
    return result;
}

Vector<double> SnapCoordinator::snapOffsets(const ContainerNode& element, ScrollbarOrientation orientation)
{
    const ComputedStyle* style = element.computedStyle();
    const LayoutBox* snapContainer = element.layoutBox();
    ASSERT(style);
    ASSERT(snapContainer);

    Vector<double> result;

    if (style->getScrollSnapType() == ScrollSnapTypeNone)
        return result;

    const ScrollSnapPoints& snapPoints = (orientation == HorizontalScrollbar) ? style->scrollSnapPointsX() : style->scrollSnapPointsY();

    LayoutUnit clientSize = (orientation == HorizontalScrollbar) ? snapContainer->clientWidth() : snapContainer->clientHeight();
    LayoutUnit scrollSize = (orientation == HorizontalScrollbar) ? snapContainer->scrollWidth() : snapContainer->scrollHeight();

    if (snapPoints.hasRepeat) {
        LayoutUnit repeat = valueForLength(snapPoints.repeatOffset, clientSize);

        // calc() values may be negative or zero in which case we clamp them to 1px.
        // See: https://lists.w3.org/Archives/Public/www-style/2015Jul/0075.html
        repeat = std::max<LayoutUnit>(repeat, LayoutUnit(1));
        for (LayoutUnit offset = repeat; offset <= (scrollSize - clientSize); offset += repeat) {
            result.append(offset.toFloat());
        }
    }

    // Compute element-based snap points by mapping the snap coordinates from
    // snap areas to snap container.
    bool didAddSnapAreaOffset = false;
    if (SnapAreaSet* snapAreas = snapContainer->snapAreas()) {
        for (auto& snapArea : *snapAreas) {
            Vector<FloatPoint> snapCoordinates = localToContainerSnapCoordinates(*snapContainer, *snapArea);
            for (const FloatPoint& snapCoordinate : snapCoordinates) {
                float snapOffset = (orientation == HorizontalScrollbar) ? snapCoordinate.x() : snapCoordinate.y();
                if (snapOffset > scrollSize - clientSize)
                    continue;
                result.append(snapOffset);
                didAddSnapAreaOffset = true;
            }
        }
    }

    if (didAddSnapAreaOffset)
        std::sort(result.begin(), result.end());

    return result;
}

#ifndef NDEBUG

void SnapCoordinator::showSnapAreaMap()
{
    for (auto& container : m_snapContainers)
        showSnapAreasFor(container);
}

void SnapCoordinator::showSnapAreasFor(const LayoutBox* container)
{
    const char* prefix = "    ";
    container->node()->showNode();
    if (SnapAreaSet* snapAreas = container->snapAreas()) {
        for (auto& snapArea : *snapAreas) {
            snapArea->node()->showNode(prefix);
        }
    }
}

#endif

} // namespace blink
