// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/RoundedInnerRectClipper.h"

#include "core/layout/LayoutObject.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

RoundedInnerRectClipper::RoundedInnerRectClipper(const LayoutObject& layoutObject, const PaintInfo& paintInfo, const LayoutRect& rect, const FloatRoundedRect& clipRect, RoundedInnerRectClipperBehavior behavior)
    : m_layoutObject(layoutObject)
    , m_paintInfo(paintInfo)
    , m_usePaintController(behavior == ApplyToDisplayList)
    , m_clipType(m_usePaintController ? m_paintInfo.displayItemTypeForClipping() : DisplayItem::ClipBoxPaintPhaseFirst)
{
    Vector<FloatRoundedRect> roundedRectClips;
    if (clipRect.isRenderable()) {
        roundedRectClips.append(clipRect);
    } else {
        // We create a rounded rect for each of the corners and clip it, while making sure we clip opposing corners together.
        if (!clipRect.getRadii().topLeft().isEmpty() || !clipRect.getRadii().bottomRight().isEmpty()) {
            FloatRect topCorner(clipRect.rect().x(), clipRect.rect().y(), rect.maxX() - clipRect.rect().x(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopLeft(clipRect.getRadii().topLeft());
            roundedRectClips.append(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(rect.x().toFloat(), rect.y().toFloat(), clipRect.rect().maxX() - rect.x().toFloat(), clipRect.rect().maxY() - rect.y().toFloat());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomRight(clipRect.getRadii().bottomRight());
            roundedRectClips.append(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        }

        if (!clipRect.getRadii().topRight().isEmpty() || !clipRect.getRadii().bottomLeft().isEmpty()) {
            FloatRect topCorner(rect.x().toFloat(), clipRect.rect().y(), clipRect.rect().maxX() - rect.x().toFloat(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopRight(clipRect.getRadii().topRight());
            roundedRectClips.append(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(clipRect.rect().x(), rect.y().toFloat(), rect.maxX() - clipRect.rect().x(), clipRect.rect().maxY() - rect.y().toFloat());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomLeft(clipRect.getRadii().bottomLeft());
            roundedRectClips.append(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        }
    }

    if (m_usePaintController) {
        m_paintInfo.context.getPaintController().createAndAppend<ClipDisplayItem>(layoutObject, m_clipType, LayoutRect::infiniteIntRect(), roundedRectClips);
    } else {
        ClipDisplayItem clipDisplayItem(layoutObject, m_clipType, LayoutRect::infiniteIntRect(), roundedRectClips);
        clipDisplayItem.replay(paintInfo.context);
    }
}

RoundedInnerRectClipper::~RoundedInnerRectClipper()
{
    DisplayItem::Type endType = DisplayItem::clipTypeToEndClipType(m_clipType);
    if (m_usePaintController) {
        m_paintInfo.context.getPaintController().endItem<EndClipDisplayItem>(m_layoutObject, endType);
    } else {
        EndClipDisplayItem endClipDisplayItem(m_layoutObject, endType);
        endClipDisplayItem.replay(m_paintInfo.context);
    }
}

} // namespace blink
