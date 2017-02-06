// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGForeignObjectPainter.h"

#include "core/layout/svg/LayoutSVGForeignObject.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/BlockPainter.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "wtf/Optional.h"

namespace blink {

namespace {

class BlockPainterDelegate : public LayoutBlock {
public:
    BlockPainterDelegate(const LayoutSVGForeignObject& layoutSVGForeignObject)
        : LayoutBlock(nullptr)
        , m_layoutSVGForeignObject(layoutSVGForeignObject)
    { }

private:
    void paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const final
    {
        BlockPainter(m_layoutSVGForeignObject).paint(paintInfo, paintOffset);
    }
    const LayoutSVGForeignObject& m_layoutSVGForeignObject;
};

} // namespace

void SVGForeignObjectPainter::paint(const PaintInfo& paintInfo)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    paintInfoBeforeFiltering.updateCullRect(m_layoutSVGForeignObject.localSVGTransform());
    SVGTransformContext transformContext(paintInfoBeforeFiltering.context, m_layoutSVGForeignObject, m_layoutSVGForeignObject.localSVGTransform());

    Optional<FloatClipRecorder> clipRecorder;
    if (SVGLayoutSupport::isOverflowHidden(&m_layoutSVGForeignObject))
        clipRecorder.emplace(paintInfoBeforeFiltering.context, m_layoutSVGForeignObject, paintInfoBeforeFiltering.phase, m_layoutSVGForeignObject.viewportRect());

    SVGPaintContext paintContext(m_layoutSVGForeignObject, paintInfoBeforeFiltering);
    bool continueRendering = true;
    if (paintContext.paintInfo().phase == PaintPhaseForeground)
        continueRendering = paintContext.applyClipMaskAndFilterIfNecessary();

    if (continueRendering) {
        // Paint all phases of FO elements atomically as though the FO element established its own stacking context.
        // The delegate forwards calls to paint() in LayoutObject::paintAllPhasesAtomically() to
        // BlockPainter::paint(), instead of m_layoutSVGForeignObject.paint() (which would call this method again).
        BlockPainterDelegate delegate(m_layoutSVGForeignObject);
        ObjectPainter(delegate).paintAllPhasesAtomically(paintContext.paintInfo(), LayoutPoint());
    }
}

} // namespace blink
