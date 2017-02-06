// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/TransformDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/transforms/AffineTransform.h"
#include "public/platform/WebDisplayItemList.h"

namespace blink {

void BeginTransformDisplayItem::replay(GraphicsContext& context) const
{
    context.save();
    context.concatCTM(m_transform);
}

void BeginTransformDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    list->appendTransformItem(visualRect, affineTransformToSkMatrix(m_transform));
}

#ifndef NDEBUG
void BeginTransformDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    PairedBeginDisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", transform: [%lf,%lf,%lf,%lf,%lf,%lf]",
        m_transform.a(), m_transform.b(), m_transform.c(), m_transform.d(), m_transform.e(), m_transform.f()));
}
#endif

void EndTransformDisplayItem::replay(GraphicsContext& context) const
{
    context.restore();
}

void EndTransformDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    list->appendEndTransformItem(visualRect);
}

} // namespace blink
