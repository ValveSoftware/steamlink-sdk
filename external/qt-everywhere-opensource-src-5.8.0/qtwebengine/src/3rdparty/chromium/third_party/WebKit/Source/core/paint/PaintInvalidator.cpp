// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintInvalidator.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutObject.h"

namespace blink {

void PaintInvalidator::invalidatePaintIfNeeded(FrameView& frameView, const PaintPropertyTreeBuilderContext& treeContext, Optional<PaintInvalidatorContext>& paintInvalidatorContext)
{
    paintInvalidatorContext.emplace(*frameView.layoutView(), m_pendingDelayedPaintInvalidations);
    frameView.invalidatePaintIfNeeded(*paintInvalidatorContext);
}

void PaintInvalidator::invalidatePaintIfNeeded(const LayoutObject& layoutObject, const PaintPropertyTreeBuilderContext& treeContext, const PaintInvalidatorContext& parentPaintInvalidatorContext, Optional<PaintInvalidatorContext>& paintInvalidatorContext)
{
    if (!layoutObject.shouldCheckForPaintInvalidation(parentPaintInvalidatorContext))
        return;

    paintInvalidatorContext.emplace(parentPaintInvalidatorContext, layoutObject);

    if (layoutObject.mayNeedPaintInvalidationSubtree())
        paintInvalidatorContext->setForceSubtreeInvalidationCheckingWithinContainer();

    PaintInvalidationReason reason = layoutObject.getMutableForPainting().invalidatePaintIfNeeded(*paintInvalidatorContext);
    layoutObject.getMutableForPainting().clearPaintInvalidationFlags(*paintInvalidatorContext);

    paintInvalidatorContext->updateForChildren(reason);
}

void PaintInvalidator::processPendingDelayedPaintInvalidations()
{
    for (auto target : m_pendingDelayedPaintInvalidations)
        target->getMutableForPainting().setShouldDoDelayedFullPaintInvalidation();
}

} // namespace blink
