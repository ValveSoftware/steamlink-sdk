// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/MediaValuesCached.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/rendering/RenderObject.h"

namespace WebCore {

PassRefPtr<MediaValues> MediaValuesCached::create()
{
    return adoptRef(new MediaValuesCached());
}

PassRefPtr<MediaValues> MediaValuesCached::create(MediaValuesCachedData& data)
{
    return adoptRef(new MediaValuesCached(data));
}

PassRefPtr<MediaValues> MediaValuesCached::create(Document& document)
{
    Document* executingDocument = document.importsController() ? document.importsController()->master() : &document;
    ASSERT(executingDocument);
    return MediaValuesCached::create(executingDocument->frame());
}

PassRefPtr<MediaValues> MediaValuesCached::create(LocalFrame* frame)
{
    // FIXME - Added an assert here so we can better understand when a frame is present without its view().
    ASSERT(!frame || frame->view());
    // FIXME - Because of crbug.com/371084, document()->renderView() may be null here.
    if (!frame || !frame->view() || !frame->document() || !frame->document()->renderView())
        return adoptRef(new MediaValuesCached());
    return adoptRef(new MediaValuesCached(frame));
}

MediaValuesCached::MediaValuesCached()
{
}

MediaValuesCached::MediaValuesCached(LocalFrame* frame)
{
    ASSERT(isMainThread());
    ASSERT(frame);
    // In case that frame is missing (e.g. for images that their document does not have a frame)
    // We simply leave the MediaValues object with the default MediaValuesCachedData values.
    m_data.viewportWidth = calculateViewportWidth(frame);
    m_data.viewportHeight = calculateViewportHeight(frame);
    m_data.deviceWidth = calculateDeviceWidth(frame);
    m_data.deviceHeight = calculateDeviceHeight(frame);
    m_data.devicePixelRatio = calculateDevicePixelRatio(frame);
    m_data.colorBitsPerComponent = calculateColorBitsPerComponent(frame);
    m_data.monochromeBitsPerComponent = calculateMonochromeBitsPerComponent(frame);
    m_data.pointer = calculateLeastCapablePrimaryPointerDeviceType(frame);
    m_data.defaultFontSize = calculateDefaultFontSize(frame);
    m_data.threeDEnabled = calculateThreeDEnabled(frame);
    m_data.scanMediaType = calculateScanMediaType(frame);
    m_data.screenMediaType = calculateScreenMediaType(frame);
    m_data.printMediaType = calculatePrintMediaType(frame);
    m_data.strictMode = calculateStrictMode(frame);
}

MediaValuesCached::MediaValuesCached(const MediaValuesCachedData& data)
    : m_data(data)
{
}

PassRefPtr<MediaValues> MediaValuesCached::copy() const
{
    return adoptRef(new MediaValuesCached(m_data));
}

bool MediaValuesCached::computeLength(double value, CSSPrimitiveValue::UnitType type, int& result) const
{
    return MediaValues::computeLength(value, type, m_data.defaultFontSize, m_data.viewportWidth, m_data.viewportHeight, result);
}

bool MediaValuesCached::computeLength(double value, CSSPrimitiveValue::UnitType type, double& result) const
{
    return MediaValues::computeLength(value, type, m_data.defaultFontSize, m_data.viewportWidth, m_data.viewportHeight, result);
}

bool MediaValuesCached::isSafeToSendToAnotherThread() const
{
    return hasOneRef();
}

int MediaValuesCached::viewportWidth() const
{
    return m_data.viewportWidth;
}

int MediaValuesCached::viewportHeight() const
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

MediaValues::PointerDeviceType MediaValuesCached::pointer() const
{
    return m_data.pointer;
}

bool MediaValuesCached::threeDEnabled() const
{
    return m_data.threeDEnabled;
}

bool MediaValuesCached::scanMediaType() const
{
    return m_data.scanMediaType;
}

bool MediaValuesCached::screenMediaType() const
{
    return m_data.screenMediaType;
}

bool MediaValuesCached::printMediaType() const
{
    return m_data.printMediaType;
}

bool MediaValuesCached::strictMode() const
{
    return m_data.strictMode;
}

Document* MediaValuesCached::document() const
{
    return 0;
}

bool MediaValuesCached::hasValues() const
{
    return true;
}

} // namespace
