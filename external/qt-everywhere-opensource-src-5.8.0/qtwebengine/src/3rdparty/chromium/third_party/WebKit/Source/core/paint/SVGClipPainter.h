// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGClipPainter_h
#define SVGClipPainter_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItemClient.h"
#include "wtf/Allocator.h"

namespace blink {

class AffineTransform;
class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceClipper;

class SVGClipPainter {
    STACK_ALLOCATED();
public:
    enum ClipperState {
        ClipperNotApplied,
        ClipperAppliedPath,
        ClipperAppliedMask
    };

    SVGClipPainter(LayoutSVGResourceClipper& clip) : m_clip(clip) { }

    // FIXME: Filters are also stateful resources that could benefit from having their state managed
    //        on the caller stack instead of the current hashmap. We should look at refactoring these
    //        into a general interface that can be shared.
    bool prepareEffect(const LayoutObject&, const FloatRect&, const FloatRect&, const FloatPoint&, GraphicsContext&, ClipperState&);
    void finishEffect(const LayoutObject&, GraphicsContext&, ClipperState&);

private:
    // Return false if there is a problem drawing the mask.
    bool drawClipAsMask(GraphicsContext&, const LayoutObject&, const FloatRect& targetBoundingBox,
        const FloatRect& targetPaintInvalidationRect, const AffineTransform&, const FloatPoint&);

    LayoutSVGResourceClipper& m_clip;
};

} // namespace blink

#endif // SVGClipPainter_h
