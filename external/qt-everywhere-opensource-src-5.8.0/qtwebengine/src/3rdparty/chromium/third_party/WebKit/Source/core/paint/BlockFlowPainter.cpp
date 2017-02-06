// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockFlowPainter.h"

#include "core/layout/FloatingObjects.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/LineBoxListPainter.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"

namespace blink {

void BlockFlowPainter::paintContents(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // Avoid painting descendants of the root element when stylesheets haven't loaded. This eliminates FOUC.
    // It's ok not to draw, because later on, when all the stylesheets do load, styleResolverMayHaveChanged()
    // on Document will trigger a full paint invalidation.
    if (m_layoutBlockFlow.document().didLayoutWithPendingStylesheets() && !m_layoutBlockFlow.isLayoutView())
        return;

    if (!m_layoutBlockFlow.childrenInline()) {
        BlockPainter(m_layoutBlockFlow).paintContents(paintInfo, paintOffset);
        return;
    }
    if (shouldPaintDescendantOutlines(paintInfo.phase))
        ObjectPainter(m_layoutBlockFlow).paintInlineChildrenOutlines(paintInfo, paintOffset);
    else
        LineBoxListPainter(m_layoutBlockFlow.lineBoxes()).paint(m_layoutBlockFlow, paintInfo, paintOffset);
}

void BlockFlowPainter::paintFloats(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_layoutBlockFlow.floatingObjects())
        return;

    ASSERT(paintInfo.phase == PaintPhaseFloat || paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip);
    PaintInfo floatPaintInfo(paintInfo);
    if (paintInfo.phase == PaintPhaseFloat)
        floatPaintInfo.phase = PaintPhaseForeground;

    for (const auto& floatingObject : m_layoutBlockFlow.floatingObjects()->set()) {
        if (!floatingObject->shouldPaint())
            continue;

        const LayoutBox* floatingLayoutObject = floatingObject->layoutObject();
        // FIXME: LayoutPoint version of xPositionForFloatIncludingMargin would make this much cleaner.
        LayoutPoint childPoint = m_layoutBlockFlow.flipFloatForWritingModeForChild(
            *floatingObject, LayoutPoint(paintOffset.x()
            + m_layoutBlockFlow.xPositionForFloatIncludingMargin(*floatingObject) - floatingLayoutObject->location().x(), paintOffset.y()
            + m_layoutBlockFlow.yPositionForFloatIncludingMargin(*floatingObject) - floatingLayoutObject->location().y()));
        ObjectPainter(*floatingLayoutObject).paintAllPhasesAtomically(floatPaintInfo, childPoint);
    }
}

} // namespace blink
