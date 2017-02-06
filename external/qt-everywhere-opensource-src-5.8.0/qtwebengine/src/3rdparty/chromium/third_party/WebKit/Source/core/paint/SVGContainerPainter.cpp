// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGContainerPainter.h"

#include "core/layout/svg/LayoutSVGContainer.h"
#include "core/layout/svg/LayoutSVGViewportContainer.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/FloatClipRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/SVGPaintContext.h"
#include "core/svg/SVGSVGElement.h"
#include "wtf/Optional.h"

namespace blink {

void SVGContainerPainter::paint(const PaintInfo& paintInfo)
{
    // Spec: groups w/o children still may render filter content.
    if (!m_layoutSVGContainer.firstChild() && !m_layoutSVGContainer.selfWillPaint())
        return;

    FloatRect boundingBox = m_layoutSVGContainer.paintInvalidationRectInLocalSVGCoordinates();
    // LayoutSVGHiddenContainer's paint invalidation rect is always empty but we need to paint its descendants.
    if (!m_layoutSVGContainer.isSVGHiddenContainer()
        && !paintInfo.cullRect().intersectsCullRect(m_layoutSVGContainer.localToSVGParentTransform(), boundingBox))
        return;

    // Spec: An empty viewBox on the <svg> element disables rendering.
    ASSERT(m_layoutSVGContainer.element());
    if (isSVGSVGElement(*m_layoutSVGContainer.element()) && toSVGSVGElement(*m_layoutSVGContainer.element()).hasEmptyViewBox())
        return;

    PaintInfo paintInfoBeforeFiltering(paintInfo);
    paintInfoBeforeFiltering.updateCullRect(m_layoutSVGContainer.localToSVGParentTransform());
    SVGTransformContext transformContext(paintInfoBeforeFiltering.context, m_layoutSVGContainer, m_layoutSVGContainer.localToSVGParentTransform());
    {
        Optional<FloatClipRecorder> clipRecorder;
        if (m_layoutSVGContainer.isSVGViewportContainer() && SVGLayoutSupport::isOverflowHidden(&m_layoutSVGContainer)) {
            FloatRect viewport = m_layoutSVGContainer.localToSVGParentTransform().inverse().mapRect(toLayoutSVGViewportContainer(m_layoutSVGContainer).viewport());
            clipRecorder.emplace(paintInfoBeforeFiltering.context, m_layoutSVGContainer, paintInfoBeforeFiltering.phase, viewport);
        }

        SVGPaintContext paintContext(m_layoutSVGContainer, paintInfoBeforeFiltering);
        bool continueRendering = true;
        if (paintContext.paintInfo().phase == PaintPhaseForeground)
            continueRendering = paintContext.applyClipMaskAndFilterIfNecessary();

        if (continueRendering) {
            for (LayoutObject* child = m_layoutSVGContainer.firstChild(); child; child = child->nextSibling())
                child->paint(paintContext.paintInfo(), IntPoint());
        }
    }

    if (paintInfoBeforeFiltering.phase != PaintPhaseForeground)
        return;

    if (m_layoutSVGContainer.style()->outlineWidth() && m_layoutSVGContainer.style()->visibility() == VISIBLE) {
        PaintInfo outlinePaintInfo(paintInfoBeforeFiltering);
        outlinePaintInfo.phase = PaintPhaseSelfOutlineOnly;
        ObjectPainter(m_layoutSVGContainer).paintOutline(outlinePaintInfo, LayoutPoint(boundingBox.location()));
    }

    if (paintInfoBeforeFiltering.isPrinting())
        ObjectPainter(m_layoutSVGContainer).addPDFURLRectIfNeeded(paintInfoBeforeFiltering, LayoutPoint());
}

} // namespace blink
