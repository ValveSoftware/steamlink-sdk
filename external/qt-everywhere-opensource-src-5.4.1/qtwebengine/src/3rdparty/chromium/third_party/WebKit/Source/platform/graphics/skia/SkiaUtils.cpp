/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "platform/graphics/skia/SkiaUtils.h"

#include "SkColorPriv.h"
#include "SkRegion.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBuffer.h"

namespace WebCore {

static const struct CompositOpToXfermodeMode {
    uint8_t mCompositOp;
    uint8_t m_xfermodeMode;
} gMapCompositOpsToXfermodeModes[] = {
    { CompositeClear,           SkXfermode::kClear_Mode },
    { CompositeCopy,            SkXfermode::kSrc_Mode },
    { CompositeSourceOver,      SkXfermode::kSrcOver_Mode },
    { CompositeSourceIn,        SkXfermode::kSrcIn_Mode },
    { CompositeSourceOut,       SkXfermode::kSrcOut_Mode },
    { CompositeSourceAtop,      SkXfermode::kSrcATop_Mode },
    { CompositeDestinationOver, SkXfermode::kDstOver_Mode },
    { CompositeDestinationIn,   SkXfermode::kDstIn_Mode },
    { CompositeDestinationOut,  SkXfermode::kDstOut_Mode },
    { CompositeDestinationAtop, SkXfermode::kDstATop_Mode },
    { CompositeXOR,             SkXfermode::kXor_Mode },
    { CompositePlusDarker,      SkXfermode::kDarken_Mode },
    { CompositePlusLighter,     SkXfermode::kPlus_Mode }
};

// keep this array in sync with blink::WebBlendMode enum in public/platform/WebBlendMode.h
static const uint8_t gMapBlendOpsToXfermodeModes[] = {
    SkXfermode::kClear_Mode, // blink::WebBlendModeNormal
    SkXfermode::kMultiply_Mode, // blink::WebBlendModeMultiply
    SkXfermode::kScreen_Mode, // blink::WebBlendModeScreen
    SkXfermode::kOverlay_Mode, // blink::WebBlendModeOverlay
    SkXfermode::kDarken_Mode, // blink::WebBlendModeDarken
    SkXfermode::kLighten_Mode, // blink::WebBlendModeLighten
    SkXfermode::kColorDodge_Mode, // blink::WebBlendModeColorDodge
    SkXfermode::kColorBurn_Mode, // blink::WebBlendModeColorBurn
    SkXfermode::kHardLight_Mode, // blink::WebBlendModeHardLight
    SkXfermode::kSoftLight_Mode, // blink::WebBlendModeSoftLight
    SkXfermode::kDifference_Mode, // blink::WebBlendModeDifference
    SkXfermode::kExclusion_Mode, // blink::WebBlendModeExclusion
    SkXfermode::kHue_Mode, // blink::WebBlendModeHue
    SkXfermode::kSaturation_Mode, // blink::WebBlendModeSaturation
    SkXfermode::kColor_Mode, // blink::WebBlendModeColor
    SkXfermode::kLuminosity_Mode // blink::WebBlendModeLuminosity
};

PassRefPtr<SkXfermode> WebCoreCompositeToSkiaComposite(CompositeOperator op, blink::WebBlendMode blendMode)
{
    if (blendMode != blink::WebBlendModeNormal) {
        if ((uint8_t)blendMode >= SK_ARRAY_COUNT(gMapBlendOpsToXfermodeModes)) {
            SkDEBUGF(("GraphicsContext::setPlatformCompositeOperation unknown blink::WebBlendMode %d\n", blendMode));
            return adoptRef(SkXfermode::Create(SkXfermode::kSrcOver_Mode));
        }
        SkXfermode::Mode mode = (SkXfermode::Mode)gMapBlendOpsToXfermodeModes[(uint8_t)blendMode];
        return adoptRef(SkXfermode::Create(mode));
    }

    const CompositOpToXfermodeMode* table = gMapCompositOpsToXfermodeModes;

    for (unsigned i = 0; i < SK_ARRAY_COUNT(gMapCompositOpsToXfermodeModes); i++) {
        if (table[i].mCompositOp == op)
            return adoptRef(SkXfermode::Create((SkXfermode::Mode)table[i].m_xfermodeMode));
    }

    SkDEBUGF(("GraphicsContext::setPlatformCompositeOperation unknown CompositeOperator %d\n", op));
    return adoptRef(SkXfermode::Create(SkXfermode::kSrcOver_Mode)); // fall-back
}

static U8CPU InvScaleByte(U8CPU component, uint32_t scale)
{
    SkASSERT(component == (uint8_t)component);
    return (component * scale + 0x8000) >> 16;
}

SkColor SkPMColorToColor(SkPMColor pm)
{
    if (!pm)
        return 0;
    unsigned a = SkGetPackedA32(pm);
    if (!a) {
        // A zero alpha value when there are non-zero R, G, or B channels is an
        // invalid premultiplied color (since all channels should have been
        // multiplied by 0 if a=0).
        SkASSERT(false);
        // In production, return 0 to protect against division by zero.
        return 0;
    }

    uint32_t scale = (255 << 16) / a;

    return SkColorSetARGB(a,
                          InvScaleByte(SkGetPackedR32(pm), scale),
                          InvScaleByte(SkGetPackedG32(pm), scale),
                          InvScaleByte(SkGetPackedB32(pm), scale));
}

bool SkPathContainsPoint(const SkPath& originalPath, const FloatPoint& point, SkPath::FillType ft)
{
    SkRect bounds = originalPath.getBounds();

    // We can immediately return false if the point is outside the bounding
    // rect.  We don't use bounds.contains() here, since it would exclude
    // points on the right and bottom edges of the bounding rect, and we want
    // to include them.
    SkScalar fX = SkFloatToScalar(point.x());
    SkScalar fY = SkFloatToScalar(point.y());
    if (fX < bounds.fLeft || fX > bounds.fRight || fY < bounds.fTop || fY > bounds.fBottom)
        return false;

    // Scale the path to a large size before hit testing for two reasons:
    // 1) Skia has trouble with coordinates close to the max signed 16-bit values, so we scale larger paths down.
    //    TODO: when Skia is patched to work properly with large values, this will not be necessary.
    // 2) Skia does not support analytic hit testing, so we scale paths up to do raster hit testing with subpixel accuracy.
    SkScalar biggestCoord = std::max(std::max(std::max(bounds.fRight, bounds.fBottom), -bounds.fLeft), -bounds.fTop);
    if (SkScalarNearlyZero(biggestCoord))
        return false;
    biggestCoord = std::max(std::max(biggestCoord, fX + 1), fY + 1);

    const SkScalar kMaxCoordinate = SkIntToScalar(1 << 15);
    SkScalar scale = SkScalarDiv(kMaxCoordinate, biggestCoord);

    SkRegion rgn;
    SkRegion clip;
    SkMatrix m;
    SkPath scaledPath(originalPath);

    scaledPath.setFillType(ft);
    m.setScale(scale, scale);
    scaledPath.transform(m, 0);

    int x = static_cast<int>(floorf(0.5f + point.x() * scale));
    int y = static_cast<int>(floorf(0.5f + point.y() * scale));
    clip.setRect(x - 1, y - 1, x + 1, y + 1);

    return rgn.setPath(scaledPath, clip);
}

SkMatrix affineTransformToSkMatrix(const AffineTransform& source)
{
    SkMatrix result;

    result.setScaleX(WebCoreDoubleToSkScalar(source.a()));
    result.setSkewX(WebCoreDoubleToSkScalar(source.c()));
    result.setTranslateX(WebCoreDoubleToSkScalar(source.e()));

    result.setScaleY(WebCoreDoubleToSkScalar(source.d()));
    result.setSkewY(WebCoreDoubleToSkScalar(source.b()));
    result.setTranslateY(WebCoreDoubleToSkScalar(source.f()));

    // FIXME: Set perspective properly.
    result.setPerspX(0);
    result.setPerspY(0);
    result.set(SkMatrix::kMPersp2, SK_Scalar1);

    return result;
}

}  // namespace WebCore
