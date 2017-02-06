// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipDisplayItem_h
#define FloatClipDisplayItem_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class RoundedRect;

class PLATFORM_EXPORT FloatClipDisplayItem final : public PairedBeginDisplayItem {
public:
    FloatClipDisplayItem(const DisplayItemClient& client, Type type, const FloatRect& clipRect)
        : PairedBeginDisplayItem(client, type, sizeof(*this))
        , m_clipRect(clipRect)
    {
        ASSERT(isFloatClipType(type));
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
            && m_clipRect == static_cast<const FloatClipDisplayItem&>(other).m_clipRect;
    }
#endif

    const FloatRect m_clipRect;
};

class PLATFORM_EXPORT EndFloatClipDisplayItem final : public PairedEndDisplayItem {
public:
    EndFloatClipDisplayItem(const DisplayItemClient& client, Type type)
        : PairedEndDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(isEndFloatClipType(type));
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final { return DisplayItem::isFloatClipType(otherType); }
#endif
};

} // namespace blink

#endif // FloatClipDisplayItem_h
