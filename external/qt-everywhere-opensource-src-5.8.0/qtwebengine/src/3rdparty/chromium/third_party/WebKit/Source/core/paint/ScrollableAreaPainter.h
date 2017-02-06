// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollableAreaPainter_h
#define ScrollableAreaPainter_h

#include "platform/heap/Handle.h"

namespace blink {

class CullRect;
class GraphicsContext;
class IntPoint;
class IntRect;
class PaintLayerScrollableArea;

class ScrollableAreaPainter {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ScrollableAreaPainter);
public:
    explicit ScrollableAreaPainter(PaintLayerScrollableArea& paintLayerScrollableArea) : m_scrollableArea(&paintLayerScrollableArea) { }

    void paintResizer(GraphicsContext&, const IntPoint& paintOffset, const CullRect&);
    void paintOverflowControls(GraphicsContext&, const IntPoint& paintOffset, const CullRect&, bool paintingOverlayControls);
    void paintScrollCorner(GraphicsContext&, const IntPoint&, const CullRect&);

private:
    void drawPlatformResizerImage(GraphicsContext&, IntRect resizerCornerRect);
    bool overflowControlsIntersectRect(const CullRect&) const;

    PaintLayerScrollableArea& getScrollableArea() const;

    Member<PaintLayerScrollableArea> m_scrollableArea;
};

} // namespace blink

#endif // ScrollableAreaPainter_h
