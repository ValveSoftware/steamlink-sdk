// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CullRect_h
#define CullRect_h

#include "platform/geometry/IntRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"

#include <limits>

namespace blink {

class FloatRect;
class LayoutBoxModelObject;
class LayoutInline;
class LayoutObject;
class LayoutRect;
class LayoutUnit;
class PaintInvalidationState;

class PLATFORM_EXPORT CullRect {
    DISALLOW_NEW();
public:
    explicit CullRect(const IntRect& rect) : m_rect(rect) { }
    CullRect(const CullRect&, const IntPoint& offset);
    CullRect(const CullRect&, const IntSize& offset);

    bool intersectsCullRect(const AffineTransform&, const FloatRect& boundingBox) const;
    void updateCullRect(const AffineTransform& localToParentTransform);
    bool intersectsCullRect(const IntRect&) const;
    bool intersectsCullRect(const LayoutRect&) const;
    bool intersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const;
    bool intersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const;

private:
    IntRect m_rect;

    // TODO(chrishtr): temporary while we implement CullRect everywhere.
    friend class FramePainter;
    friend class GridPainter;
    friend class SVGInlineTextBoxPainter;
    friend class ReplicaPainter;
    friend class SVGPaintContext;
    friend class SVGRootInlineBoxPainter;
    friend class SVGShapePainter;
    friend class TableSectionPainter;
    friend class ThemePainterMac;
    friend class WebPluginContainerImpl;
};

} // namespace blink

#endif // CullRect_h
