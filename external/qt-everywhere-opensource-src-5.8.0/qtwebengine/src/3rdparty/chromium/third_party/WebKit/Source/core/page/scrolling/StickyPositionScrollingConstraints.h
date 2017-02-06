// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StickyPositionScrollingConstraints_h
#define StickyPositionScrollingConstraints_h

#include "platform/geometry/FloatRect.h"

namespace blink {

class StickyPositionScrollingConstraints final {
public:
    enum AnchorEdgeFlags {
        AnchorEdgeLeft = 1 << 0,
        AnchorEdgeRight = 1 << 1,
        AnchorEdgeTop = 1 << 2,
        AnchorEdgeBottom = 1 << 3
    };
    typedef unsigned AnchorEdges;

    StickyPositionScrollingConstraints()
        : m_anchorEdges(0)
        , m_leftOffset(0)
        , m_rightOffset(0)
        , m_topOffset(0)
        , m_bottomOffset(0)
    { }

    StickyPositionScrollingConstraints(const StickyPositionScrollingConstraints& other)
        : m_anchorEdges(other.m_anchorEdges)
        , m_leftOffset(other.m_leftOffset)
        , m_rightOffset(other.m_rightOffset)
        , m_topOffset(other.m_topOffset)
        , m_bottomOffset(other.m_bottomOffset)
        , m_scrollContainerRelativeContainingBlockRect(other.m_scrollContainerRelativeContainingBlockRect)
        , m_scrollContainerRelativeStickyBoxRect(other.m_scrollContainerRelativeStickyBoxRect)
    { }

    FloatSize computeStickyOffset(const FloatRect& viewportRect) const;

    AnchorEdges anchorEdges() const { return m_anchorEdges; }
    bool hasAnchorEdge(AnchorEdgeFlags flag) const { return m_anchorEdges & flag; }
    void addAnchorEdge(AnchorEdgeFlags edgeFlag) { m_anchorEdges |= edgeFlag; }
    void setAnchorEdges(AnchorEdges edges) { m_anchorEdges = edges; }

    float leftOffset() const { return m_leftOffset; }
    float rightOffset() const { return m_rightOffset; }
    float topOffset() const { return m_topOffset; }
    float bottomOffset() const { return m_bottomOffset; }

    void setLeftOffset(float offset) { m_leftOffset = offset; }
    void setRightOffset(float offset) { m_rightOffset = offset; }
    void setTopOffset(float offset) { m_topOffset = offset; }
    void setBottomOffset(float offset) { m_bottomOffset = offset; }

    void setScrollContainerRelativeContainingBlockRect(const FloatRect& rect) { m_scrollContainerRelativeContainingBlockRect = rect; }

    void setScrollContainerRelativeStickyBoxRect(const FloatRect& rect) { m_scrollContainerRelativeStickyBoxRect = rect; }

    bool operator==(const StickyPositionScrollingConstraints& other) const
    {
        return m_leftOffset == other.m_leftOffset
            && m_rightOffset == other.m_rightOffset
            && m_topOffset == other.m_topOffset
            && m_bottomOffset == other.m_bottomOffset
            && m_scrollContainerRelativeContainingBlockRect == other.m_scrollContainerRelativeContainingBlockRect
            && m_scrollContainerRelativeStickyBoxRect == other.m_scrollContainerRelativeStickyBoxRect;
    }

    bool operator!=(const StickyPositionScrollingConstraints& other) const { return !(*this == other); }

private:
    AnchorEdges m_anchorEdges;
    float m_leftOffset;
    float m_rightOffset;
    float m_topOffset;
    float m_bottomOffset;
    FloatRect m_scrollContainerRelativeContainingBlockRect;
    FloatRect m_scrollContainerRelativeStickyBoxRect;
};

} // namespace blink

#endif // StickyPositionScrollingConstraints_h
