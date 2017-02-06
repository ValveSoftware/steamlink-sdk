// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/GraphicsContextState.h"

#include "platform/graphics/skia/SkiaUtils.h"

namespace blink {

static inline SkFilterQuality filterQualityForPaint(InterpolationQuality quality)
{
    // The filter quality "selected" here will primarily be used when painting a
    // primitive using one of the SkPaints below. For the most part this will
    // not affect things that are part of the Image class hierarchy (which use
    // the unmodified m_interpolationQuality.)
    return quality != InterpolationNone ? kLow_SkFilterQuality : kNone_SkFilterQuality;
}

GraphicsContextState::GraphicsContextState()
    : m_textDrawingMode(TextModeFill)
    , m_interpolationQuality(InterpolationDefault)
    , m_saveCount(0)
    , m_shouldAntialias(true)
{
    m_strokePaint.setStyle(SkPaint::kStroke_Style);
    m_strokePaint.setStrokeWidth(SkFloatToScalar(m_strokeData.thickness()));
    m_strokePaint.setStrokeCap(SkPaint::kDefault_Cap);
    m_strokePaint.setStrokeJoin(SkPaint::kDefault_Join);
    m_strokePaint.setStrokeMiter(SkFloatToScalar(m_strokeData.miterLimit()));
    m_strokePaint.setFilterQuality(filterQualityForPaint(m_interpolationQuality));
    m_strokePaint.setAntiAlias(m_shouldAntialias);
    m_fillPaint.setFilterQuality(filterQualityForPaint(m_interpolationQuality));
    m_fillPaint.setAntiAlias(m_shouldAntialias);
}

GraphicsContextState::GraphicsContextState(const GraphicsContextState& other)
    : m_strokePaint(other.m_strokePaint)
    , m_fillPaint(other.m_fillPaint)
    , m_strokeData(other.m_strokeData)
    , m_textDrawingMode(other.m_textDrawingMode)
    , m_interpolationQuality(other.m_interpolationQuality)
    , m_saveCount(0)
    , m_shouldAntialias(other.m_shouldAntialias) { }

void GraphicsContextState::copy(const GraphicsContextState& source)
{
    this->~GraphicsContextState();
    new (this) GraphicsContextState(source);
}

const SkPaint& GraphicsContextState::strokePaint(int strokedPathLength) const
{
    m_strokeData.setupPaintDashPathEffect(&m_strokePaint, strokedPathLength);
    return m_strokePaint;
}

void GraphicsContextState::setStrokeStyle(StrokeStyle style)
{
    m_strokeData.setStyle(style);
}

void GraphicsContextState::setStrokeThickness(float thickness)
{
    m_strokeData.setThickness(thickness);
    m_strokePaint.setStrokeWidth(SkFloatToScalar(thickness));
}

void GraphicsContextState::setStrokeColor(const Color& color)
{
    m_strokePaint.setColor(color.rgb());
    m_strokePaint.setShader(0);
}

void GraphicsContextState::setLineCap(LineCap cap)
{
    m_strokeData.setLineCap(cap);
    m_strokePaint.setStrokeCap((SkPaint::Cap)cap);
}

void GraphicsContextState::setLineJoin(LineJoin join)
{
    m_strokeData.setLineJoin(join);
    m_strokePaint.setStrokeJoin((SkPaint::Join)join);
}

void GraphicsContextState::setMiterLimit(float miterLimit)
{
    m_strokeData.setMiterLimit(miterLimit);
    m_strokePaint.setStrokeMiter(SkFloatToScalar(miterLimit));
}

void GraphicsContextState::setFillColor(const Color& color)
{
    m_fillPaint.setColor(color.rgb());
    m_fillPaint.setShader(0);
}

// Shadow. (This will need tweaking if we use draw loopers for other things.)
void GraphicsContextState::setDrawLooper(PassRefPtr<SkDrawLooper> drawLooper)
{
    // Grab a new ref for stroke.
    m_strokePaint.setLooper(sk_ref_sp(drawLooper.get()));
    // Pass the existing ref to fill (to minimize refcount churn).
    m_fillPaint.setLooper(toSkSp(drawLooper));
}

void GraphicsContextState::setLineDash(const DashArray& dashes, float dashOffset)
{
    m_strokeData.setLineDash(dashes, dashOffset);
}

void GraphicsContextState::setColorFilter(PassRefPtr<SkColorFilter> colorFilter)
{
    // Grab a new ref for stroke.
    m_strokePaint.setColorFilter(sk_ref_sp(colorFilter.get()));
    // Pass the existing ref to fill (to minimize refcount churn).
    m_fillPaint.setColorFilter(toSkSp(colorFilter));
}

void GraphicsContextState::setInterpolationQuality(InterpolationQuality quality)
{
    m_interpolationQuality = quality;
    m_strokePaint.setFilterQuality(filterQualityForPaint(quality));
    m_fillPaint.setFilterQuality(filterQualityForPaint(quality));
}

void GraphicsContextState::setShouldAntialias(bool shouldAntialias)
{
    m_shouldAntialias = shouldAntialias;
    m_strokePaint.setAntiAlias(shouldAntialias);
    m_fillPaint.setAntiAlias(shouldAntialias);
}

} // namespace blink
