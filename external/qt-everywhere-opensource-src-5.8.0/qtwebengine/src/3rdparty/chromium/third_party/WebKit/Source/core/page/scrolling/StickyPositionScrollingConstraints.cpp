// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/StickyPositionScrollingConstraints.h"

namespace blink {

FloatSize StickyPositionScrollingConstraints::computeStickyOffset(const FloatRect& viewportRect) const
{
    FloatRect boxRect = m_scrollContainerRelativeStickyBoxRect;

    if (hasAnchorEdge(AnchorEdgeRight)) {
        float rightLimit = viewportRect.maxX() - m_rightOffset;
        float rightDelta = std::min<float>(0, rightLimit - m_scrollContainerRelativeStickyBoxRect.maxX());
        float availableSpace = std::min<float>(0, m_scrollContainerRelativeContainingBlockRect.x() - m_scrollContainerRelativeStickyBoxRect.x());
        if (rightDelta < availableSpace)
            rightDelta = availableSpace;

        boxRect.move(rightDelta, 0);
    }

    if (hasAnchorEdge(AnchorEdgeLeft)) {
        float leftLimit = viewportRect.x() + m_leftOffset;
        float leftDelta = std::max<float>(0, leftLimit - m_scrollContainerRelativeStickyBoxRect.x());
        float availableSpace = std::max<float>(0, m_scrollContainerRelativeContainingBlockRect.maxX() - m_scrollContainerRelativeStickyBoxRect.maxX());
        if (leftDelta > availableSpace)
            leftDelta = availableSpace;

        boxRect.move(leftDelta, 0);
    }

    if (hasAnchorEdge(AnchorEdgeBottom)) {
        float bottomLimit = viewportRect.maxY() - m_bottomOffset;
        float bottomDelta = std::min<float>(0, bottomLimit - m_scrollContainerRelativeStickyBoxRect.maxY());
        float availableSpace = std::min<float>(0, m_scrollContainerRelativeContainingBlockRect.y() - m_scrollContainerRelativeStickyBoxRect.y());
        if (bottomDelta < availableSpace)
            bottomDelta = availableSpace;

        boxRect.move(0, bottomDelta);
    }

    if (hasAnchorEdge(AnchorEdgeTop)) {
        float topLimit = viewportRect.y() + m_topOffset;
        float topDelta = std::max<float>(0, topLimit - m_scrollContainerRelativeStickyBoxRect.y());
        float availableSpace = std::max<float>(0, m_scrollContainerRelativeContainingBlockRect.maxY() - m_scrollContainerRelativeStickyBoxRect.maxY());
        if (topDelta > availableSpace)
            topDelta = availableSpace;

        boxRect.move(0, topDelta);
    }

    return boxRect.location() - m_scrollContainerRelativeStickyBoxRect.location();
}

} // namespace blink
