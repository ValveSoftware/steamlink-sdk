// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPainter_h
#define ObjectPainter_h

#include "core/style/ComputedStyleConstants.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

namespace blink {

class Color;
class GraphicsContext;
class LayoutPoint;
class LayoutRect;
class LayoutSize;
struct PaintInfo;
class LayoutObject;
class ComputedStyle;

class ObjectPainter {
    STACK_ALLOCATED();
public:
    ObjectPainter(const LayoutObject& layoutObject) : m_layoutObject(layoutObject) { }

    void paintOutline(const PaintInfo&, const LayoutPoint& paintOffset);
    void paintInlineChildrenOutlines(const PaintInfo&, const LayoutPoint& paintOffset);
    void addPDFURLRectIfNeeded(const PaintInfo&, const LayoutPoint& paintOffset);

    static void drawLineForBoxSide(GraphicsContext&, int x1, int y1, int x2, int y2, BoxSide, Color, EBorderStyle, int adjbw1, int adjbw2, bool antialias = false);

    // Paints the object atomically as if it created a new stacking context, for:
    // - inline blocks, inline tables, inline-level replaced elements (Section 7.2.1.4 in
    //   http://www.w3.org/TR/CSS2/zindex.html#painting-order),
    // - non-positioned floating objects (Section 5 in http://www.w3.org/TR/CSS2/zindex.html#painting-order),
    // - flex items (http://www.w3.org/TR/css-flexbox-1/#painting),
    // - grid items (http://www.w3.org/TR/css-grid-1/#z-order),
    // - custom scrollbar parts.
    // Also see https://chromium.googlesource.com/chromium/src.git/+/master/third_party/WebKit/Source/core/paint/README.md.
    //
    // It is expected that the caller will call this function independent of the value of paintInfo.phase,
    // and this function will do atomic paint (for PaintPhaseForeground), normal paint (for PaintPhaseSelection
    // and PaintPhaseTextClip) or nothing (other paint phases) according to paintInfo.phase.
    void paintAllPhasesAtomically(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    static void drawDashedOrDottedBoxSide(GraphicsContext&, int x1, int y1, int x2, int y2,
        BoxSide, Color, int thickness, EBorderStyle, bool antialias);
    static void drawDoubleBoxSide(GraphicsContext&, int x1, int y1, int x2, int y2,
        int length, BoxSide, Color, int thickness, int adjacentWidth1, int adjacentWidth2, bool antialias);
    static void drawRidgeOrGrooveBoxSide(GraphicsContext&, int x1, int y1, int x2, int y2,
        BoxSide, Color, EBorderStyle, int adjacentWidth1, int adjacentWidth2, bool antialias);
    static void drawSolidBoxSide(GraphicsContext&, int x1, int y1, int x2, int y2,
        BoxSide, Color, int adjacentWidth1, int adjacentWidth2, bool antialias);

    const LayoutObject& m_layoutObject;
};

} // namespace blink

#endif
