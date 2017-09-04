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
  PrePaintTreeWalkContext() : paintInvalidatorContext(treeBuilderContext) {}
  PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parentContext)
      : treeBuilderContext(parentContext.treeBuilderContext),
        paintInvalidatorContext(treeBuilderContext,
                                parentContext.paintInvalidatorContext) {}

  bool needToUpdatePaintPropertySubtree = false;
  PaintPropertyTreeBuilderContext treeBuilderContext;
  PaintInvalidatorContext paintInvalidatorContext;
};

void PrePaintTreeWalk::walk(FrameView& rootFrame) {
  DCHECK(rootFrame.frame().document()->lifecycle().state() ==
         DocumentLifecycle::InPrePaint);

  PrePaintTreeWalkContext initialContext;
  initialContext.treeBuilderContext =
      m_propertyTreeBuilder.setupInitialContext();
  walk(rootFrame, initialContext);
  m_paintInvalidator.processPendingDelayedPaintInvalidations();
}

void PrePaintTreeWalk::walk(FrameView& frameView,
                            const PrePaintTreeWalkContext& context) {
  if (frameView.shouldThrottleRendering())
    return;

  PrePaintTreeWalkContext localContext(context);

  // Check whether we need to update the paint property trees.
  if (!localContext.needToUpdatePaintPropertySubtree) {
    if (context.paintInvalidatorContext.forcedSubtreeInvalidationFlags) {
      // forcedSubtreeInvalidationFlags will be true if locations have changed
      // which will affect paint properties (e.g., PaintOffset).
      localContext.needToUpdatePaintPropertySubtree = true;
    } else if (frameView.needsPaintPropertyUpdate()) {
      localContext.needToUpdatePaintPropertySubtree = true;
    }
  }
  // Paint properties can depend on their ancestor properties so ensure the
  // entire subtree is rebuilt on any changes.
  // TODO(pdr): Add additional granularity to the needs update approach such as
  // the ability to do local updates that don't change the subtree.
  if (localContext.needToUpdatePaintPropertySubtree)
    frameView.setNeedsPaintPropertyUpdate();

  m_propertyTreeBuilder.updateProperties(frameView,
                                         localContext.treeBuilderContext);

  m_paintInvalidator.invalidatePaintIfNeeded(
      frameView, localContext.paintInvalidatorContext);

  if (LayoutView* layoutView = frameView.layoutView())
    walk(*layoutView, localContext);

#if DCHECK_IS_ON()
  frameView.layoutView()->assertSubtreeClearedPaintInvalidationFlags();
#endif

  frameView.clearNeedsPaintPropertyUpdate();
}

void PrePaintTreeWalk::walk(const LayoutObject& object,
                            const PrePaintTreeWalkContext& context) {
  PrePaintTreeWalkContext localContext(context);

  // Check whether we need to update the paint property trees.
  if (!localContext.needToUpdatePaintPropertySubtree) {
    if (context.paintInvalidatorContext.forcedSubtreeInvalidationFlags) {
      // forcedSubtreeInvalidationFlags will be true if locations have changed
      // which will affect paint properties (e.g., PaintOffset).
      localContext.needToUpdatePaintPropertySubtree = true;
    } else if (object.needsPaintPropertyUpdate()) {
      localContext.needToUpdatePaintPropertySubtree = true;
    } else if (object.mayNeedPaintInvalidation()) {
      // mayNeedpaintInvalidation will be true when locations change which will
      // affect paint properties (e.g., PaintOffset).
      localContext.needToUpdatePaintPropertySubtree = true;
    } else if (object.shouldDoFullPaintInvalidation()) {
      // shouldDoFullPaintInvalidation will be true when locations or overflow
      // changes which will affect paint properties (e.g., PaintOffset, scroll).
      localContext.needToUpdatePaintPropertySubtree = true;
    }
  }

  // Paint properties can depend on their ancestor properties so ensure the
  // entire subtree is rebuilt on any changes.
  // TODO(pdr): Add additional granularity to the needs update approach such as
  // the ability to do local updates that don't change the subtree.
  if (localContext.needToUpdatePaintPropertySubtree)
    object.getMutableForPainting().setNeedsPaintPropertyUpdate();

  // TODO(pdr): Ensure multi column works with incremental property tree
  // construction.
  if (object.isLayoutMultiColumnSpannerPlaceholder()) {
    // Walk multi-column spanner as if it replaces the placeholder.
    // Set the flag so that the tree builder can specially handle out-of-flow
    // positioned descendants if their containers are between the multi-column
    // container and the spanner. See PaintPropertyTreeBuilder for details.
    localContext.treeBuilderContext.isUnderMultiColumnSpanner = true;
    walk(*toLayoutMultiColumnSpannerPlaceholder(object)
              .layoutObjectInFlowThread(),
         localContext);
    object.getMutableForPainting().clearPaintInvalidationFlags();
    return;
  }

  m_propertyTreeBuilder.updatePropertiesForSelf(
      object, localContext.treeBuilderContext);
  m_paintInvalidator.invalidatePaintIfNeeded(
      object, localContext.paintInvalidatorContext);
  m_propertyTreeBuilder.updatePropertiesForChildren(
      object, localContext.treeBuilderContext);

  for (const LayoutObject* child = object.slowFirstChild(); child;
       child = child->nextSibling()) {
    // Column spanners are walked through their placeholders. See above.
    if (child->isColumnSpanAll())
      continue;
    walk(*child, localContext);
  }

  if (object.isLayoutPart()) {
    const LayoutPart& layoutPart = toLayoutPart(object);
    Widget* widget = layoutPart.widget();
    if (widget && widget->isFrameView()) {
      localContext.treeBuilderContext.current.paintOffset +=
          layoutPart.replacedContentRect().location() -
          widget->frameRect().location();
      localContext.treeBuilderContext.current.paintOffset =
          roundedIntPoint(localContext.treeBuilderContext.current.paintOffset);
      walk(*toFrameView(widget), localContext);
    }
    // TODO(pdr): Investigate RemoteFrameView (crbug.com/579281).
  }

  object.getMutableForPainting().clearPaintInvalidationFlags();
  object.getMutableForPainting().clearNeedsPaintPropertyUpdate();
}

}  // namespace blink
