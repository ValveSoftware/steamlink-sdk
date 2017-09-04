// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ScrollDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginScrollDisplayItem::replay(GraphicsContext& context) const {
  context.save();
  context.translate(-m_currentOffset.width(), -m_currentOffset.height());
}

void BeginScrollDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  WebDisplayItemList::ScrollContainerId scrollContainerId = &client();
  list->appendScrollItem(m_currentOffset, scrollContainerId);
}

#ifndef NDEBUG
void BeginScrollDisplayItem::dumpPropertiesAsDebugString(
    WTF::StringBuilder& stringBuilder) const {
  PairedBeginDisplayItem::dumpPropertiesAsDebugString(stringBuilder);
  stringBuilder.append(WTF::String::format(", currentOffset: [%d,%d]",
                                           m_currentOffset.width(),
                                           m_currentOffset.height()));
}
#endif

void EndScrollDisplayItem::replay(GraphicsContext& context) const {
  context.restore();
}

void EndScrollDisplayItem::appendToWebDisplayItemList(
    const IntRect& visualRect,
    WebDisplayItemList* list) const {
  list->appendEndScrollItem();
}

}  // namespace blink
