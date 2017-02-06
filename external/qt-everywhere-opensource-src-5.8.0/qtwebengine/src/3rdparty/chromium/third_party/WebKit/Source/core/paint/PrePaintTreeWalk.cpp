// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PrePaintTreeWalk.h"

#include "core/dom/DocumentLifecycle.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutMultiColumnSpannerPlaceholder.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutView.h"

namespace blink {

struct PrePaintTreeWalkContext {
    PrePaintTreeWalkContext() { }
    PrePaintTreeWalkContext(const PaintPropertyTreeBuilderContext& parentTreeBuilderContext)
        : treeBuilderContext(parentTreeBuilderContext) { }

    PaintPropertyTreeBuilderContext treeBuilderContext;
    // This will be initialized by PaintInvalidator::invalidatePaintIfNeeded().
    // TODO(wangxianzhu): Change to copy-and-update pattern like PaintPropertyTreeBuilderContext.
    Optional<PaintInvalidatorContext> paintInvalidatorContext;
};

void PrePaintTreeWalk::walk(FrameView& rootFrame)
{
    DCHECK(rootFrame.frame().document()->lifecycle().state() == DocumentLifecycle::InPrePaint);

    PrePaintTreeWalkContext rootContext;
    m_propertyTreeBuilder.buildTreeRootNodes(rootFrame, rootContext.treeBuilderContext);
    walk(rootFrame, rootContext);
    m_paintInvalidator.processPendingDelayedPaintInvalidations();
}

void PrePaintTreeWalk::walk(FrameView& frameView, const PrePaintTreeWalkContext& context)
{
    PrePaintTreeWalkContext localContext(context.treeBuilderContext);
    m_propertyTreeBuilder.buildTreeNodes(frameView, localContext.treeBuilderContext);

    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
        m_paintInvalidator.invalidatePaintIfNeeded(frameView, localContext.treeBuilderContext, localContext.paintInvalidatorContext);

    if (LayoutView* layoutView = frameView.layoutView())
        walk(*layoutView, localContext);
}

void PrePaintTreeWalk::walk(const LayoutObject& object, const PrePaintTreeWalkContext& context)
{
    PrePaintTreeWalkContext localContext(context.treeBuilderContext);
    m_propertyTreeBuilder.buildTreeNodes(object, localContext.treeBuilderContext);

    if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() && context.paintInvalidatorContext)
        m_paintInvalidator.invalidatePaintIfNeeded(object, localContext.treeBuilderContext, *context.paintInvalidatorContext, localContext.paintInvalidatorContext);

    for (const LayoutObject* child = object.slowFirstChild(); child; child = child->nextSibling()) {
        // Column spanners are walked through their placeholders. See below.
        if (child->isColumnSpanAll())
            continue;
        walk(*child, localContext);
    }

    if (object.isLayoutMultiColumnSpannerPlaceholder())
        walk(*toLayoutMultiColumnSpannerPlaceholder(object).layoutObjectInFlowThread(), localContext);

    if (object.isLayoutPart()) {
        Widget* widget = toLayoutPart(object).widget();
        if (widget && widget->isFrameView())
            walk(*toFrameView(widget), localContext);
        // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
    }
}

} // namespace blink
