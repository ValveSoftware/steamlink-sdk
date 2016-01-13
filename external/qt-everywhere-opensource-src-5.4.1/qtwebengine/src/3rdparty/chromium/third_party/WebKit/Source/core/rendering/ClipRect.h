/*
 * Copyright (C) 2003, 2009, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ClipRect_h
#define ClipRect_h

#include "platform/geometry/LayoutRect.h"

#include "wtf/Vector.h"

#ifndef NDEBUG
#include "core/rendering/RenderBox.h" // For OverlayScrollbarSizeRelevancy.
#endif

namespace WebCore {

class RenderLayer;
class HitTestLocation;

class ClipRect {
public:
    ClipRect()
        : m_hasRadius(false)
    { }

    ClipRect(const LayoutRect& rect)
        : m_rect(rect)
        , m_hasRadius(false)
    { }

    const LayoutRect& rect() const { return m_rect; }
    void setRect(const LayoutRect& rect) { m_rect = rect; }

    bool hasRadius() const { return m_hasRadius; }
    void setHasRadius(bool hasRadius) { m_hasRadius = hasRadius; }

    bool operator==(const ClipRect& other) const { return rect() == other.rect() && hasRadius() == other.hasRadius(); }
    bool operator!=(const ClipRect& other) const { return rect() != other.rect() || hasRadius() != other.hasRadius(); }
    bool operator!=(const LayoutRect& otherRect) const { return rect() != otherRect; }

    void intersect(const LayoutRect& other) { m_rect.intersect(other); }
    void intersect(const ClipRect& other)
    {
        m_rect.intersect(other.rect());
        if (other.hasRadius())
            m_hasRadius = true;
    }
    void move(LayoutUnit x, LayoutUnit y) { m_rect.move(x, y); }
    void move(const LayoutSize& size) { m_rect.move(size); }
    void moveBy(const LayoutPoint& point) { m_rect.moveBy(point); }

    bool isEmpty() const { return m_rect.isEmpty(); }
    bool intersects(const LayoutRect& rect) const { return m_rect.intersects(rect); }
    bool intersects(const HitTestLocation&) const;

private:
    LayoutRect m_rect;
    bool m_hasRadius;
};

inline ClipRect intersection(const ClipRect& a, const ClipRect& b)
{
    ClipRect c = a;
    c.intersect(b);
    return c;
}

class ClipRects {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<ClipRects> create()
    {
        return adoptRef(new ClipRects);
    }

    static PassRefPtr<ClipRects> create(const ClipRects& other)
    {
        return adoptRef(new ClipRects(other));
    }

    ClipRects()
        : m_refCnt(1)
        , m_fixed(0)
    {
    }

    void reset(const LayoutRect& r)
    {
        m_overflowClipRect = r;
        m_fixedClipRect = r;
        m_posClipRect = r;
        m_fixed = 0;
    }

    const ClipRect& overflowClipRect() const { return m_overflowClipRect; }
    void setOverflowClipRect(const ClipRect& r) { m_overflowClipRect = r; }

    const ClipRect& fixedClipRect() const { return m_fixedClipRect; }
    void setFixedClipRect(const ClipRect&r) { m_fixedClipRect = r; }

    const ClipRect& posClipRect() const { return m_posClipRect; }
    void setPosClipRect(const ClipRect& r) { m_posClipRect = r; }

    bool fixed() const { return static_cast<bool>(m_fixed); }
    void setFixed(bool fixed) { m_fixed = fixed ? 1 : 0; }

    void ref() { m_refCnt++; }
    void deref()
    {
        if (!--m_refCnt)
            delete this;
    }

    bool operator==(const ClipRects& other) const
    {
        return m_overflowClipRect == other.overflowClipRect()
            && m_fixedClipRect == other.fixedClipRect()
            && m_posClipRect == other.posClipRect()
            && fixed() == other.fixed();
    }

    ClipRects& operator=(const ClipRects& other)
    {
        m_overflowClipRect = other.overflowClipRect();
        m_fixedClipRect = other.fixedClipRect();
        m_posClipRect = other.posClipRect();
        m_fixed = other.fixed();
        return *this;
    }

private:
    ClipRects(const LayoutRect& r)
        : m_overflowClipRect(r)
        , m_fixedClipRect(r)
        , m_posClipRect(r)
        , m_refCnt(1)
        , m_fixed(0)
    {
    }

    ClipRects(const ClipRects& other)
        : m_overflowClipRect(other.overflowClipRect())
        , m_fixedClipRect(other.fixedClipRect())
        , m_posClipRect(other.posClipRect())
        , m_refCnt(1)
        , m_fixed(other.fixed())
    {
    }

    ClipRect m_overflowClipRect;
    ClipRect m_fixedClipRect;
    ClipRect m_posClipRect;
    unsigned m_refCnt : 31;
    unsigned m_fixed : 1;
};

enum ClipRectsType {
    PaintingClipRects, // Relative to painting ancestor. Used for painting.
    RootRelativeClipRects, // Relative to the ancestor treated as the root (e.g. transformed layer). Used for hit testing.
    CompositingClipRects, // Relative to the compositing ancestor. Used for updating graphics layer geometry.
    AbsoluteClipRects, // Relative to the RenderView's layer. Used for compositing overlap testing.
    NumCachedClipRectsTypes,
    AllClipRectTypes,
    TemporaryClipRects
};

enum ShouldRespectOverflowClip {
    IgnoreOverflowClip,
    RespectOverflowClip
};

struct ClipRectsCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClipRectsCache()
    {
        for (int i = 0; i < NumCachedClipRectsTypes; ++i) {
            m_clipRectsRoot[i] = 0;
#ifndef NDEBUG
            m_scrollbarRelevancy[i] = IgnoreOverlayScrollbarSize;
#endif
        }
    }

    PassRefPtr<ClipRects> getClipRects(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow) { return m_clipRects[getIndex(clipRectsType, respectOverflow)]; }
    void setClipRects(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow, PassRefPtr<ClipRects> clipRects, const RenderLayer* root)
    {
        m_clipRects[getIndex(clipRectsType, respectOverflow)] = clipRects;
        m_clipRectsRoot[clipRectsType] = root;
    }

    const RenderLayer* clipRectsRoot(ClipRectsType clipRectsType) const { return m_clipRectsRoot[clipRectsType]; }

#ifndef NDEBUG
    OverlayScrollbarSizeRelevancy m_scrollbarRelevancy[NumCachedClipRectsTypes];
#endif

private:
    int getIndex(ClipRectsType clipRectsType, ShouldRespectOverflowClip respectOverflow)
    {
        int index = static_cast<int>(clipRectsType);
        if (respectOverflow == RespectOverflowClip)
            index += static_cast<int>(NumCachedClipRectsTypes);
        return index;
    }

    const RenderLayer* m_clipRectsRoot[NumCachedClipRectsTypes];
    RefPtr<ClipRects> m_clipRects[NumCachedClipRectsTypes * 2];
};

struct LayerFragment {
public:
    LayerFragment()
        : shouldPaintContent(false)
    { }

    void setRects(const LayoutRect& bounds, const ClipRect& background, const ClipRect& foreground, const ClipRect& outline)
    {
        layerBounds = bounds;
        backgroundRect = background;
        foregroundRect = foreground;
        outlineRect = outline;
    }

    void moveBy(const LayoutPoint& offset)
    {
        layerBounds.moveBy(offset);
        backgroundRect.moveBy(offset);
        foregroundRect.moveBy(offset);
        outlineRect.moveBy(offset);
        paginationClip.moveBy(offset);
    }

    void intersect(const LayoutRect& rect)
    {
        backgroundRect.intersect(rect);
        foregroundRect.intersect(rect);
        outlineRect.intersect(rect);
    }

    bool shouldPaintContent;
    LayoutRect layerBounds;
    ClipRect backgroundRect;
    ClipRect foregroundRect;
    ClipRect outlineRect;

    // Unique to paginated fragments. The physical translation to apply to shift the layer when painting/hit-testing.
    LayoutPoint paginationOffset;

    // Also unique to paginated fragments. An additional clip that applies to the layer. It is in layer-local
    // (physical) coordinates.
    LayoutRect paginationClip;
};

typedef Vector<LayerFragment, 1> LayerFragments;

} // namespace WebCore

#endif // ClipRect_h
