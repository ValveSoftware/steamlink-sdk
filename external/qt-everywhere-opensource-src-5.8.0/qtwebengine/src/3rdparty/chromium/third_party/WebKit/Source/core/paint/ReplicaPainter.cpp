// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ReplicaPainter.h"

#include "core/layout/LayoutReplica.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintLayerPainter.h"

namespace blink {

void ReplicaPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // PaintLayerReflectionInfo should have set a transform on the LayoutReplica.
    // Without it, we won't paint the reflection correctly.
    ASSERT(m_layoutReplica.layer()->transform());

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseMask)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + m_layoutReplica.location();

    if (paintInfo.phase == PaintPhaseForeground) {
        // Turn around and paint the parent layer. Use temporary clipRects, so that the layer doesn't end up caching clip rects
        // computing using the wrong rootLayer
        PaintLayer* rootPaintingLayer = m_layoutReplica.layer()->parent();
        PaintLayerPaintingInfo paintingInfo(rootPaintingLayer, LayoutRect(paintInfo.cullRect().m_rect), GlobalPaintNormalPhase, LayoutSize());
        PaintLayerFlags flags = PaintLayerHaveTransparency | PaintLayerAppliedTransform | PaintLayerUncachedClipRects | PaintLayerPaintingReflection;
        PaintLayerPainter(*m_layoutReplica.layer()->parent()).paintLayer(paintInfo.context, paintingInfo, flags);
    } else if (paintInfo.phase == PaintPhaseMask) {
        m_layoutReplica.paintMask(paintInfo, adjustedPaintOffset);
    }
}

} // namespace blink
