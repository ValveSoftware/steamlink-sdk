// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/CanvasRenderingContext2DState.h"

#include "core/css/CSSFontSelector.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/paint/FilterEffectBuilder.h"
#include "core/style/ComputedStyle.h"
#include "core/svg/SVGFilterElement.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/canvas2d/CanvasRenderingContext2D.h"
#include "modules/canvas2d/CanvasStyle.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/filters/FilterOperation.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include <memory>

static const char defaultFont[] = "10px sans-serif";
static const char defaultFilter[] = "none";

namespace blink {

CanvasRenderingContext2DState::CanvasRenderingContext2DState()
    : m_unrealizedSaveCount(0)
    , m_strokeStyle(CanvasStyle::createFromRGBA(SK_ColorBLACK))
    , m_fillStyle(CanvasStyle::createFromRGBA(SK_ColorBLACK))
    , m_shadowBlur(0)
    , m_shadowColor(Color::transparent)
    , m_globalAlpha(1)
    , m_lineDashOffset(0)
    , m_unparsedFont(defaultFont)
    , m_unparsedFilter(defaultFilter)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_direction(DirectionInherit)
    , m_realizedFont(false)
    , m_isTransformInvertible(true)
    , m_hasClip(false)
    , m_hasComplexClip(false)
    , m_fillStyleDirty(true)
    , m_strokeStyleDirty(true)
    , m_lineDashDirty(false)
    , m_imageSmoothingQuality(kLow_SkFilterQuality)
{
    m_fillPaint.setStyle(SkPaint::kFill_Style);
    m_fillPaint.setAntiAlias(true);
    m_imagePaint.setStyle(SkPaint::kFill_Style);
    m_imagePaint.setAntiAlias(true);
    m_strokePaint.setStyle(SkPaint::kStroke_Style);
    m_strokePaint.setStrokeWidth(1);
    m_strokePaint.setStrokeCap(SkPaint::kButt_Cap);
    m_strokePaint.setStrokeMiter(10);
    m_strokePaint.setStrokeJoin(SkPaint::kMiter_Join);
    m_strokePaint.setAntiAlias(true);
    setImageSmoothingEnabled(true);
}

CanvasRenderingContext2DState::CanvasRenderingContext2DState(const CanvasRenderingContext2DState& other, ClipListCopyMode mode)
    : CSSFontSelectorClient()
    , m_unrealizedSaveCount(other.m_unrealizedSaveCount)
    , m_unparsedStrokeColor(other.m_unparsedStrokeColor)
    , m_unparsedFillColor(other.m_unparsedFillColor)
    , m_strokeStyle(other.m_strokeStyle)
    , m_fillStyle(other.m_fillStyle)
    , m_strokePaint(other.m_strokePaint)
    , m_fillPaint(other.m_fillPaint)
    , m_imagePaint(other.m_imagePaint)
    , m_shadowOffset(other.m_shadowOffset)
    , m_shadowBlur(other.m_shadowBlur)
    , m_shadowColor(other.m_shadowColor)
    , m_emptyDrawLooper(other.m_emptyDrawLooper)
    , m_shadowOnlyDrawLooper(other.m_shadowOnlyDrawLooper)
    , m_shadowAndForegroundDrawLooper(other.m_shadowAndForegroundDrawLooper)
    , m_shadowOnlyImageFilter(other.m_shadowOnlyImageFilter)
    , m_shadowAndForegroundImageFilter(other.m_shadowAndForegroundImageFilter)
    , m_globalAlpha(other.m_globalAlpha)
    , m_transform(other.m_transform)
    , m_lineDash(other.m_lineDash)
    , m_lineDashOffset(other.m_lineDashOffset)
    , m_unparsedFont(other.m_unparsedFont)
    , m_font(other.m_font)
    , m_fontForFilter(other.m_fontForFilter)
    , m_unparsedFilter(other.m_unparsedFilter)
    , m_filterValue(other.m_filterValue)
    , m_resolvedFilter(other.m_resolvedFilter)
    , m_textAlign(other.m_textAlign)
    , m_textBaseline(other.m_textBaseline)
    , m_direction(other.m_direction)
    , m_realizedFont(other.m_realizedFont)
    , m_isTransformInvertible(other.m_isTransformInvertible)
    , m_hasClip(other.m_hasClip)
    , m_hasComplexClip(other.m_hasComplexClip)
    , m_fillStyleDirty(other.m_fillStyleDirty)
    , m_strokeStyleDirty(other.m_strokeStyleDirty)
    , m_lineDashDirty(other.m_lineDashDirty)
    , m_imageSmoothingEnabled(other.m_imageSmoothingEnabled)
    , m_imageSmoothingQuality(other.m_imageSmoothingQuality)
{
    if (mode == CopyClipList) {
        m_clipList = other.m_clipList;
    }
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.getFontSelector())->registerForInvalidationCallbacks(this);
}

CanvasRenderingContext2DState::~CanvasRenderingContext2DState()
{
}

void CanvasRenderingContext2DState::fontsNeedUpdate(CSSFontSelector* fontSelector)
{
    DCHECK_EQ(fontSelector, m_font.getFontSelector());
    ASSERT(m_realizedFont);

    m_font.update(fontSelector);
    // FIXME: We only really need to invalidate the resolved filter if the font
    // update above changed anything and the filter uses font-dependent units.
    m_resolvedFilter.reset();
}

DEFINE_TRACE(CanvasRenderingContext2DState)
{
    visitor->trace(m_strokeStyle);
    visitor->trace(m_fillStyle);
    visitor->trace(m_filterValue);
    CSSFontSelectorClient::trace(visitor);
}

void CanvasRenderingContext2DState::setLineDashOffset(double offset)
{
    m_lineDashOffset = offset;
    m_lineDashDirty = true;
}

void CanvasRenderingContext2DState::setLineDash(const Vector<double>& dash)
{
    m_lineDash = dash;
    // Spec requires the concatenation of two copies the dash list when the
    // number of elements is odd
    if (dash.size() % 2)
        m_lineDash.appendVector(dash);

    m_lineDashDirty = true;
}

static bool hasANonZeroElement(const Vector<double>& lineDash)
{
    for (size_t i = 0; i < lineDash.size(); i++) {
        if (lineDash[i] != 0.0)
            return true;
    }
    return false;
}

void CanvasRenderingContext2DState::updateLineDash() const
{
    if (!m_lineDashDirty)
        return;

    if (!hasANonZeroElement(m_lineDash)) {
        m_strokePaint.setPathEffect(0);
    } else {
        Vector<float> lineDash(m_lineDash.size());
        std::copy(m_lineDash.begin(), m_lineDash.end(), lineDash.begin());
        m_strokePaint.setPathEffect(SkDashPathEffect::Make(lineDash.data(), lineDash.size(), m_lineDashOffset));
    }

    m_lineDashDirty = false;
}

void CanvasRenderingContext2DState::setStrokeStyle(CanvasStyle* style)
{
    m_strokeStyle = style;
    m_strokeStyleDirty = true;
}

void CanvasRenderingContext2DState::setFillStyle(CanvasStyle* style)
{
    m_fillStyle = style;
    m_fillStyleDirty = true;
}

void CanvasRenderingContext2DState::updateStrokeStyle() const
{
    if (!m_strokeStyleDirty)
        return;

    int clampedAlpha = clampedAlphaForBlending(m_globalAlpha);
    ASSERT(m_strokeStyle);
    m_strokeStyle->applyToPaint(m_strokePaint);
    m_strokePaint.setColor(scaleAlpha(m_strokeStyle->paintColor(), clampedAlpha));
    m_strokeStyleDirty = false;
}

void CanvasRenderingContext2DState::updateFillStyle() const
{
    if (!m_fillStyleDirty)
        return;

    int clampedAlpha = clampedAlphaForBlending(m_globalAlpha);
    ASSERT(m_fillStyle);
    m_fillStyle->applyToPaint(m_fillPaint);
    m_fillPaint.setColor(scaleAlpha(m_fillStyle->paintColor(), clampedAlpha));
    m_fillStyleDirty = false;
}

CanvasStyle* CanvasRenderingContext2DState::style(PaintType paintType) const
{
    switch (paintType) {
    case FillPaintType:
        return fillStyle();
    case StrokePaintType:
        return strokeStyle();
    case ImagePaintType:
        return nullptr;
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void CanvasRenderingContext2DState::setShouldAntialias(bool shouldAntialias)
{
    m_fillPaint.setAntiAlias(shouldAntialias);
    m_strokePaint.setAntiAlias(shouldAntialias);
    m_imagePaint.setAntiAlias(shouldAntialias);
}

bool CanvasRenderingContext2DState::shouldAntialias() const
{
    ASSERT(m_fillPaint.isAntiAlias() == m_strokePaint.isAntiAlias() && m_fillPaint.isAntiAlias() == m_imagePaint.isAntiAlias());
    return m_fillPaint.isAntiAlias();
}

void CanvasRenderingContext2DState::setGlobalAlpha(double alpha)
{
    m_globalAlpha = alpha;
    m_strokeStyleDirty = true;
    m_fillStyleDirty = true;
    int imageAlpha = clampedAlphaForBlending(alpha);
    m_imagePaint.setAlpha(imageAlpha > 255 ? 255 : imageAlpha);
}

void CanvasRenderingContext2DState::clipPath(const SkPath& path, AntiAliasingMode antiAliasingMode)
{
    m_clipList.clipPath(path, antiAliasingMode, affineTransformToSkMatrix(m_transform));
    m_hasClip = true;
    if (!path.isRect(0))
        m_hasComplexClip = true;
}

void CanvasRenderingContext2DState::setFont(const Font& font, CSSFontSelector* selector)
{
    m_font = font;
    m_font.update(selector);
    m_realizedFont = true;
    selector->registerForInvalidationCallbacks(this);
}

const Font& CanvasRenderingContext2DState::font() const
{
    ASSERT(m_realizedFont);
    return m_font;
}

void CanvasRenderingContext2DState::setTransform(const AffineTransform& transform)
{
    m_isTransformInvertible = transform.isInvertible();
    m_transform = transform;
}

void CanvasRenderingContext2DState::resetTransform()
{
    m_transform.makeIdentity();
    m_isTransformInvertible = true;
}

static void updateFilterReferences(HTMLCanvasElement* canvasElement, CanvasRenderingContext2D* context, const FilterOperations& filters)
{
    context->clearFilterReferences();
    context->addFilterReferences(filters, canvasElement->document());
}

SkImageFilter* CanvasRenderingContext2DState::getFilter(Element* styleResolutionHost, IntSize canvasSize, CanvasRenderingContext2D* context) const
{
    if (!m_filterValue)
        return nullptr;

    // StyleResolverState cannot be used in frame-less documents.
    if (!styleResolutionHost->document().frame())
        return nullptr;

    if (!m_resolvedFilter) {
        RefPtr<ComputedStyle> filterStyle = ComputedStyle::create();
        // Must set font in case the filter uses any font-relative units (em, ex)
        filterStyle->setFont(m_fontForFilter);

        StyleResolverState resolverState(styleResolutionHost->document(), styleResolutionHost, filterStyle.get());
        resolverState.setStyle(filterStyle);

        StyleBuilder::applyProperty(CSSPropertyFilter, resolverState, *m_filterValue);
        resolverState.loadPendingResources();
        FilterEffectBuilder* filterEffectBuilder = FilterEffectBuilder::create();

        // We can't reuse m_fillPaint and m_strokePaint for the filter, since these incorporate
        // the global alpha, which isn't applicable here.
        SkPaint fillPaintForFilter;
        SkPaint strokePaintForFilter;
        m_fillStyle->applyToPaint(fillPaintForFilter);
        m_strokeStyle->applyToPaint(strokePaintForFilter);
        fillPaintForFilter.setColor(m_fillStyle->paintColor());
        strokePaintForFilter.setColor(m_strokeStyle->paintColor());
        FloatSize floatCanvasSize(canvasSize);
        const double effectiveZoom = 1.0; // Deliberately ignore zoom on the canvas element
        filterEffectBuilder->build(styleResolutionHost, filterStyle->filter(), effectiveZoom, &floatCanvasSize, &fillPaintForFilter, &strokePaintForFilter);

        FilterEffect* lastEffect = filterEffectBuilder->lastEffect();
        if (lastEffect) {
            lastEffect->determineFilterPrimitiveSubregion();
        }
        m_resolvedFilter = SkiaImageFilterBuilder::build(lastEffect, ColorSpaceDeviceRGB);
        if (m_resolvedFilter) {
            updateFilterReferences(toHTMLCanvasElement(styleResolutionHost), context, filterStyle->filter());
            if (lastEffect->originTainted())
                context->setOriginTainted();
        }
    }

    return m_resolvedFilter.get();
}

bool CanvasRenderingContext2DState::hasFilter(Element* styleResolutionHost, IntSize canvasSize, CanvasRenderingContext2D* context) const
{
    // Checking for a non-null m_filterValue isn't sufficient, since this value
    // might refer to a non-existent filter.
    return !!getFilter(styleResolutionHost, canvasSize, context);
}

void CanvasRenderingContext2DState::clearResolvedFilter() const
{
    m_resolvedFilter.reset();
}

SkDrawLooper* CanvasRenderingContext2DState::emptyDrawLooper() const
{
    if (!m_emptyDrawLooper) {
        std::unique_ptr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
        m_emptyDrawLooper = drawLooperBuilder->detachDrawLooper();
    }
    return m_emptyDrawLooper.get();
}

SkDrawLooper* CanvasRenderingContext2DState::shadowOnlyDrawLooper() const
{
    if (!m_shadowOnlyDrawLooper) {
        std::unique_ptr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
        drawLooperBuilder->addShadow(m_shadowOffset, m_shadowBlur, m_shadowColor, DrawLooperBuilder::ShadowIgnoresTransforms, DrawLooperBuilder::ShadowRespectsAlpha);
        m_shadowOnlyDrawLooper = drawLooperBuilder->detachDrawLooper();
    }
    return m_shadowOnlyDrawLooper.get();
}

SkDrawLooper* CanvasRenderingContext2DState::shadowAndForegroundDrawLooper() const
{
    if (!m_shadowAndForegroundDrawLooper) {
        std::unique_ptr<DrawLooperBuilder> drawLooperBuilder = DrawLooperBuilder::create();
        drawLooperBuilder->addShadow(m_shadowOffset, m_shadowBlur, m_shadowColor, DrawLooperBuilder::ShadowIgnoresTransforms, DrawLooperBuilder::ShadowRespectsAlpha);
        drawLooperBuilder->addUnmodifiedContent();
        m_shadowAndForegroundDrawLooper = drawLooperBuilder->detachDrawLooper();
    }
    return m_shadowAndForegroundDrawLooper.get();
}

SkImageFilter* CanvasRenderingContext2DState::shadowOnlyImageFilter() const
{
    if (!m_shadowOnlyImageFilter) {
        double sigma = skBlurRadiusToSigma(m_shadowBlur);
        m_shadowOnlyImageFilter = SkDropShadowImageFilter::Make(m_shadowOffset.width(), m_shadowOffset.height(), sigma, sigma, m_shadowColor, SkDropShadowImageFilter::kDrawShadowOnly_ShadowMode, nullptr);
    }
    return m_shadowOnlyImageFilter.get();
}

SkImageFilter* CanvasRenderingContext2DState::shadowAndForegroundImageFilter() const
{
    if (!m_shadowAndForegroundImageFilter) {
        double sigma = skBlurRadiusToSigma(m_shadowBlur);
        m_shadowAndForegroundImageFilter = SkDropShadowImageFilter::Make(m_shadowOffset.width(), m_shadowOffset.height(), sigma, sigma, m_shadowColor, SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode, nullptr);
    }
    return m_shadowAndForegroundImageFilter.get();
}

void CanvasRenderingContext2DState::shadowParameterChanged()
{
    m_shadowOnlyDrawLooper.clear();
    m_shadowAndForegroundDrawLooper.clear();
    m_shadowOnlyImageFilter.reset();
    m_shadowAndForegroundImageFilter.reset();
}

void CanvasRenderingContext2DState::setShadowOffsetX(double x)
{
    m_shadowOffset.setWidth(x);
    shadowParameterChanged();
}

void CanvasRenderingContext2DState::setShadowOffsetY(double y)
{
    m_shadowOffset.setHeight(y);
    shadowParameterChanged();
}

void CanvasRenderingContext2DState::setShadowBlur(double shadowBlur)
{
    m_shadowBlur = shadowBlur;
    shadowParameterChanged();
}

void CanvasRenderingContext2DState::setShadowColor(SkColor shadowColor)
{
    m_shadowColor = shadowColor;
    shadowParameterChanged();
}

void CanvasRenderingContext2DState::setFilter(const CSSValue* filterValue)
{
    m_filterValue = filterValue;
    m_resolvedFilter.reset();
}

void CanvasRenderingContext2DState::setGlobalComposite(SkXfermode::Mode mode)
{
    m_strokePaint.setXfermodeMode(mode);
    m_fillPaint.setXfermodeMode(mode);
    m_imagePaint.setXfermodeMode(mode);
}

SkXfermode::Mode CanvasRenderingContext2DState::globalComposite() const
{
    SkXfermode* xferMode = m_strokePaint.getXfermode();
    SkXfermode::Mode mode;
    if (!xferMode || !xferMode->asMode(&mode))
        return SkXfermode::kSrcOver_Mode;
    return mode;
}

void CanvasRenderingContext2DState::setImageSmoothingEnabled(bool enabled)
{
    m_imageSmoothingEnabled = enabled;
    updateFilterQuality();
}

bool CanvasRenderingContext2DState::imageSmoothingEnabled() const
{
    return m_imageSmoothingEnabled;
}

void CanvasRenderingContext2DState::setImageSmoothingQuality(const String& qualityString)
{
    if (qualityString == "low") {
        m_imageSmoothingQuality = kLow_SkFilterQuality;
    } else if (qualityString == "medium") {
        m_imageSmoothingQuality = kMedium_SkFilterQuality;
    } else if (qualityString == "high") {
        m_imageSmoothingQuality = kHigh_SkFilterQuality;
    } else {
        return;
    }
    updateFilterQuality();
}

String CanvasRenderingContext2DState::imageSmoothingQuality() const
{
    switch (m_imageSmoothingQuality) {
    case kLow_SkFilterQuality:
        return "low";
    case kMedium_SkFilterQuality:
        return "medium";
    case kHigh_SkFilterQuality:
        return "high";
    default:
        ASSERT_NOT_REACHED();
        return "low";
    }
}

void CanvasRenderingContext2DState::updateFilterQuality() const
{
    if (!m_imageSmoothingEnabled) {
        updateFilterQualityWithSkFilterQuality(kNone_SkFilterQuality);
    } else {
        updateFilterQualityWithSkFilterQuality(m_imageSmoothingQuality);
    }
}

void CanvasRenderingContext2DState::updateFilterQualityWithSkFilterQuality(const SkFilterQuality& filterQuality) const
{
    m_strokePaint.setFilterQuality(filterQuality);
    m_fillPaint.setFilterQuality(filterQuality);
    m_imagePaint.setFilterQuality(filterQuality);
}


bool CanvasRenderingContext2DState::shouldDrawShadows() const
{
    return alphaChannel(m_shadowColor) && (m_shadowBlur || !m_shadowOffset.isZero());
}

const SkPaint* CanvasRenderingContext2DState::getPaint(PaintType paintType, ShadowMode shadowMode, ImageType imageType) const
{
    SkPaint* paint;
    switch (paintType) {
    case StrokePaintType:
        updateLineDash();
        updateStrokeStyle();
        paint = &m_strokePaint;
        break;
    default:
        ASSERT_NOT_REACHED();
        // no break on purpose: paint needs to be assigned to avoid compiler warning about uninitialized variable
    case FillPaintType:
        updateFillStyle();
        paint = &m_fillPaint;
        break;
    case ImagePaintType:
        paint = &m_imagePaint;
        break;
    }

    if ((!shouldDrawShadows() && shadowMode == DrawShadowAndForeground) || shadowMode == DrawForegroundOnly) {
        paint->setLooper(0);
        paint->setImageFilter(0);
        return paint;
    }

    if (!shouldDrawShadows() && shadowMode == DrawShadowOnly) {
        paint->setLooper(sk_ref_sp(emptyDrawLooper())); // draw nothing
        paint->setImageFilter(0);
        return paint;
    }

    if (shadowMode == DrawShadowOnly) {
        if (imageType == NonOpaqueImage || m_filterValue) {
            paint->setLooper(0);
            paint->setImageFilter(shadowOnlyImageFilter());
            return paint;
        }
        paint->setLooper(sk_ref_sp(shadowOnlyDrawLooper()));
        paint->setImageFilter(0);
        return paint;
    }

    ASSERT(shadowMode == DrawShadowAndForeground);
    if (imageType == NonOpaqueImage) {
        paint->setLooper(0);
        paint->setImageFilter(shadowAndForegroundImageFilter());
        return paint;
    }
    paint->setLooper(sk_ref_sp(shadowAndForegroundDrawLooper()));
    paint->setImageFilter(0);
    return paint;
}

} // namespace blink
