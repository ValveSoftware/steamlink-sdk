// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/canvas2d/BaseRenderingContext2D.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/css/parser/CSSParser.h"
#include "core/frame/ImageBitmap.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "modules/canvas2d/CanvasGradient.h"
#include "modules/canvas2d/CanvasPattern.h"
#include "modules/canvas2d/CanvasStyle.h"
#include "modules/canvas2d/Path2D.h"
#include "platform/Histogram.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/Color.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StrokeData.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkImageFilter.h"

namespace blink {

BaseRenderingContext2D::BaseRenderingContext2D()
    : m_clipAntialiasing(NotAntiAliased)
{
    m_stateStack.append(CanvasRenderingContext2DState::create());
}

BaseRenderingContext2D::~BaseRenderingContext2D()
{
}

CanvasRenderingContext2DState& BaseRenderingContext2D::modifiableState()
{
    realizeSaves();
    return *m_stateStack.last();
}

void BaseRenderingContext2D::realizeSaves()
{
    validateStateStack();
    if (state().hasUnrealizedSaves()) {
        ASSERT(m_stateStack.size() >= 1);
        // Reduce the current state's unrealized count by one now,
        // to reflect the fact we are saving one state.
        m_stateStack.last()->restore();
        m_stateStack.append(CanvasRenderingContext2DState::create(state(), CanvasRenderingContext2DState::DontCopyClipList));
        // Set the new state's unrealized count to 0, because it has no outstanding saves.
        // We need to do this explicitly because the copy constructor and operator= used
        // by the Vector operations copy the unrealized count from the previous state (in
        // turn necessary to support correct resizing and unwinding of the stack).
        m_stateStack.last()->resetUnrealizedSaveCount();
        SkCanvas* canvas = drawingCanvas();
        if (canvas)
            canvas->save();
        validateStateStack();
    }
}

void BaseRenderingContext2D::save()
{
    m_stateStack.last()->save();
}

void BaseRenderingContext2D::restore()
{
    validateStateStack();
    if (state().hasUnrealizedSaves()) {
        // We never realized the save, so just record that it was unnecessary.
        m_stateStack.last()->restore();
        return;
    }
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().transform());
    m_stateStack.removeLast();
    m_stateStack.last()->clearResolvedFilter();
    m_path.transform(state().transform().inverse());
    SkCanvas* c = drawingCanvas();
    if (c)
        c->restore();

    validateStateStack();
}

void BaseRenderingContext2D::restoreMatrixClipStack(SkCanvas* c) const
{
    if (!c)
        return;
    HeapVector<Member<CanvasRenderingContext2DState>>::const_iterator currState;
    DCHECK(m_stateStack.begin() < m_stateStack.end());
    for (currState = m_stateStack.begin(); currState < m_stateStack.end(); currState++) {
        c->setMatrix(SkMatrix::I());
        currState->get()->playbackClips(c);
        c->setMatrix(affineTransformToSkMatrix(currState->get()->transform()));
        c->save();
    }
    c->restore();
}

static inline void convertCanvasStyleToUnionType(CanvasStyle* style, StringOrCanvasGradientOrCanvasPattern& returnValue)
{
    if (CanvasGradient* gradient = style->getCanvasGradient()) {
        returnValue.setCanvasGradient(gradient);
        return;
    }
    if (CanvasPattern* pattern = style->getCanvasPattern()) {
        returnValue.setCanvasPattern(pattern);
        return;
    }
    returnValue.setString(style->color());
}

void BaseRenderingContext2D::strokeStyle(StringOrCanvasGradientOrCanvasPattern& returnValue) const
{
    convertCanvasStyleToUnionType(state().strokeStyle(), returnValue);
}

void BaseRenderingContext2D::setStrokeStyle(const StringOrCanvasGradientOrCanvasPattern& style)
{
    ASSERT(!style.isNull());

    String colorString;
    CanvasStyle* canvasStyle = nullptr;
    if (style.isString()) {
        colorString = style.getAsString();
        if (colorString == state().unparsedStrokeColor())
            return;
        Color parsedColor = 0;
        if (!parseColorOrCurrentColor(parsedColor, colorString))
            return;
        if (state().strokeStyle()->isEquivalentRGBA(parsedColor.rgb())) {
            modifiableState().setUnparsedStrokeColor(colorString);
            return;
        }
        canvasStyle = CanvasStyle::createFromRGBA(parsedColor.rgb());
    } else if (style.isCanvasGradient()) {
        canvasStyle = CanvasStyle::createFromGradient(style.getAsCanvasGradient());
    } else if (style.isCanvasPattern()) {
        CanvasPattern* canvasPattern = style.getAsCanvasPattern();

        if (originClean() && !canvasPattern->originClean())
            setOriginTainted();

        canvasStyle = CanvasStyle::createFromPattern(canvasPattern);
    }

    ASSERT(canvasStyle);

    modifiableState().setStrokeStyle(canvasStyle);
    modifiableState().setUnparsedStrokeColor(colorString);
    modifiableState().clearResolvedFilter();
}

void BaseRenderingContext2D::fillStyle(StringOrCanvasGradientOrCanvasPattern& returnValue) const
{
    convertCanvasStyleToUnionType(state().fillStyle(), returnValue);
}

void BaseRenderingContext2D::setFillStyle(const StringOrCanvasGradientOrCanvasPattern& style)
{
    ASSERT(!style.isNull());
    validateStateStack();
    String colorString;
    CanvasStyle* canvasStyle = nullptr;
    if (style.isString()) {
        colorString = style.getAsString();
        if (colorString == state().unparsedFillColor())
            return;
        Color parsedColor = 0;
        if (!parseColorOrCurrentColor(parsedColor, colorString))
            return;
        if (state().fillStyle()->isEquivalentRGBA(parsedColor.rgb())) {
            modifiableState().setUnparsedFillColor(colorString);
            return;
        }
        canvasStyle = CanvasStyle::createFromRGBA(parsedColor.rgb());
    } else if (style.isCanvasGradient()) {
        canvasStyle = CanvasStyle::createFromGradient(style.getAsCanvasGradient());
    } else if (style.isCanvasPattern()) {
        CanvasPattern* canvasPattern = style.getAsCanvasPattern();

        if (originClean() && !canvasPattern->originClean())
            setOriginTainted();
        if (canvasPattern->getPattern()->isTextureBacked())
            disableDeferral(DisableDeferralReasonUsingTextureBackedPattern);
        canvasStyle = CanvasStyle::createFromPattern(canvasPattern);
    }

    ASSERT(canvasStyle);
    modifiableState().setFillStyle(canvasStyle);
    modifiableState().setUnparsedFillColor(colorString);
    modifiableState().clearResolvedFilter();
}

double BaseRenderingContext2D::lineWidth() const
{
    return state().lineWidth();
}

void BaseRenderingContext2D::setLineWidth(double width)
{
    if (!std::isfinite(width) || width <= 0)
        return;
    if (state().lineWidth() == width)
        return;
    modifiableState().setLineWidth(width);
}

String BaseRenderingContext2D::lineCap() const
{
    return lineCapName(state().getLineCap());
}

void BaseRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    if (state().getLineCap() == cap)
        return;
    modifiableState().setLineCap(cap);
}

String BaseRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().getLineJoin());
}

void BaseRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    if (state().getLineJoin() == join)
        return;
    modifiableState().setLineJoin(join);
}

double BaseRenderingContext2D::miterLimit() const
{
    return state().miterLimit();
}

void BaseRenderingContext2D::setMiterLimit(double limit)
{
    if (!std::isfinite(limit) || limit <= 0)
        return;
    if (state().miterLimit() == limit)
        return;
    modifiableState().setMiterLimit(limit);
}

double BaseRenderingContext2D::shadowOffsetX() const
{
    return state().shadowOffset().width();
}

void BaseRenderingContext2D::setShadowOffsetX(double x)
{
    if (!std::isfinite(x))
        return;
    if (state().shadowOffset().width() == x)
        return;
    modifiableState().setShadowOffsetX(x);
}

double BaseRenderingContext2D::shadowOffsetY() const
{
    return state().shadowOffset().height();
}

void BaseRenderingContext2D::setShadowOffsetY(double y)
{
    if (!std::isfinite(y))
        return;
    if (state().shadowOffset().height() == y)
        return;
    modifiableState().setShadowOffsetY(y);
}

double BaseRenderingContext2D::shadowBlur() const
{
    return state().shadowBlur();
}

void BaseRenderingContext2D::setShadowBlur(double blur)
{
    if (!std::isfinite(blur) || blur < 0)
        return;
    if (state().shadowBlur() == blur)
        return;
    modifiableState().setShadowBlur(blur);
}

String BaseRenderingContext2D::shadowColor() const
{
    return Color(state().shadowColor()).serialized();
}

void BaseRenderingContext2D::setShadowColor(const String& colorString)
{
    Color color;
    if (!parseColorOrCurrentColor(color, colorString))
        return;
    if (state().shadowColor() == color)
        return;
    modifiableState().setShadowColor(color.rgb());
}

const Vector<double>& BaseRenderingContext2D::getLineDash() const
{
    return state().lineDash();
}

static bool lineDashSequenceIsValid(const Vector<double>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void BaseRenderingContext2D::setLineDash(const Vector<double>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;
    modifiableState().setLineDash(dash);
}

double BaseRenderingContext2D::lineDashOffset() const
{
    return state().lineDashOffset();
}

void BaseRenderingContext2D::setLineDashOffset(double offset)
{
    if (!std::isfinite(offset) || state().lineDashOffset() == offset)
        return;
    modifiableState().setLineDashOffset(offset);
}

double BaseRenderingContext2D::globalAlpha() const
{
    return state().globalAlpha();
}

void BaseRenderingContext2D::setGlobalAlpha(double alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    if (state().globalAlpha() == alpha)
        return;
    modifiableState().setGlobalAlpha(alpha);
}

String BaseRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(compositeOperatorFromSkia(state().globalComposite()), blendModeFromSkia(state().globalComposite()));
}

void BaseRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeSourceOver;
    WebBlendMode blendMode = WebBlendModeNormal;
    if (!parseCompositeAndBlendOperator(operation, op, blendMode))
        return;
    SkXfermode::Mode xfermode = WebCoreCompositeToSkiaComposite(op, blendMode);
    if (state().globalComposite() == xfermode)
        return;
    modifiableState().setGlobalComposite(xfermode);
}

String BaseRenderingContext2D::filter() const
{
    return state().unparsedFilter();
}

void BaseRenderingContext2D::setFilter(const String& filterString)
{
    if (filterString == state().unparsedFilter())
        return;

    CSSValue* filterValue = CSSParser::parseSingleValue(CSSPropertyFilter, filterString, CSSParserContext(HTMLStandardMode, nullptr));

    if (!filterValue || filterValue->isInitialValue() || filterValue->isInheritedValue())
        return;

    modifiableState().setUnparsedFilter(filterString);
    modifiableState().setFilter(filterValue);
    snapshotStateForFilter();
}

SVGMatrixTearOff* BaseRenderingContext2D::currentTransform() const
{
    return SVGMatrixTearOff::create(state().transform());
}

void BaseRenderingContext2D::setCurrentTransform(SVGMatrixTearOff* matrixTearOff)
{
    const AffineTransform& transform = matrixTearOff->value();
    setTransform(transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f());
}

void BaseRenderingContext2D::scale(double sx, double sy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(sx) || !std::isfinite(sy))
        return;

    AffineTransform newTransform = state().transform();
    newTransform.scaleNonUniform(sx, sy);
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;

    c->scale(sx, sy);
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void BaseRenderingContext2D::rotate(double angleInRadians)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().transform();
    newTransform.rotateRadians(angleInRadians);
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;
    c->rotate(angleInRadians * (180.0 / piFloat));
    m_path.transform(AffineTransform().rotateRadians(-angleInRadians));
}

void BaseRenderingContext2D::translate(double tx, double ty)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().isTransformInvertible())
        return;

    if (!std::isfinite(tx) || !std::isfinite(ty))
        return;

    AffineTransform newTransform = state().transform();
    newTransform.translate(tx, ty);
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void BaseRenderingContext2D::transform(double m11, double m12, double m21, double m22, double dx, double dy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = state().transform() * transform;
    if (state().transform() == newTransform)
        return;

    modifiableState().setTransform(newTransform);
    if (!state().isTransformInvertible())
        return;

    c->concat(affineTransformToSkMatrix(transform));
    m_path.transform(transform.inverse());
}

void BaseRenderingContext2D::resetTransform()
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    AffineTransform ctm = state().transform();
    bool invertibleCTM = state().isTransformInvertible();
    // It is possible that CTM is identity while CTM is not invertible.
    // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
    if (ctm.isIdentity() && invertibleCTM)
        return;

    // resetTransform() resolves the non-invertible CTM state.
    modifiableState().resetTransform();
    c->setMatrix(affineTransformToSkMatrix(baseTransform()));

    if (invertibleCTM)
        m_path.transform(ctm);
    // When else, do nothing because all transform methods didn't update m_path when CTM became non-invertible.
    // It means that resetTransform() restores m_path just before CTM became non-invertible.
}

void BaseRenderingContext2D::setTransform(double m11, double m12, double m21, double m22, double dx, double dy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    resetTransform();
    transform(m11, m12, m21, m22, dx, dy);
}

void BaseRenderingContext2D::beginPath()
{
    m_path.clear();
}

static bool validateRectForCanvas(double& x, double& y, double& width, double& height)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

    return true;
}

bool BaseRenderingContext2D::isFullCanvasCompositeMode(SkXfermode::Mode op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeSourceAtop and CompositeDestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == SkXfermode::kSrcIn_Mode || op == SkXfermode::kSrcOut_Mode || op == SkXfermode::kDstIn_Mode || op == SkXfermode::kDstATop_Mode;
}

static bool isPathExpensive(const Path& path)
{
    const SkPath& skPath = path.getSkPath();
    if (ExpensiveCanvasHeuristicParameters::ConcavePathsAreExpensive && !skPath.isConvex())
        return true;

    if (skPath.countPoints() > ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount)
        return true;

    return false;
}

void BaseRenderingContext2D::drawPathInternal(const Path& path, CanvasRenderingContext2DState::PaintType paintType, SkPath::FillType fillType)
{
    if (path.isEmpty())
        return;

    SkPath skPath = path.getSkPath();
    FloatRect bounds = path.boundingRect();
    skPath.setFillType(fillType);

    if (paintType == CanvasRenderingContext2DState::StrokePaintType)
        inflateStrokeRect(bounds);

    if (!drawingCanvas())
        return;

    if (draw(
        [&skPath, this](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            c->drawPath(skPath, *paint);
        },
        [](const SkIRect& rect) // overdraw test lambda
        {
            return false;
        }, bounds, paintType)) {
        if (isPathExpensive(path)) {
            ImageBuffer* buffer = imageBuffer();
            if (buffer)
                buffer->setHasExpensiveOp();
        }
    }
}

static SkPath::FillType parseWinding(const String& windingRuleString)
{
    if (windingRuleString == "nonzero")
        return SkPath::kWinding_FillType;
    if (windingRuleString == "evenodd")
        return SkPath::kEvenOdd_FillType;

    ASSERT_NOT_REACHED();
    return SkPath::kEvenOdd_FillType;
}

void BaseRenderingContext2D::fill(const String& windingRuleString)
{
    trackDrawCall(FillPath);
    drawPathInternal(m_path, CanvasRenderingContext2DState::FillPaintType, parseWinding(windingRuleString));
}

void BaseRenderingContext2D::fill(Path2D* domPath, const String& windingRuleString)
{
    trackDrawCall(FillPath, domPath);
    drawPathInternal(domPath->path(), CanvasRenderingContext2DState::FillPaintType, parseWinding(windingRuleString));
}

void BaseRenderingContext2D::stroke()
{
    trackDrawCall(StrokePath);
    drawPathInternal(m_path, CanvasRenderingContext2DState::StrokePaintType);
}

void BaseRenderingContext2D::stroke(Path2D* domPath)
{
    trackDrawCall(StrokePath, domPath);
    drawPathInternal(domPath->path(), CanvasRenderingContext2DState::StrokePaintType);
}

void BaseRenderingContext2D::fillRect(double x, double y, double width, double height)
{
    trackDrawCall(FillRect);
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!drawingCanvas())
        return;

    SkRect rect = SkRect::MakeXYWH(x, y, width, height);
    draw(
        [&rect, this](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            c->drawRect(rect, *paint);
        },
        [&rect, this](const SkIRect& clipBounds) // overdraw test lambda
        {
            return rectContainsTransformedRect(rect, clipBounds);
        }, rect, CanvasRenderingContext2DState::FillPaintType);
}

static void strokeRectOnCanvas(const FloatRect& rect, SkCanvas* canvas, const SkPaint* paint)
{
    ASSERT(paint->getStyle() == SkPaint::kStroke_Style);
    if ((rect.width() > 0) != (rect.height() > 0)) {
        // When stroking, we must skip the zero-dimension segments
        SkPath path;
        path.moveTo(rect.x(), rect.y());
        path.lineTo(rect.maxX(), rect.maxY());
        path.close();
        canvas->drawPath(path, *paint);
        return;
    }
    canvas->drawRect(rect, *paint);
}

void BaseRenderingContext2D::strokeRect(double x, double y, double width, double height)
{
    trackDrawCall(StrokeRect);
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!drawingCanvas())
        return;

    SkRect rect = SkRect::MakeXYWH(x, y, width, height);
    FloatRect bounds = rect;
    inflateStrokeRect(bounds);
    draw(
        [&rect, this](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            strokeRectOnCanvas(rect, c, paint);
        },
        [](const SkIRect& clipBounds) // overdraw test lambda
        {
            return false;
        }, bounds, CanvasRenderingContext2DState::StrokePaintType);
}

void BaseRenderingContext2D::clipInternal(const Path& path, const String& windingRuleString)
{
    SkCanvas* c = drawingCanvas();
    if (!c) {
        return;
    }
    if (!state().isTransformInvertible()) {
        return;
    }

    SkPath skPath = path.getSkPath();
    skPath.setFillType(parseWinding(windingRuleString));
    modifiableState().clipPath(skPath, m_clipAntialiasing);
    c->clipPath(skPath, SkRegion::kIntersect_Op, m_clipAntialiasing == AntiAliased);
    if (ExpensiveCanvasHeuristicParameters::ComplexClipsAreExpensive && !skPath.isRect(0) && hasImageBuffer()) {
        imageBuffer()->setHasExpensiveOp();
    }
}

void BaseRenderingContext2D::clip(const String& windingRuleString)
{
    clipInternal(m_path, windingRuleString);
}

void BaseRenderingContext2D::clip(Path2D* domPath, const String& windingRuleString)
{
    clipInternal(domPath->path(), windingRuleString);
}

bool BaseRenderingContext2D::isPointInPath(const double x, const double y, const String& windingRuleString)
{
    return isPointInPathInternal(m_path, x, y, windingRuleString);
}

bool BaseRenderingContext2D::isPointInPath(Path2D* domPath, const double x, const double y, const String& windingRuleString)
{
    return isPointInPathInternal(domPath->path(), x, y, windingRuleString);
}

bool BaseRenderingContext2D::isPointInPathInternal(const Path& path, const double x, const double y, const String& windingRuleString)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return false;
    if (!state().isTransformInvertible())
        return false;

    FloatPoint point(x, y);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    AffineTransform ctm = state().transform();
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);

    return path.contains(transformedPoint, SkFillTypeToWindRule(parseWinding(windingRuleString)));
}

bool BaseRenderingContext2D::isPointInStroke(const double x, const double y)
{
    return isPointInStrokeInternal(m_path, x, y);
}

bool BaseRenderingContext2D::isPointInStroke(Path2D* domPath, const double x, const double y)
{
    return isPointInStrokeInternal(domPath->path(), x, y);
}

bool BaseRenderingContext2D::isPointInStrokeInternal(const Path& path, const double x, const double y)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return false;
    if (!state().isTransformInvertible())
        return false;

    FloatPoint point(x, y);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    AffineTransform ctm = state().transform();
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);

    StrokeData strokeData;
    strokeData.setThickness(state().lineWidth());
    strokeData.setLineCap(state().getLineCap());
    strokeData.setLineJoin(state().getLineJoin());
    strokeData.setMiterLimit(state().miterLimit());
    Vector<float> lineDash(state().lineDash().size());
    std::copy(state().lineDash().begin(), state().lineDash().end(), lineDash.begin());
    strokeData.setLineDash(lineDash, state().lineDashOffset());
    return path.strokeContains(transformedPoint, strokeData);
}

void BaseRenderingContext2D::clearRect(double x, double y, double width, double height)
{
    m_usageCounters.numClearRectCalls++;

    if (!validateRectForCanvas(x, y, width, height))
        return;

    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().isTransformInvertible())
        return;

    SkIRect clipBounds;
    if (!c->getClipDeviceBounds(&clipBounds))
        return;

    SkPaint clearPaint;
    clearPaint.setXfermodeMode(SkXfermode::kClear_Mode);
    clearPaint.setStyle(SkPaint::kFill_Style);
    FloatRect rect(x, y, width, height);

    if (rectContainsTransformedRect(rect, clipBounds)) {
        checkOverdraw(rect, &clearPaint, CanvasRenderingContext2DState::NoImage, ClipFill);
        if (drawingCanvas())
            drawingCanvas()->drawRect(rect, clearPaint);
        didDraw(clipBounds);
    } else {
        SkIRect dirtyRect;
        if (computeDirtyRect(rect, clipBounds, &dirtyRect)) {
            c->drawRect(rect, clearPaint);
            didDraw(dirtyRect);
        }
    }
}

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(std::min(rect.x(), rect.maxX()),
        std::min(rect.y(), rect.maxY()),
        std::max(rect.width(), -rect.width()),
        std::max(rect.height(), -rect.height()));
}

static inline void clipRectsToImageRect(const FloatRect& imageRect, FloatRect* srcRect, FloatRect* dstRect)
{
    if (imageRect.contains(*srcRect))
        return;

    // Compute the src to dst transform
    FloatSize scale(dstRect->size().width() / srcRect->size().width(), dstRect->size().height() / srcRect->size().height());
    FloatPoint scaledSrcLocation = srcRect->location();
    scaledSrcLocation.scale(scale.width(), scale.height());
    FloatSize offset = dstRect->location() - scaledSrcLocation;

    srcRect->intersect(imageRect);

    // To clip the destination rectangle in the same proportion, transform the clipped src rect
    *dstRect = *srcRect;
    dstRect->scale(scale.width(), scale.height());
    dstRect->move(offset);
}

static inline CanvasImageSource* toImageSourceInternal(const CanvasImageSourceUnion& value, ExceptionState& exceptionState)
{
    if (value.isHTMLImageElement())
        return value.getAsHTMLImageElement();
    if (value.isHTMLVideoElement())
        return value.getAsHTMLVideoElement();
    if (value.isHTMLCanvasElement())
        return value.getAsHTMLCanvasElement();
    if (value.isImageBitmap()) {
        if (static_cast<ImageBitmap*>(value.getAsImageBitmap())->isNeutered()) {
            exceptionState.throwDOMException(InvalidStateError, String::format("The image source is detached"));
            return nullptr;
        }
        return value.getAsImageBitmap();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void BaseRenderingContext2D::drawImage(ExecutionContext* executionContext, const CanvasImageSourceUnion& imageSource, double x, double y, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource, exceptionState);
    if (!imageSourceInternal)
        return;
    FloatSize defaultObjectSize(width(), height());
    FloatSize sourceRectSize = imageSourceInternal->elementSize(defaultObjectSize);
    FloatSize destRectSize = imageSourceInternal->defaultDestinationSize(defaultObjectSize);
    drawImage(executionContext, imageSourceInternal, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, destRectSize.width(), destRectSize.height(), exceptionState);
}

void BaseRenderingContext2D::drawImage(ExecutionContext* executionContext, const CanvasImageSourceUnion& imageSource,
    double x, double y, double width, double height, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource, exceptionState);
    if (!imageSourceInternal)
        return;
    FloatSize defaultObjectSize(this->width(), this->height());
    FloatSize sourceRectSize = imageSourceInternal->elementSize(defaultObjectSize);
    drawImage(executionContext, imageSourceInternal, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, width, height, exceptionState);
}

void BaseRenderingContext2D::drawImage(ExecutionContext* executionContext, const CanvasImageSourceUnion& imageSource,
    double sx, double sy, double sw, double sh,
    double dx, double dy, double dw, double dh, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource, exceptionState);
    if (!imageSourceInternal)
        return;
    drawImage(executionContext, imageSourceInternal, sx, sy, sw, sh, dx, dy, dw, dh, exceptionState);
}

bool BaseRenderingContext2D::shouldDrawImageAntialiased(const FloatRect& destRect) const
{
    if (!state().shouldAntialias())
        return false;
    SkCanvas* c = drawingCanvas();
    ASSERT(c);

    const SkMatrix &ctm = c->getTotalMatrix();
    // Don't disable anti-aliasing if we're rotated or skewed.
    if (!ctm.rectStaysRect())
        return true;
    // Check if the dimensions of the destination are "small" (less than one
    // device pixel). To prevent sudden drop-outs. Since we know that
    // kRectStaysRect_Mask is set, the matrix either has scale and no skew or
    // vice versa. We can query the kAffine_Mask flag to determine which case
    // it is.
    // FIXME: This queries the CTM while drawing, which is generally
    // discouraged. Always drawing with AA can negatively impact performance
    // though - that's why it's not always on.
    SkScalar widthExpansion, heightExpansion;
    if (ctm.getType() & SkMatrix::kAffine_Mask)
        widthExpansion = ctm[SkMatrix::kMSkewY], heightExpansion = ctm[SkMatrix::kMSkewX];
    else
        widthExpansion = ctm[SkMatrix::kMScaleX], heightExpansion = ctm[SkMatrix::kMScaleY];
    return destRect.width() * fabs(widthExpansion) < 1 || destRect.height() * fabs(heightExpansion) < 1;
}

void BaseRenderingContext2D::drawImageInternal(SkCanvas* c, CanvasImageSource* imageSource, Image* image, const FloatRect& srcRect, const FloatRect& dstRect, const SkPaint* paint)
{
    trackDrawCall(DrawImage);

    int initialSaveCount = c->getSaveCount();
    SkPaint imagePaint = *paint;

    if (paint->getImageFilter()) {
        SkMatrix ctm = c->getTotalMatrix();
        SkMatrix invCtm;
        if (!ctm.invert(&invCtm)) {
            // There is an earlier check for invertibility, but the arithmetic
            // in AffineTransform is not exactly identical, so it is possible
            // for SkMatrix to find the transform to be non-invertible at this stage.
            // crbug.com/504687
            return;
        }
        c->save();
        c->concat(invCtm);
        SkRect bounds = dstRect;
        ctm.mapRect(&bounds);
        SkPaint layerPaint;
        layerPaint.setXfermode(sk_ref_sp(paint->getXfermode()));
        layerPaint.setImageFilter(paint->getImageFilter());

        c->saveLayer(&bounds, &layerPaint);
        c->concat(ctm);
        imagePaint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
        imagePaint.setImageFilter(nullptr);
    }

    if (!imageSource->isVideoElement()) {
        imagePaint.setAntiAlias(shouldDrawImageAntialiased(dstRect));
        image->draw(c, imagePaint, dstRect, srcRect, DoNotRespectImageOrientation, Image::DoNotClampImageToSourceRect);
    } else {
        c->save();
        c->clipRect(dstRect);
        c->translate(dstRect.x(), dstRect.y());
        c->scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
        c->translate(-srcRect.x(), -srcRect.y());
        HTMLVideoElement* video = static_cast<HTMLVideoElement*>(imageSource);
        video->paintCurrentFrame(c, IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())), &imagePaint);
    }

    c->restoreToCount(initialSaveCount);
}

bool shouldDisableDeferral(CanvasImageSource* imageSource, DisableDeferralReason* reason)
{
    ASSERT(reason);
    ASSERT(*reason == DisableDeferralReasonUnknown);

    if (imageSource->isVideoElement()) {
        *reason = DisableDeferralReasonDrawImageOfVideo;
        return true;
    }
    if (imageSource->isCanvasElement()) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(imageSource);
        if (canvas->isAnimated2D()) {
            *reason = DisableDeferralReasonDrawImageOfAnimated2dCanvas;
            return true;
        }
    }
    return false;
}

void BaseRenderingContext2D::drawImage(ExecutionContext* executionContext, CanvasImageSource* imageSource,
    double sx, double sy, double sw, double sh,
    double dx, double dy, double dw, double dh, ExceptionState& exceptionState)
{
    if (!drawingCanvas())
        return;

    RefPtr<Image> image;
    FloatSize defaultObjectSize(width(), height());
    SourceImageStatus sourceImageStatus = InvalidSourceImageStatus;
    if (!imageSource->isVideoElement()) {
        AccelerationHint hint = imageBuffer()->isAccelerated() ? PreferAcceleration : PreferNoAcceleration;
        image = imageSource->getSourceImageForCanvas(&sourceImageStatus, hint, SnapshotReasonDrawImage, defaultObjectSize);
        if (sourceImageStatus == UndecodableSourceImageStatus)
            exceptionState.throwDOMException(InvalidStateError, "The HTMLImageElement provided is in the 'broken' state.");
        if (!image || !image->width() || !image->height())
            return;
    } else {
        if (!static_cast<HTMLVideoElement*>(imageSource)->hasAvailableVideoFrame())
            return;
    }

    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) || !std::isfinite(dh)
        || !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sw) || !std::isfinite(sh)
        || !dw || !dh || !sw || !sh)
        return;

    FloatRect srcRect = normalizeRect(FloatRect(sx, sy, sw, sh));
    FloatRect dstRect = normalizeRect(FloatRect(dx, dy, dw, dh));
    FloatSize imageSize = imageSource->elementSize(defaultObjectSize);

    clipRectsToImageRect(FloatRect(FloatPoint(), imageSize), &srcRect, &dstRect);

    imageSource->adjustDrawRects(&srcRect, &dstRect);

    if (srcRect.isEmpty())
        return;

    DisableDeferralReason reason = DisableDeferralReasonUnknown;
    if (shouldDisableDeferral(imageSource, &reason))
        disableDeferral(reason);
    else if (image->isTextureBacked())
        disableDeferral(DisableDeferralDrawImageWithTextureBackedSourceImage);

    validateStateStack();

    // TODO(xidachen): After collecting some data, come back and prune off
    // the ones that is not needed.
    Optional<ScopedUsHistogramTimer> timer;
    if (imageBuffer() && imageBuffer()->isAccelerated()) {
        if (imageSource->isVideoElement()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterVideoGPU, new CustomCountHistogram("Blink.Canvas.DrawImage.Video.GPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterVideoGPU);
        } else if (imageSource->isCanvasElement()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterCanvasGPU, new CustomCountHistogram("Blink.Canvas.DrawImage.Canvas.GPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterCanvasGPU);
        } else if (imageSource->isSVGSource()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterSVGGPU, new CustomCountHistogram("Blink.Canvas.DrawImage.SVG.GPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterSVGGPU);
        } else if (imageSource->isImageBitmap()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterImageBitmapGPU, new CustomCountHistogram("Blink.Canvas.DrawImage.ImageBitmap.GPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterImageBitmapGPU);
        } else {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterOthersGPU, new CustomCountHistogram("Blink.Canvas.DrawImage.Others.GPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterOthersGPU);
        }
    } else if (imageBuffer() && imageBuffer()->isRecording()) {
        if (imageSource->isVideoElement()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterVideoDisplayList, new CustomCountHistogram("Blink.Canvas.DrawImage.Video.DisplayList", 0, 10000000, 50));
            timer.emplace(scopedUsCounterVideoDisplayList);
        } else if (imageSource->isCanvasElement()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterCanvasDisplayList, new CustomCountHistogram("Blink.Canvas.DrawImage.Canvas.DisplayList", 0, 10000000, 50));
            timer.emplace(scopedUsCounterCanvasDisplayList);
        } else if (imageSource->isSVGSource()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterSVGDisplayList, new CustomCountHistogram("Blink.Canvas.DrawImage.SVG.DisplayList", 0, 10000000, 50));
            timer.emplace(scopedUsCounterSVGDisplayList);
        } else if (imageSource->isImageBitmap()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterImageBitmapDisplayList, new CustomCountHistogram("Blink.Canvas.DrawImage.ImageBitmap.DisplayList", 0, 10000000, 50));
            timer.emplace(scopedUsCounterImageBitmapDisplayList);
        } else {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterOthersDisplayList, new CustomCountHistogram("Blink.Canvas.DrawImage.Others.DisplayList", 0, 10000000, 50));
            timer.emplace(scopedUsCounterOthersDisplayList);
        }
    } else {
        if (imageSource->isVideoElement()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterVideoCPU, new CustomCountHistogram("Blink.Canvas.DrawImage.Video.CPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterVideoCPU);
        } else if (imageSource->isCanvasElement()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterCanvasCPU, new CustomCountHistogram("Blink.Canvas.DrawImage.Canvas.CPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterCanvasCPU);
        } else if (imageSource->isSVGSource()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterSVGCPU, new CustomCountHistogram("Blink.Canvas.DrawImage.SVG.CPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterSVGCPU);
        } else if (imageSource->isImageBitmap()) {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterImageBitmapCPU, new CustomCountHistogram("Blink.Canvas.DrawImage.ImageBitmap.CPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterImageBitmapCPU);
        } else {
            DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterOthersCPU, new CustomCountHistogram("Blink.Canvas.DrawImage.Others.CPU", 0, 10000000, 50));
            timer.emplace(scopedUsCounterOthersCPU);
        }
    }

    draw(
        [this, &imageSource, &image, &srcRect, dstRect](SkCanvas* c, const SkPaint* paint) // draw lambda
        {
            drawImageInternal(c, imageSource, image.get(), srcRect, dstRect, paint);
        },
        [this, &dstRect](const SkIRect& clipBounds) // overdraw test lambda
        {
            return rectContainsTransformedRect(dstRect, clipBounds);
        }, dstRect, CanvasRenderingContext2DState::ImagePaintType,
        imageSource->isOpaque() ? CanvasRenderingContext2DState::OpaqueImage : CanvasRenderingContext2DState::NonOpaqueImage);

    validateStateStack();

    bool isExpensive = false;

    if (ExpensiveCanvasHeuristicParameters::SVGImageSourcesAreExpensive && imageSource->isSVGSource())
        isExpensive = true;

    if (imageSize.width() * imageSize.height() > width() * height() * ExpensiveCanvasHeuristicParameters::ExpensiveImageSizeRatio)
        isExpensive = true;

    if (isExpensive) {
        ImageBuffer* buffer = imageBuffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }

    if (originClean() && wouldTaintOrigin(imageSource, executionContext))
        setOriginTainted();
}

void BaseRenderingContext2D::clearCanvas()
{
    FloatRect canvasRect(0, 0, width(), height());
    checkOverdraw(canvasRect, 0, CanvasRenderingContext2DState::NoImage, ClipFill);
    SkCanvas* c = drawingCanvas();
    if (c)
        c->clear(hasAlpha() ? SK_ColorTRANSPARENT : SK_ColorBLACK);
}

bool BaseRenderingContext2D::rectContainsTransformedRect(const FloatRect& rect, const SkIRect& transformedRect) const
{
    FloatQuad quad(rect);
    FloatQuad transformedQuad(FloatRect(transformedRect.x(), transformedRect.y(), transformedRect.width(), transformedRect.height()));
    return state().transform().mapQuad(quad).containsQuad(transformedQuad);
}

CanvasGradient* BaseRenderingContext2D::createLinearGradient(double x0, double y0, double x1, double y1)
{
    CanvasGradient* gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    return gradient;
}

CanvasGradient* BaseRenderingContext2D::createRadialGradient(double x0, double y0, double r0, double x1, double y1, double r1, ExceptionState& exceptionState)
{
    if (r0 < 0 || r1 < 0) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The %s provided is less than 0.", r0 < 0 ? "r0" : "r1"));
        return nullptr;
    }

    CanvasGradient* gradient = CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    return gradient;
}

CanvasPattern* BaseRenderingContext2D::createPattern(ExecutionContext* executionContext, const CanvasImageSourceUnion& imageSource, const String& repetitionType, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource, exceptionState);
    if (!imageSourceInternal) {
        return nullptr;
    }

    return createPattern(executionContext, imageSourceInternal, repetitionType, exceptionState);
}

CanvasPattern* BaseRenderingContext2D::createPattern(ExecutionContext* executionContext, CanvasImageSource* imageSource, const String& repetitionType, ExceptionState& exceptionState)
{
    if (!imageSource) {
        return nullptr;
    }

    Pattern::RepeatMode repeatMode = CanvasPattern::parseRepetitionType(repetitionType, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    SourceImageStatus status;

    FloatSize defaultObjectSize(width(), height());
    RefPtr<Image> imageForRendering = imageSource->getSourceImageForCanvas(&status, PreferNoAcceleration, SnapshotReasonCreatePattern, defaultObjectSize);

    switch (status) {
    case NormalSourceImageStatus:
        break;
    case ZeroSizeCanvasSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, String::format("The canvas %s is 0.", imageSource->elementSize(defaultObjectSize).width() ? "height" : "width"));
        return nullptr;
    case UndecodableSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, "Source image is in the 'broken' state.");
        return nullptr;
    case InvalidSourceImageStatus:
        imageForRendering = Image::nullImage();
        break;
    case IncompleteSourceImageStatus:
        return nullptr;
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT(imageForRendering);

    bool originClean = !wouldTaintOrigin(imageSource, executionContext);

    return CanvasPattern::create(imageForRendering.release(), repeatMode, originClean);
}

bool BaseRenderingContext2D::computeDirtyRect(const FloatRect& localRect, SkIRect* dirtyRect)
{
    SkIRect clipBounds;
    if (!drawingCanvas()->getClipDeviceBounds(&clipBounds))
        return false;
    return computeDirtyRect(localRect, clipBounds, dirtyRect);
}

bool BaseRenderingContext2D::computeDirtyRect(const FloatRect& localRect, const SkIRect& transformedClipBounds, SkIRect* dirtyRect)
{
    FloatRect canvasRect = state().transform().mapRect(localRect);

    if (alphaChannel(state().shadowColor())) {
        FloatRect shadowRect(canvasRect);
        shadowRect.move(state().shadowOffset());
        shadowRect.inflate(state().shadowBlur());
        canvasRect.unite(shadowRect);
    }

    SkIRect canvasIRect;
    static_cast<SkRect>(canvasRect).roundOut(&canvasIRect);
    if (!canvasIRect.intersect(transformedClipBounds))
        return false;

    if (dirtyRect)
        *dirtyRect = canvasIRect;

    return true;
}

ImageData* BaseRenderingContext2D::createImageData(ImageData* imageData, ExceptionState &exceptionState) const
{
    ImageData* result = ImageData::create(imageData->size());
    if (!result)
        exceptionState.throwRangeError("Out of memory at ImageData creation");
    return result;
}

ImageData* BaseRenderingContext2D::createImageData(double sw, double sh, ExceptionState& exceptionState) const
{
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));
        return nullptr;
    }

    FloatSize logicalSize(fabs(sw), fabs(sh));
    if (!logicalSize.isExpressibleAsIntSize())
        return nullptr;

    IntSize size = expandedIntSize(logicalSize);
    if (size.width() < 1)
        size.setWidth(1);
    if (size.height() < 1)
        size.setHeight(1);

    ImageData* result = ImageData::create(size);
    if (!result)
        exceptionState.throwRangeError("Out of memory at ImageData creation");
    return result;
}

ImageData* BaseRenderingContext2D::getImageData(double sx, double sy, double sw, double sh, ExceptionState& exceptionState) const
{
    m_usageCounters.numGetImageDataCalls++;
    if (!originClean())
        exceptionState.throwSecurityError("The canvas has been tainted by cross-origin data.");
    else if (!sw || !sh)
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));

    if (exceptionState.hadException())
        return nullptr;

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    FloatRect logicalRect(sx, sy, sw, sh);
    if (logicalRect.width() < 1)
        logicalRect.setWidth(1);
    if (logicalRect.height() < 1)
        logicalRect.setHeight(1);
    if (!logicalRect.isExpressibleAsIntRect())
        return nullptr;

    Optional<ScopedUsHistogramTimer> timer;
    if (imageBuffer() && imageBuffer()->isAccelerated()) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterGPU, new CustomCountHistogram("Blink.Canvas.GetImageData.GPU", 0, 10000000, 50));
        timer.emplace(scopedUsCounterGPU);
    } else if (imageBuffer() && imageBuffer()->isRecording()) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterDisplayList, new CustomCountHistogram("Blink.Canvas.GetImageData.DisplayList", 0, 10000000, 50));
        timer.emplace(scopedUsCounterDisplayList);
    } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterCPU, new CustomCountHistogram("Blink.Canvas.GetImageData.CPU", 0, 10000000, 50));
        timer.emplace(scopedUsCounterCPU);
    }

    IntRect imageDataRect = enclosingIntRect(logicalRect);
    ImageBuffer* buffer = imageBuffer();
    if (!buffer || isContextLost()) {
        ImageData* result = ImageData::create(imageDataRect.size());
        if (!result)
            exceptionState.throwRangeError("Out of memory at ImageData creation");
        return result;
    }

    WTF::ArrayBufferContents contents;
    if (!buffer->getImageData(Unmultiplied, imageDataRect, contents)) {
        exceptionState.throwRangeError("Out of memory at ImageData creation");
        return nullptr;
    }

    DOMArrayBuffer* arrayBuffer = DOMArrayBuffer::create(contents);
    return ImageData::create(
        imageDataRect.size(),
        DOMUint8ClampedArray::create(arrayBuffer, 0, arrayBuffer->byteLength()));
}

void BaseRenderingContext2D::putImageData(ImageData* data, double dx, double dy, ExceptionState& exceptionState)
{
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), exceptionState);
}

void BaseRenderingContext2D::putImageData(ImageData* data, double dx, double dy, double dirtyX, double dirtyY, double dirtyWidth, double dirtyHeight, ExceptionState& exceptionState)
{
    m_usageCounters.numPutImageDataCalls++;
    if (data->data()->bufferBase()->isNeutered()) {
        exceptionState.throwDOMException(InvalidStateError, "The source data has been neutered.");
        return;
    }
    ImageBuffer* buffer = imageBuffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect destRect = enclosingIntRect(clipRect);
    destRect.move(destOffset);
    destRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (destRect.isEmpty())
        return;

    Optional<ScopedUsHistogramTimer> timer;
    if (imageBuffer() && imageBuffer()->isAccelerated()) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterGPU, new CustomCountHistogram("Blink.Canvas.PutImageData.GPU", 0, 10000000, 50));
        timer.emplace(scopedUsCounterGPU);
    } else if (imageBuffer() && imageBuffer()->isRecording()) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterDisplayList, new CustomCountHistogram("Blink.Canvas.PutImageData.DisplayList", 0, 10000000, 50));
        timer.emplace(scopedUsCounterDisplayList);
    } else {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, scopedUsCounterCPU, new CustomCountHistogram("Blink.Canvas.PutImageData.CPU", 0, 10000000, 50));
        timer.emplace(scopedUsCounterCPU);
    }

    IntRect sourceRect(destRect);
    sourceRect.move(-destOffset);

    checkOverdraw(destRect, 0, CanvasRenderingContext2DState::NoImage, UntransformedUnclippedFill);

    buffer->putByteArray(Unmultiplied, data->data()->data(), IntSize(data->width(), data->height()), sourceRect, IntPoint(destOffset));

    didDraw(destRect);
}

void BaseRenderingContext2D::inflateStrokeRect(FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const double root2 = sqrtf(2);
    double delta = state().lineWidth() / 2;
    if (state().getLineJoin() == MiterJoin)
        delta *= state().miterLimit();
    else if (state().getLineCap() == SquareCap)
        delta *= root2;

    rect.inflate(delta);
}

bool BaseRenderingContext2D::imageSmoothingEnabled() const
{
    return state().imageSmoothingEnabled();
}

void BaseRenderingContext2D::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().imageSmoothingEnabled())
        return;

    modifiableState().setImageSmoothingEnabled(enabled);
}

String BaseRenderingContext2D::imageSmoothingQuality() const
{
    return state().imageSmoothingQuality();
}

void BaseRenderingContext2D::setImageSmoothingQuality(const String& quality)
{
    if (quality == state().imageSmoothingQuality())
        return;

    modifiableState().setImageSmoothingQuality(quality);
}

void BaseRenderingContext2D::checkOverdraw(const SkRect& rect, const SkPaint* paint, CanvasRenderingContext2DState::ImageType imageType, DrawType drawType)
{
    SkCanvas* c = drawingCanvas();
    if (!c || !imageBuffer()->isRecording())
        return;

    SkRect deviceRect;
    if (drawType == UntransformedUnclippedFill) {
        deviceRect = rect;
    } else {
        ASSERT(drawType == ClipFill);
        if (state().hasComplexClip())
            return;

        SkIRect skIBounds;
        if (!c->getClipDeviceBounds(&skIBounds))
            return;
        deviceRect = SkRect::Make(skIBounds);
    }

    const SkImageInfo& imageInfo = c->imageInfo();
    if (!deviceRect.contains(SkRect::MakeWH(imageInfo.width(), imageInfo.height())))
        return;

    bool isSourceOver = true;
    unsigned alpha = 0xFF;
    if (paint) {
        if (paint->getLooper() || paint->getImageFilter() || paint->getMaskFilter())
            return;

        SkXfermode* xfermode = paint->getXfermode();
        if (xfermode) {
            SkXfermode::Mode mode;
            if (xfermode->asMode(&mode)) {
                isSourceOver = mode == SkXfermode::kSrcOver_Mode;
                if (!isSourceOver && mode != SkXfermode::kSrc_Mode && mode != SkXfermode::kClear_Mode)
                    return; // The code below only knows how to handle Src, SrcOver, and Clear
            } else {
                // unknown xfermode
                ASSERT_NOT_REACHED();
                return;
            }
        }

        alpha = paint->getAlpha();

        if (isSourceOver && imageType == CanvasRenderingContext2DState::NoImage) {
            SkShader* shader = paint->getShader();
            if (shader) {
                if (shader->isOpaque() && alpha == 0xFF)
                    imageBuffer()->willOverwriteCanvas();
                return;
            }
        }
    }

    if (isSourceOver) {
        // With source over, we need to certify that alpha == 0xFF for all pixels
        if (imageType == CanvasRenderingContext2DState::NonOpaqueImage)
            return;
        if (alpha < 0xFF)
            return;
    }

    imageBuffer()->willOverwriteCanvas();
}

void BaseRenderingContext2D::trackDrawCall(DrawCallType callType, Path2D* path2d)
{
    m_usageCounters.numDrawCalls[callType]++;

    if (callType == FillPath) {
        SkPath skPath;
        if (path2d) {
            skPath = path2d->path().getSkPath();
        } else {
            skPath = m_path.getSkPath();
        }
        if (!(skPath.getConvexity() == SkPath::kConvex_Convexity)) {
            m_usageCounters.numNonConvexFillPathCalls++;
        }
    }

    if (callType == FillText
        || callType == FillPath
        || callType == StrokeText
        || callType == StrokePath
        || callType == FillRect
        || callType == StrokeRect) {

        CanvasStyle* canvasStyle;
        if (callType == FillText || callType == FillPath || callType == FillRect) {
            canvasStyle = state().fillStyle();
        } else {
            canvasStyle = state().strokeStyle();
        }

        if (canvasStyle->getCanvasGradient()) {
            m_usageCounters.numGradients++;
        }

        if (canvasStyle->getCanvasPattern()) {
            m_usageCounters.numPatterns++;
        }
    }

    if (state().shadowBlur() > 0.0 && SkColorGetA(state().shadowColor()) > 0) {
        m_usageCounters.numBlurredShadows++;
    }

    if (state().hasComplexClip()) {
        m_usageCounters.numDrawWithComplexClips++;
    }

    if (stateHasFilter()) {
        m_usageCounters.numFilters++;
    }
}

const BaseRenderingContext2D::UsageCounters& BaseRenderingContext2D::getUsage()
{
    return m_usageCounters;
}

DEFINE_TRACE(BaseRenderingContext2D)
{
    visitor->trace(m_stateStack);
}

BaseRenderingContext2D::UsageCounters::UsageCounters() :
    numDrawCalls {0, 0, 0, 0, 0, 0},
    numNonConvexFillPathCalls(0),
    numGradients(0),
    numPatterns(0),
    numDrawWithComplexClips(0),
    numBlurredShadows(0),
    numFilters(0),
    numGetImageDataCalls(0),
    numPutImageDataCalls(0),
    numClearRectCalls(0),
    numDrawFocusCalls(0) {}

} // namespace blink
