// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGInlineFlowBoxPainter.h"

#include "core/layout/api/LineLayoutAPIShim.h"
#include "core/layout/svg/line/SVGInlineFlowBox.h"
#include "core/layout/svg/line/SVGInlineTextBox.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGInlineTextBoxPainter.h"
#include "core/paint/SVGPaintContext.h"

namespace blink {

void SVGInlineFlowBoxPainter::paintSelectionBackground(
    const PaintInfo& paintInfo) {
  DCHECK(paintInfo.phase == PaintPhaseForeground ||
         paintInfo.phase == PaintPhaseSelection);

  PaintInfo childPaintInfo(paintInfo);
  for (InlineBox* child = m_svgInlineFlowBox.firstChild(); child;
       child = child->nextOnLine()) {
    if (child->isSVGInlineTextBox())
      SVGInlineTextBoxPainter(*toSVGInlineTextBox(child))
          .paintSelectionBackground(childPaintInfo);
    else if (child->isSVGInlineFlowBox())
      SVGInlineFlowBoxPainter(*toSVGInlineFlowBox(child))
          .paintSelectionBackground(childPaintInfo);
  }
}

void SVGInlineFlowBoxPainter::paint(const PaintInfo& paintInfo,
                                    const LayoutPoint& paintOffset) {
  DCHECK(paintInfo.phase == PaintPhaseForeground ||
         paintInfo.phase == PaintPhaseSelection);

  SVGPaintContext paintContext(*LineLayoutAPIShim::constLayoutObjectFrom(
                                   m_svgInlineFlowBox.getLineLayoutItem()),
                               paintInfo);
  if (paintContext.applyClipMaskAndFilterIfNecessary()) {
    for (InlineBox* child = m_svgInlineFlowBox.firstChild(); child;
         child = child->nextOnLine())
      child->paint(paintContext.paintInfo(), paintOffset, LayoutUnit(),
                   LayoutUnit());
  }
}

}  // namespace blink
