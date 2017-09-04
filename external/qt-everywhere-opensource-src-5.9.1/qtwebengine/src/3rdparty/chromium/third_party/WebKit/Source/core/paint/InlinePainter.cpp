// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/InlinePainter.h"

#include "core/layout/LayoutInline.h"
#include "core/paint/LineBoxListPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/geometry/LayoutPoint.h"

namespace blink {

void InlinePainter::paint(const PaintInfo& paintInfo,
                          const LayoutPoint& paintOffset) {
  ObjectPainter(m_layoutInline).checkPaintOffset(paintInfo, paintOffset);
  if (paintInfo.phase == PaintPhaseForeground && paintInfo.isPrinting())
    ObjectPainter(m_layoutInline).addPDFURLRectIfNeeded(paintInfo, paintOffset);

  if (shouldPaintSelfOutline(paintInfo.phase) ||
      shouldPaintDescendantOutlines(paintInfo.phase)) {
    ObjectPainter painter(m_layoutInline);
    if (shouldPaintDescendantOutlines(paintInfo.phase))
      painter.paintInlineChildrenOutlines(paintInfo, paintOffset);
    if (shouldPaintSelfOutline(paintInfo.phase) &&
        !m_layoutInline.isElementContinuation())
      painter.paintOutline(paintInfo, paintOffset);
    return;
  }

  LineBoxListPainter(*m_layoutInline.lineBoxes())
      .paint(m_layoutInline, paintInfo, paintOffset);
}

}  // namespace blink
