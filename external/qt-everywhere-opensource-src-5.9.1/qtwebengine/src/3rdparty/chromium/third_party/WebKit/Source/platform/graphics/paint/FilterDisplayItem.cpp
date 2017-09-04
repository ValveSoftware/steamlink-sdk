// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/FilterDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginFilterDisplayItem::replay(GraphicsContext& context) const {
  FloatRect imageFilterBounds(m_bounds);
  imageFilterBounds.move(-m_origin.x(), -m_origin.y());
  context.save();
  context.translate(m_origin.x(), m_origin.y());
  context.beginLayer(1, SkBlendMode::kSrcOver, &imageFilterBounds,
                     ColorFilterNone, m_imageFilter);
  context.translate(-m_origin.x(), -m_origin.y());
}

void BeginFilterDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  list->appendFilterItem(m_compositorFilterOperations.asCcFilterOperations(),
                         m_bounds, m_origin);
}

bool BeginFilterDisplayItem::drawsContent() const {
  // Skia cannot currently tell us if a filter will draw content,
  // even when no input primitives are drawn.
  return true;
}

#ifndef NDEBUG
void BeginFilterDisplayItem::dumpPropertiesAsDebugString(
    WTF::StringBuilder& stringBuilder) const {
  DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
  stringBuilder.append(
      WTF::String::format(", filter bounds: [%f,%f,%f,%f]", m_bounds.x(),
                          m_bounds.y(), m_bounds.width(), m_bounds.height()));
}
#endif

void EndFilterDisplayItem::replay(GraphicsContext& context) const {
  context.endLayer();
  context.restore();
}

void EndFilterDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  list->appendEndFilterItem();
}

}  // namespace blink
