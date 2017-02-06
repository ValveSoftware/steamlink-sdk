// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxDecorationData.h"

#include "core/layout/LayoutBox.h"
#include "core/paint/BoxPainter.h"
#include "core/style/BorderEdge.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

BoxDecorationData::BoxDecorationData(const LayoutBox& layoutBox)
{
    backgroundColor = layoutBox.style()->visitedDependentColor(CSSPropertyBackgroundColor);
    hasBackground = backgroundColor.alpha() || layoutBox.style()->hasBackgroundImage();
    ASSERT(hasBackground == layoutBox.style()->hasBackground());
    hasBorderDecoration = layoutBox.style()->hasBorderDecoration();
    hasAppearance = layoutBox.style()->hasAppearance();
    bleedAvoidance = determineBackgroundBleedAvoidance(layoutBox);
}

namespace {

bool borderObscuresBackgroundEdge(const ComputedStyle& style)
{
    BorderEdge edges[4];
    style.getBorderEdgeInfo(edges);

    for (auto& edge : edges) {
        if (!edge.obscuresBackgroundEdge())
            return false;
    }

    return true;
}

} // anonymous namespace

BackgroundBleedAvoidance BoxDecorationData::determineBackgroundBleedAvoidance(const LayoutBox& layoutBox)
{
    if (layoutBox.isDocumentElement())
        return BackgroundBleedNone;

    if (!hasBackground)
        return BackgroundBleedNone;

    const ComputedStyle& boxStyle = layoutBox.styleRef();
    const bool hasBorderRadius = boxStyle.hasBorderRadius();
    if (!hasBorderDecoration || !hasBorderRadius || layoutBox.canRenderBorderImage()) {
        if (layoutBox.backgroundShouldAlwaysBeClipped())
            return BackgroundBleedClipOnly;
        // Border radius clipping may require layer bleed avoidance if we are going to draw
        // an image over something else, because we do not want the antialiasing to lead to bleeding
        if (boxStyle.hasBackgroundImage() && hasBorderRadius) {
            // But if the top layer is opaque for the purposes of background painting, we do not
            // need the bleed avoidance because we will not paint anything behind the top layer.
            // But only if we need to draw something underneath.
            const FillLayer& fillLayer = layoutBox.style()->backgroundLayers();
            if ((backgroundColor.alpha() || fillLayer.next()) && !fillLayer.imageOccludesNextLayers(layoutBox))
                return BackgroundBleedClipLayer;
        }
        return BackgroundBleedNone;
    }

    if (borderObscuresBackgroundEdge(boxStyle))
        return BackgroundBleedShrinkBackground;

    return BackgroundBleedClipLayer;
}

} // namespace blink
