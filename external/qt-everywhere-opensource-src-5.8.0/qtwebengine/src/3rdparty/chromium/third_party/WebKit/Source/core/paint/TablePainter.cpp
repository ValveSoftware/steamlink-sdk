// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TablePainter.h"

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableSection.h"
#include "core/style/CollapsedBorderValue.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableSectionPainter.h"

namespace blink {

void TablePainter::paintObject(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;

    if (shouldPaintSelfBlockBackground(paintPhase)) {
        paintBoxDecorationBackground(paintInfo, paintOffset);
        if (paintPhase == PaintPhaseSelfBlockBackgroundOnly)
            return;
    }

    if (paintPhase == PaintPhaseMask) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    if (paintPhase != PaintPhaseSelfOutlineOnly) {
        PaintInfo paintInfoForDescendants = paintInfo.forDescendants();

        for (LayoutObject* child = m_layoutTable.firstChild(); child; child = child->nextSibling()) {
            if (child->isBox() && !toLayoutBox(child)->hasSelfPaintingLayer() && (child->isTableSection() || child->isTableCaption())) {
                LayoutPoint childPoint = m_layoutTable.flipForWritingModeForChild(toLayoutBox(child), paintOffset);
                child->paint(paintInfoForDescendants, childPoint);
            }
        }

        if (m_layoutTable.collapseBorders() && shouldPaintDescendantBlockBackgrounds(paintPhase) && m_layoutTable.style()->visibility() == VISIBLE) {
            // Using our cached sorted styles, we then do individual passes,
            // painting each style of border from lowest precedence to highest precedence.
            LayoutTable::CollapsedBorderValues collapsedBorders = m_layoutTable.collapsedBorders();
            size_t count = collapsedBorders.size();
            for (size_t i = 0; i < count; ++i) {
                for (LayoutTableSection* section = m_layoutTable.bottomSection(); section; section = m_layoutTable.sectionAbove(section)) {
                    LayoutPoint childPoint = m_layoutTable.flipForWritingModeForChild(section, paintOffset);
                    TableSectionPainter(*section).paintCollapsedBorders(paintInfoForDescendants, childPoint, collapsedBorders[i]);
                }
            }
        }
    }

    if (shouldPaintSelfOutline(paintPhase))
        ObjectPainter(m_layoutTable).paintOutline(paintInfo, paintOffset);
}

void TablePainter::paintBoxDecorationBackground(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!m_layoutTable.hasBoxDecorationBackground() || m_layoutTable.style()->visibility() != VISIBLE)
        return;

    LayoutRect rect(paintOffset, m_layoutTable.size());
    m_layoutTable.subtractCaptionRect(rect);
    BoxPainter(m_layoutTable).paintBoxDecorationBackgroundWithRect(paintInfo, paintOffset, rect);
}

void TablePainter::paintMask(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (m_layoutTable.style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(paintInfo.context, m_layoutTable, paintInfo.phase))
        return;

    LayoutRect rect(paintOffset, m_layoutTable.size());
    m_layoutTable.subtractCaptionRect(rect);

    LayoutObjectDrawingRecorder recorder(paintInfo.context, m_layoutTable, paintInfo.phase, rect);
    BoxPainter(m_layoutTable).paintMaskImages(paintInfo, rect);
}

} // namespace blink
