// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/ClipDisplayItem.h"

#include "platform/geometry/FloatRoundedRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void ClipDisplayItem::replay(GraphicsContext& context) const
{
    context.save();

    // RoundedInnerRectClipper only cares about rounded-rect clips,
    // and passes an "infinite" rect clip; there is no reason to apply this clip.
    // TODO(fmalita): convert RoundedInnerRectClipper to a better suited
    //   DisplayItem so we don't have to special-case its semantics.
    if (m_clipRect != LayoutRect::infiniteIntRect())
        context.clipRect(m_clipRect, AntiAliased);

    for (const FloatRoundedRect& roundedRect : m_roundedRectClips)
        context.clipRoundedRect(roundedRect);
}

void ClipDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    WebVector<SkRRect> webRoundedRects(m_roundedRectClips.size());
    for (size_t i = 0; i < m_roundedRectClips.size(); ++i)
        webRoundedRects[i] = m_roundedRectClips[i];

    list->appendClipItem(visualRect, m_clipRect, webRoundedRects);
}

void EndClipDisplayItem::replay(GraphicsContext& context) const
{
    context.restore();
}

void EndClipDisplayItem::appendToWebDisplayItemList(const IntRect& visualRect, WebDisplayItemList* list) const
{
    list->appendEndClipItem(visualRect);
}

#ifndef NDEBUG
void ClipDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", clipRect: [%d,%d,%d,%d]",
        m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height()));
}
#endif

} // namespace blink
