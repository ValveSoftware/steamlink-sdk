// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ForeignLayerDisplayItem.h"

#include "cc/layers/layer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"
#include "public/platform/WebLayer.h"
#include "wtf/Assertions.h"
#include <utility>

namespace blink {

ForeignLayerDisplayItem::ForeignLayerDisplayItem(
    const DisplayItemClient& client, Type type,
    scoped_refptr<cc::Layer> layer,
    const FloatPoint& location, const IntSize& bounds)
    : DisplayItem(client, type, sizeof(*this))
    , m_layer(std::move(layer))
    , m_location(location)
    , m_bounds(bounds)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintV2Enabled());
    ASSERT(isForeignLayerType(type));
    ASSERT(m_layer);
}

ForeignLayerDisplayItem::~ForeignLayerDisplayItem()
{
}

void ForeignLayerDisplayItem::replay(GraphicsContext&) const
{
    ASSERT_NOT_REACHED();
}

void ForeignLayerDisplayItem::appendToWebDisplayItemList(const IntRect&, WebDisplayItemList*) const
{
    ASSERT_NOT_REACHED();
}

bool ForeignLayerDisplayItem::drawsContent() const
{
    return true;
}

#if ENABLE(ASSERT)
bool ForeignLayerDisplayItem::equals(const DisplayItem& other) const
{
    return DisplayItem::equals(other)
        && m_layer == static_cast<const ForeignLayerDisplayItem&>(other).m_layer;
}
#endif // ENABLE(ASSERT)

#ifndef NDEBUG
void ForeignLayerDisplayItem::dumpPropertiesAsDebugString(StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(String::format(", layer: %d", m_layer->id()));
}
#endif // NDEBUG

void recordForeignLayer(GraphicsContext& context,
    const DisplayItemClient& client, DisplayItem::Type type,
    WebLayer* webLayer, const FloatPoint& location, const IntSize& bounds)
{
    PaintController& paintController = context.getPaintController();
    if (paintController.displayItemConstructionIsDisabled())
        return;

    paintController.createAndAppend<ForeignLayerDisplayItem>(client, type, webLayer->ccLayer(), location, bounds);
}

} // namespace blink
