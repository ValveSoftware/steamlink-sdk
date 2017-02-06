// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPainter_h
#define BoxPainter_h

#include "core/layout/BackgroundBleedAvoidance.h"
#include "core/style/ShadowData.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/graphics/GraphicsTypes.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "wtf/Allocator.h"

namespace blink {

class BackgroundImageGeometry;
class ComputedStyle;
class FillLayer;
class FloatRoundedRect;
class GraphicsContext;
class Image;
class InlineFlowBox;
class LayoutPoint;
class LayoutRect;
class LayoutBoxModelObject;
class NinePieceImage;
struct PaintInfo;
class LayoutBox;
class LayoutObject;

class BoxPainter {
    STACK_ALLOCATED();
public:
    BoxPainter(const LayoutBox& layoutBox) : m_layoutBox(layoutBox) { }
    void paint(const PaintInfo&, const LayoutPoint&);

    void paintBoxDecorationBackground(const PaintInfo&, const LayoutPoint&);
    void paintMask(const PaintInfo&, const LayoutPoint&);
    void paintClippingMask(const PaintInfo&, const LayoutPoint&);

    typedef Vector<const FillLayer*, 8> FillLayerOcclusionOutputList;
    // Returns true if the result fill layers have non-associative blending or compositing mode.
    // (i.e. The rendering will be different without creating isolation group by context.saveLayer().)
    // Note that the output list will be in top-bottom order.
    bool calculateFillLayerOcclusionCulling(FillLayerOcclusionOutputList &reversedPaintList, const FillLayer&);

    // Returns true if the fill layer will certainly occlude anything painted behind it.
    static bool isFillLayerOpaque(const FillLayer&, const LayoutObject&);

    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance = BackgroundBleedNone, SkXfermode::Mode = SkXfermode::kSrcOver_Mode, const LayoutObject* backgroundObject = nullptr);
    void paintMaskImages(const PaintInfo&, const LayoutRect&);
    void paintBoxDecorationBackgroundWithRect(const PaintInfo&, const LayoutPoint&, const LayoutRect&);
    static void paintFillLayer(const LayoutBoxModelObject&, const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, BackgroundBleedAvoidance, const InlineFlowBox* = nullptr, const LayoutSize& = LayoutSize(), SkXfermode::Mode = SkXfermode::kSrcOver_Mode, const LayoutObject* backgroundObject = nullptr);
    static InterpolationQuality chooseInterpolationQuality(const LayoutObject&, Image*, const void*, const LayoutSize&);
    static bool paintNinePieceImage(const LayoutBoxModelObject&, GraphicsContext&, const LayoutRect&, const ComputedStyle&, const NinePieceImage&, SkXfermode::Mode = SkXfermode::kSrcOver_Mode);
    static void paintBorder(const LayoutBoxModelObject&, const PaintInfo&, const LayoutRect&, const ComputedStyle&, BackgroundBleedAvoidance = BackgroundBleedNone, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static void paintBoxShadow(const PaintInfo&, const LayoutRect&, const ComputedStyle&, ShadowStyle, bool includeLogicalLeftEdge = true, bool includeLogicalRightEdge = true);
    static bool shouldForceWhiteBackgroundForPrintEconomy(const ComputedStyle&, const Document&);

    LayoutRect boundsForDrawingRecorder(const LayoutPoint& adjustedPaintOffset);

private:
    void paintBackground(const PaintInfo&, const LayoutRect&, const Color& backgroundColor, BackgroundBleedAvoidance = BackgroundBleedNone);

    const LayoutBox& m_layoutBox;
};

} // namespace blink

#endif
