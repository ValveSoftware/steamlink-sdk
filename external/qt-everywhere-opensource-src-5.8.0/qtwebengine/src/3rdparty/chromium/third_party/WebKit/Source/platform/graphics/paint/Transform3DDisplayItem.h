// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transform3DDisplayItem_h
#define Transform3DDisplayItem_h

#include "platform/geometry/FloatPoint3D.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/transforms/TransformationMatrix.h"
#include "wtf/Assertions.h"

namespace blink {

class PLATFORM_EXPORT BeginTransform3DDisplayItem final : public PairedBeginDisplayItem {
public:
    BeginTransform3DDisplayItem(
        const DisplayItemClient& client,
        Type type,
        const TransformationMatrix& transform,
        const FloatPoint3D& transformOrigin)
        : PairedBeginDisplayItem(client, type, sizeof(*this))
        , m_transform(transform)
        , m_transformOrigin(transformOrigin)
    {
        ASSERT(DisplayItem::isTransform3DType(type));
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

    const TransformationMatrix& transform() const { return m_transform; }
    const FloatPoint3D& transformOrigin() const { return m_transformOrigin; }

private:
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(WTF::StringBuilder&) const final;
#endif
#if ENABLE(ASSERT)
    bool equals(const DisplayItem& other) const final
    {
        return DisplayItem::equals(other)
            && m_transform == static_cast<const BeginTransform3DDisplayItem&>(other).m_transform
            && m_transformOrigin == static_cast<const BeginTransform3DDisplayItem&>(other).m_transformOrigin;
    }
#endif

    const TransformationMatrix m_transform;
    const FloatPoint3D m_transformOrigin;
};

class PLATFORM_EXPORT EndTransform3DDisplayItem final : public PairedEndDisplayItem {
public:
    EndTransform3DDisplayItem(const DisplayItemClient& client, Type type)
        : PairedEndDisplayItem(client, type, sizeof(*this))
    {
        ASSERT(DisplayItem::isEndTransform3DType(type));
    }

    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;

private:
#if ENABLE(ASSERT)
    bool isEndAndPairedWith(DisplayItem::Type otherType) const final
    {
        return DisplayItem::transform3DTypeToEndTransform3DType(otherType) == getType();
    }
#endif
};

} // namespace blink

#endif // Transform3DDisplayItem_h
