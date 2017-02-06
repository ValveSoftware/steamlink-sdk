// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingDisplayItem_h
#define CompositingDisplayItem_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/GraphicsTypes.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "public/platform/WebBlendMode.h"
#ifndef NDEBUG
#include "wtf/text/WTFString.h"
#endif

namespace blink {

class PLATFORM_EXPORT BeginCompositingDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginCompositingDisplayItem(const DisplayItemClient& client, const SkXfermode::Mode xferMode, const float opacity, const FloatRect* bounds, ColorFilter colorFilter = ColorFilterNone)
        : PairedBeginDisplayItem(client, BeginCompositing, sizeof(*this))
        , m_xferMode(xferMode)
        , m_opacity(opacity)
        , m_hasBounds(bounds)
        , m_colorFilter(colorFilter)
    {
        if (bounds)
            m_bounds = FloatRect(*bounds);
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const override;
#endif
#if ENABLE(ASSERT)
    bool equals(const DisplayItem& other) const final
    {
        return DisplayItem::equals(other)
            && m_xferMode == static_cast<const BeginCompositingDisplayItem&>(other).m_xferMode
            && m_opacity == static_cast<const BeginCompositingDisplayItem&>(other).m_opacity
            && m_hasBounds == static_cast<const BeginCompositingDisplayItem&>(other).m_hasBounds
            && m_bounds == static_cast<const BeginCompositingDisplayItem&>(other).m_bounds
            && m_colorFilter == static_cast<const BeginCompositingDisplayItem&>(other).m_colorFilter;
    }
#endif

    const SkXfermode::Mode m_xferMode;
    const float m_opacity;
    bool m_hasBounds;
    FloatRect m_bounds;
    ColorFilter m_colorFilter;
};

class PLATFORM_EXPORT EndCompositingDisplayItem final : public PairedEndDisplayItem {
public:
    EndCompositingDisplayItem(const DisplayItemClient& client)
        : PairedEndDisplayItem(client, EndCompositing, sizeof(*this)) { }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return otherType == BeginCompositing; }
#endif
};

} // namespace blink

#endif // CompositingDisplayItem_h
