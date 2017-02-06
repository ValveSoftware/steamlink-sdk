// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForeignLayerDisplayItem_h
#define ForeignLayerDisplayItem_h

#include "base/memory/ref_counted.h"
#include "platform/PlatformExport.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace cc {
class Layer;
}

namespace blink {

class GraphicsContext;
class WebLayer;

// Represents foreign content (produced outside Blink) which draws to a layer.
// A client supplies a layer which can be unwrapped and inserted into the full
// layer tree.
//
// Before SPv2, this content is not painted, but is instead inserted into the
// GraphicsLayer tree.
class PLATFORM_EXPORT ForeignLayerDisplayItem final : public DisplayItem {
public:
    ForeignLayerDisplayItem(
        const DisplayItemClient&, Type,
        scoped_refptr<cc::Layer>,
        const FloatPoint& location,
        const IntSize& bounds);
    ~ForeignLayerDisplayItem();

    cc::Layer* layer() const { return m_layer.get(); }
    const FloatPoint& location() const { return m_location; }
    const IntSize& bounds() const { return m_bounds; }

    // DisplayItem
    void replay(GraphicsContext&) const override;
    void appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const override;
    bool drawsContent() const override;
#if ENABLE(ASSERT)
    bool equals(const DisplayItem&) const override;
#endif
#ifndef NDEBUG
    void dumpPropertiesAsDebugString(StringBuilder&) const override;
#endif

private:
    scoped_refptr<cc::Layer> m_layer;
    FloatPoint m_location;
    IntSize m_bounds;
};

// Records a foreign layer into a GraphicsContext.
// Use this where you would use a recorder class.
PLATFORM_EXPORT void recordForeignLayer(
    GraphicsContext&, const DisplayItemClient&, DisplayItem::Type,
    WebLayer*, const FloatPoint& location, const IntSize& bounds);

} // namespace blink

#endif // ForeignLayerDisplayItem_h
