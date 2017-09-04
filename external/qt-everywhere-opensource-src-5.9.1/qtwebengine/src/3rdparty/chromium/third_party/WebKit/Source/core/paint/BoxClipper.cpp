// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxClipper.h"

#include "core/layout/LayoutBox.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

static bool boxNeedsClip(const LayoutBox& box) {
  if (box.hasControlClip())
    return true;
  if (box.isSVGRoot() && toLayoutSVGRoot(box).shouldApplyViewportClip())
    return true;
  if (box.hasLayer() && box.layer()->isSelfPaintingLayer())
    return false;
  return box.hasOverflowClip() || box.styleRef().containsPaint();
}

BoxClipper::BoxClipper(const LayoutBox& box,
                       const PaintInfo& paintInfo,
                       const LayoutPoint& accumulatedOffset,
                       ContentsClipBehavior contentsClipBehavior)
    : m_box(box),
      m_paintInfo(paintInfo),
      m_clipType(DisplayItem::kUninitializedType) {
  DCHECK(m_paintInfo.phase != PaintPhaseSelfBlockBackgroundOnly &&
         m_paintInfo.phase != PaintPhaseSelfOutlineOnly);

  if (m_paintInfo.phase == PaintPhaseMask)
    return;

  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    const auto* objectProperties = m_box.paintProperties();
    if (objectProperties && objectProperties->overflowClip()) {
      PaintChunkProperties properties(
          paintInfo.context.getPaintController().currentPaintChunkProperties());
      properties.clip = objectProperties->overflowClip();
      m_scopedClipProperty.emplace(paintInfo.context.getPaintController(), box,
                                   paintInfo.displayItemTypeForClipping(),
                                   properties);
    }
    return;
  }

  if (!boxNeedsClip(m_box))
    return;

  LayoutRect clipRect = m_box.hasControlClip()
                            ? m_box.controlClipRect(accumulatedOffset)
                            : m_box.overflowClipRect(accumulatedOffset);
  FloatRoundedRect clipRoundedRect(0, 0, 0, 0);
  bool hasBorderRadius = m_box.style()->hasBorderRadius();
  if (hasBorderRadius)
    clipRoundedRect = m_box.style()->getRoundedInnerBorderFor(
        LayoutRect(accumulatedOffset, m_box.size()));

  // Selection may extend beyond visual overflow, so this optimization is
  // invalid if selection is present.
  if (contentsClipBehavior == SkipContentsClipIfPossible &&
      box.getSelectionState() == SelectionNone) {
    LayoutRect contentsVisualOverflow = m_box.contentsVisualOverflowRect();
    if (contentsVisualOverflow.isEmpty())
      return;

    LayoutRect conservativeClipRect = clipRect;
    if (hasBorderRadius)
      conservativeClipRect.intersect(
          LayoutRect(clipRoundedRect.radiusCenterRect()));
    conservativeClipRect.moveBy(-accumulatedOffset);
    if (m_box.hasLayer())
      conservativeClipRect.move(m_box.scrolledContentOffset());
    if (conservativeClipRect.contains(contentsVisualOverflow))
      return;
  }

  if (!m_paintInfo.context.getPaintController()
           .displayItemConstructionIsDisabled()) {
    m_clipType = m_paintInfo.displayItemTypeForClipping();
    Vector<FloatRoundedRect> roundedRects;
    if (hasBorderRadius)
      roundedRects.append(clipRoundedRect);
    m_paintInfo.context.getPaintController().createAndAppend<ClipDisplayItem>(
        m_box, m_clipType, pixelSnappedIntRect(clipRect), roundedRects);
  }
}

BoxClipper::~BoxClipper() {
  if (m_clipType == DisplayItem::kUninitializedType)
    return;

  DCHECK(boxNeedsClip(m_box));
  m_paintInfo.context.getPaintController().endItem<EndClipDisplayItem>(
      m_box, DisplayItem::clipTypeToEndClipType(m_clipType));
}

}  // namespace blink
