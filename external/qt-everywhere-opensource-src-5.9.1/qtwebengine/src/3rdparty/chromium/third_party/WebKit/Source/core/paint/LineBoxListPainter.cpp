// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/LineBoxListPainter.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/line/LineBoxList.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

static void addPDFURLRectsForInlineChildrenRecursively(
    const LayoutObject& layoutObject,
    const PaintInfo& paintInfo,
    const LayoutPoint& paintOffset) {
  for (LayoutObject* child = layoutObject.slowFirstChild(); child;
       child = child->nextSibling()) {
    if (!child->isLayoutInline() ||
        toLayoutBoxModelObject(child)->hasSelfPaintingLayer())
      continue;
    ObjectPainter(*child).addPDFURLRectIfNeeded(paintInfo, paintOffset);
    addPDFURLRectsForInlineChildrenRecursively(*child, paintInfo, paintOffset);
  }
}

void LineBoxListPainter::paint(const LayoutBoxModelObject& layoutObject,
                               const PaintInfo& paintInfo,
                               const LayoutPoint& paintOffset) const {
  DCHECK(!shouldPaintSelfOutline(paintInfo.phase) &&
         !shouldPaintDescendantOutlines(paintInfo.phase));

  // Only paint during the foreground/selection phases.
  if (paintInfo.phase != PaintPhaseForeground &&
      paintInfo.phase != PaintPhaseSelection &&
      paintInfo.phase != PaintPhaseTextClip &&
      paintInfo.phase != PaintPhaseMask)
    return;

  // The only way an inline could paint like this is if it has a layer.
  DCHECK(layoutObject.isLayoutBlock() ||
         (layoutObject.isLayoutInline() && layoutObject.hasLayer()));

  if (paintInfo.phase == PaintPhaseForeground && paintInfo.isPrinting())
    addPDFURLRectsForInlineChildrenRecursively(layoutObject, paintInfo,
                                               paintOffset);

  // If we have no lines then we have no work to do.
  if (!m_lineBoxList.firstLineBox())
    return;

  if (!m_lineBoxList.anyLineIntersectsRect(
          LineLayoutBoxModel(const_cast<LayoutBoxModelObject*>(&layoutObject)),
          paintInfo.cullRect(), paintOffset))
    return;

  PaintInfo info(paintInfo);

  // See if our root lines intersect with the dirty rect. If so, then we paint
  // them. Note that boxes can easily overlap, so we can't make any assumptions
  // based off positions of our first line box or our last line box.
  for (InlineFlowBox* curr = m_lineBoxList.firstLineBox(); curr;
       curr = curr->nextLineBox()) {
    if (m_lineBoxList.lineIntersectsDirtyRect(
            LineLayoutBoxModel(
                const_cast<LayoutBoxModelObject*>(&layoutObject)),
            curr, info.cullRect(), paintOffset)) {
      RootInlineBox& root = curr->root();
      curr->paint(info, paintOffset, root.lineTop(), root.lineBottom());
    }
  }
}

static void invalidateLineBoxPaintOffsetsInternal(
    PaintController& paintController,
    InlineFlowBox* inlineBox) {
#if 0
    // TODO(wangxianzhu): Implement this with PaintInvalidator.
    paintController.invalidatePaintOffset(*inlineBox);
    for (InlineBox* child = inlineBox->firstChild(); child; child = child->nextOnLine()) {
        if (!child->getLineLayoutItem().isText() && child->boxModelObject().hasSelfPaintingLayer())
            continue;
        if (child->isInlineFlowBox())
            invalidateLineBoxPaintOffsetsInternal(paintController, toInlineFlowBox(child));
        else
            paintController.invalidatePaintOffset(*child);
    }
#endif
}

void LineBoxListPainter::invalidateLineBoxPaintOffsets(
    const PaintInfo& paintInfo) const {
  PaintController& paintController = paintInfo.context.getPaintController();
  for (InlineFlowBox* curr = m_lineBoxList.firstLineBox(); curr;
       curr = curr->nextLineBox())
    invalidateLineBoxPaintOffsetsInternal(paintController, curr);
}

}  // namespace blink
