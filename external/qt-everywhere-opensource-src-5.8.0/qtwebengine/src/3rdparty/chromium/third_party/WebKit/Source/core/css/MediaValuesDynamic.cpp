// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesDynamic.h"

#include "core/css/CSSHelper.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/css/MediaValuesCached.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

MediaValues* MediaValuesDynamic::create(Document& document)
{
    return MediaValuesDynamic::create(frameFrom(document));
}

MediaValues* MediaValuesDynamic::create(LocalFrame* frame)
{
    if (!frame || !frame->view() || !frame->document() || frame->document()->layoutViewItem().isNull())
        return MediaValuesCached::create();
    return new MediaValuesDynamic(frame);
}

MediaValuesDynamic::MediaValuesDynamic(LocalFrame* frame)
    : m_frame(frame)
    , m_viewportDimensionsOverridden(false)
    , m_viewportWidthOverride(0)
    , m_viewportHeightOverride(0)
{
    ASSERT(m_frame);
}

MediaValuesDynamic::MediaValuesDynamic(LocalFrame* frame, bool overriddenViewportDimensions, double viewportWidth, double viewportHeight)
    : m_frame(frame)
    , m_viewportDimensionsOverridden(overriddenViewportDimensions)
    , m_viewportWidthOverride(viewportWidth)
    , m_viewportHeightOverride(viewportHeight)
{
    ASSERT(m_frame);
}

MediaValues* MediaValuesDynamic::copy() const
{
    return new MediaValuesDynamic(m_frame, m_viewportDimensionsOverridden, m_viewportWidthOverride, m_viewportHeightOverride);
}

bool MediaValuesDynamic::computeLength(double value, CSSPrimitiveValue::UnitType type, int& result) const
{
    return MediaValues::computeLength(value,
        type,
        calculateDefaultFontSize(m_frame),
        calculateViewportWidth(m_frame),
        calculateViewportHeight(m_frame),
        result);
}

bool MediaValuesDynamic::computeLength(double value, CSSPrimitiveValue::UnitType type, double& result) const
{
    return MediaValues::computeLength(value,
        type,
        calculateDefaultFontSize(m_frame),
        calculateViewportWidth(m_frame),
        calculateViewportHeight(m_frame),
        result);
}

double MediaValuesDynamic::viewportWidth() const
{
    if (m_viewportDimensionsOverridden)
        return m_viewportWidthOverride;
    return calculateViewportWidth(m_frame);
}

double MediaValuesDynamic::viewportHeight() const
{
    if (m_viewportDimensionsOverridden)
        return m_viewportHeightOverride;
    return calculateViewportHeight(m_frame);
}

int MediaValuesDynamic::deviceWidth() const
{
    return calculateDeviceWidth(m_frame);
}

int MediaValuesDynamic::deviceHeight() const
{
    return calculateDeviceHeight(m_frame);
}

float MediaValuesDynamic::devicePixelRatio() const
{
    return calculateDevicePixelRatio(m_frame);
}

int MediaValuesDynamic::colorBitsPerComponent() const
{
    return calculateColorBitsPerComponent(m_frame);
}

int MediaValuesDynamic::monochromeBitsPerComponent() const
{
    return calculateMonochromeBitsPerComponent(m_frame);
}

PointerType MediaValuesDynamic::primaryPointerType() const
{
    return calculatePrimaryPointerType(m_frame);
}

int MediaValuesDynamic::availablePointerTypes() const
{
    return calculateAvailablePointerTypes(m_frame);
}

HoverType MediaValuesDynamic::primaryHoverType() const
{
    return calculatePrimaryHoverType(m_frame);
}

int MediaValuesDynamic::availableHoverTypes() const
{
    return calculateAvailableHoverTypes(m_frame);
}

bool MediaValuesDynamic::threeDEnabled() const
{
    return calculateThreeDEnabled(m_frame);
}

const String MediaValuesDynamic::mediaType() const
{
    return calculateMediaType(m_frame);
}

WebDisplayMode MediaValuesDynamic::displayMode() const
{
    return calculateDisplayMode(m_frame);
}

bool MediaValuesDynamic::strictMode() const
{
    return calculateStrictMode(m_frame);
}

Document* MediaValuesDynamic::document() const
{
    return m_frame->document();
}

bool MediaValuesDynamic::hasValues() const
{
    return m_frame;
}

DEFINE_TRACE(MediaValuesDynamic)
{
    visitor->trace(m_frame);
    MediaValues::trace(visitor);
}

void MediaValuesDynamic::overrideViewportDimensions(double width, double height)
{
    m_viewportDimensionsOverridden = true;
    m_viewportWidthOverride = width;
    m_viewportHeightOverride = height;
}

} // namespace blink
