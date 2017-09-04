// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ScrollbarPainter.h"

#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

void ScrollbarPainter::paintPart(GraphicsContext& graphicsContext,
                                 ScrollbarPart partType,
                                 const IntRect& rect) {
  const LayoutScrollbarPart* partLayoutObject =
      m_layoutScrollbar->getPart(partType);
  if (!partLayoutObject)
    return;
  paintIntoRect(*partLayoutObject, graphicsContext,
                m_layoutScrollbar->location(), LayoutRect(rect));
}

void ScrollbarPainter::paintIntoRect(
    const LayoutScrollbarPart& layoutScrollbarPart,
    GraphicsContext& graphicsContext,
    const LayoutPoint& paintOffset,
    const LayoutRect& rect) {
  // Make sure our dimensions match the rect.
  // FIXME: Setting these is a bad layering violation!
  const_cast<LayoutScrollbarPart&>(layoutScrollbarPart)
      .setLocation(rect.location() - toSize(paintOffset));
  const_cast<LayoutScrollbarPart&>(layoutScrollbarPart).setWidth(rect.width());
  const_cast<LayoutScrollbarPart&>(layoutScrollbarPart)
      .setHeight(rect.height());

  PaintInfo paintInfo(graphicsContext, pixelSnappedIntRect(rect),
                      PaintPhaseForeground, GlobalPaintNormalPhase,
                      PaintLayerNoFlag);
  ObjectPainter(layoutScrollbarPart)
      .paintAllPhasesAtomically(paintInfo, paintOffset);
}

}  // namespace blink
