// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ClipDisplayItem_h
#define ClipDisplayItem_h

#include "SkRegion.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRoundedRect.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "wtf/Vector.h"

namespace blink {

class PLATFORM_EXPORT ClipDisplayItem final : public PairedBeginDisplayItem {
public:
    ClipDisplayItem(const DisplayItemClient& client, Type type, const IntRect& clipRect)
        : PairedBeginDisplayItem(client, type, sizeof(*this))
        , m_clipRect(clipRect)
    {
        ASSERT(isClipType(type));
    }

    ClipDisplayItem(const DisplayItemClient& client, Type type, const IntRect& clipRect, Vector<FloatRoundedRect>& roundedRectClips)
        : ClipDisplayItem(client, type, clipRect)
    {
        m_roundedRectClips.swap(roundedRectClips);
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
            && m_clipRect == static_cast<const ClipDisplayItem&>(other).m_clipRect
            && m_roundedRectClips == static_cast<const ClipDisplayItem&>(other).m_roundedRectClips;
    }
#endif

    const IntRect m_clipRect;
    Vector<FloatRoundedRect> m_roundedRectClips;
};

class PLATFORM_EXPORT EndClipDisplayItem final : public PairedEndDisplayItem {
public:
    EndClipDisplayItem(const DisplayItemClient& client, Type type)
        : PairedEndDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(isEndClipType(type));
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return DisplayItem::isClipType(otherType); }
#endif
};

} // namespace blink

#endif // ClipDisplayItem_h
