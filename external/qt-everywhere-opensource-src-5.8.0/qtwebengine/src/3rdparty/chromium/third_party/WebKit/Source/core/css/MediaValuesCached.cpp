// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesCached.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

MediaValuesCached::MediaValuesCachedData::MediaValuesCachedData(Document& document)
    : MediaValuesCached::MediaValuesCachedData()
{
    ASSERT(isMainThread());
    LocalFrame* frame = MediaValues::frameFrom(document);
    // TODO(hiroshige): Clean up |frame->view()| conditions.
    ASSERT(!frame || frame->view());
    if (frame && frame->view()) {
        ASSERT(frame->document() && !frame->document()->layoutViewItem().isNull());

        // In case that frame is missing (e.g. for images that their document does not have a frame)
        // We simply leave the MediaValues object with the default MediaValuesCachedData values.
        viewportWidth = MediaValues::calculateViewportWidth(frame);
        viewportHeight = MediaValues::calculateViewportHeight(frame);
        deviceWidth = MediaValues::calculateDeviceWidth(frame);
        deviceHeight = MediaValues::calculateDeviceHeight(frame);
        devicePixelRatio = MediaValues::calculateDevicePixelRatio(frame);
        colorBitsPerComponent = MediaValues::calculateColorBitsPerComponent(frame);
        monochromeBitsPerComponent = MediaValues::calculateMonochromeBitsPerComponent(frame);
        primaryPointerType = MediaValues::calculatePrimaryPointerType(frame);
        availablePointerTypes = MediaValues::calculateAvailablePointerTypes(frame);
        primaryHoverType = MediaValues::calculatePrimaryHoverType(frame);
        availableHoverTypes = MediaValues::calculateAvailableHoverTypes(frame);
        defaultFontSize = MediaValues::calculateDefaultFontSize(frame);
        threeDEnabled = MediaValues::calculateThreeDEnabled(frame);
        strictMode = MediaValues::calculateStrictMode(frame);
        displayMode = MediaValues::calculateDisplayMode(frame);
        mediaType = MediaValues::calculateMediaType(frame);
    }
}

MediaValuesCached* MediaValuesCached::create()
{
    return new MediaValuesCached();
}

MediaValuesCached* MediaValuesCached::create(const MediaValuesCachedData& data)
{
    return new MediaValuesCached(data);
}

MediaValuesCached::MediaValuesCached()
{
}

MediaValuesCached::MediaValuesCached(const MediaValuesCachedData& data)
    : m_data(data)
{
}

MediaValues* MediaValuesCached::copy() const
{
    return new MediaValuesCached(m_data);
}

bool MediaValuesCached::computeLength(double value, CSSPrimitiveValue::UnitType type, int& result) const
{
    return MediaValues::computeLength(value, type, m_data.defaultFontSize, m_data.viewportWidth, m_data.viewportHeight, result);
}

bool MediaValuesCached::computeLength(double value, CSSPrimitiveValue::UnitType type, double& result) const
{
    return MediaValues::computeLength(value, type, m_data.defaultFontSize, m_data.viewportWidth, m_data.viewportHeight, result);
}

double MediaValuesCached::viewportWidth() const
{
    return m_data.viewportWidth;
}

double MediaValuesCached::viewportHeight() const
{
    return m_data.viewportHeight;
}

int MediaValuesCached::deviceWidth() const
{
    return m_data.deviceWidth;
}

int MediaValuesCached::deviceHeight() const
{
    return m_data.deviceHeight;
}

float MediaValuesCached::devicePixelRatio() const
{
    return m_data.devicePixelRatio;
}

int MediaValuesCached::colorBitsPerComponent() const
{
    return m_data.colorBitsPerComponent;
}

int MediaValuesCached::monochromeBitsPerComponent() const
{
    return m_data.monochromeBitsPerComponent;
}

PointerType MediaValuesCached::primaryPointerType() const
{
    return m_data.primaryPointerType;
}

int MediaValuesCached::availablePointerTypes() const
{
    return m_data.availablePointerTypes;
}

HoverType MediaValuesCached::primaryHoverType() const
{
    return m_data.primaryHoverType;
}

int MediaValuesCached::availableHoverTypes() const
{
    return m_data.availableHoverTypes;
}

bool MediaValuesCached::threeDEnabled() const
{
    return m_data.threeDEnabled;
}

bool MediaValuesCached::strictMode() const
{
    return m_data.strictMode;
}

const String MediaValuesCached::mediaType() const
{
    return m_data.mediaType;
}

WebDisplayMode MediaValuesCached::displayMode() const
{
    return m_data.displayMode;
}

Document* MediaValuesCached::document() const
{
    return nullptr;
}

bool MediaValuesCached::hasValues() const
{
    return true;
}

void MediaValuesCached::overrideViewportDimensions(double width, double height)
{
    m_data.viewportWidth = width;
    m_data.viewportHeight = height;
}

} // namespace blink
