// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValues.h"

#include "core/css/CSSHelper.h"
#include "core/css/MediaValuesCached.h"
#include "core/css/MediaValuesDynamic.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "public/platform/WebScreenInfo.h"

namespace blink {

MediaValues* MediaValues::createDynamicIfFrameExists(LocalFrame* frame)
{
    if (frame)
        return MediaValuesDynamic::create(frame);
    return MediaValuesCached::create();
}

double MediaValues::calculateViewportWidth(LocalFrame* frame)
{
    ASSERT(frame && frame->view() && frame->document());
    int viewportWidth = frame->view()->layoutSize(IncludeScrollbars).width();
    return adjustDoubleForAbsoluteZoom(viewportWidth, frame->document()->layoutViewItem().styleRef());
}

double MediaValues::calculateViewportHeight(LocalFrame* frame)
{
    ASSERT(frame && frame->view() && frame->document());
    int viewportHeight = frame->view()->layoutSize(IncludeScrollbars).height();
    return adjustDoubleForAbsoluteZoom(viewportHeight, frame->document()->layoutViewItem().styleRef());
}

int MediaValues::calculateDeviceWidth(LocalFrame* frame)
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    blink::WebScreenInfo screenInfo = frame->host()->chromeClient().screenInfo();
    int deviceWidth = screenInfo.rect.width;
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceWidth = lroundf(deviceWidth * screenInfo.deviceScaleFactor);
    return deviceWidth;
}

int MediaValues::calculateDeviceHeight(LocalFrame* frame)
{
    ASSERT(frame && frame->view() && frame->settings() && frame->host());
    blink::WebScreenInfo screenInfo = frame->host()->chromeClient().screenInfo();
    int deviceHeight = screenInfo.rect.height;
    if (frame->settings()->reportScreenSizeInPhysicalPixelsQuirk())
        deviceHeight = lroundf(deviceHeight * screenInfo.deviceScaleFactor);
    return deviceHeight;
}

bool MediaValues::calculateStrictMode(LocalFrame* frame)
{
    ASSERT(frame && frame->document());
    return !frame->document()->inQuirksMode();
}

float MediaValues::calculateDevicePixelRatio(LocalFrame* frame)
{
    return frame->devicePixelRatio();
}

int MediaValues::calculateColorBitsPerComponent(LocalFrame* frame)
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (!frame->page()->mainFrame()->isLocalFrame()
        || frame->host()->chromeClient().screenInfo().isMonochrome)
        return 0;
    return frame->host()->chromeClient().screenInfo().depthPerComponent;
}

int MediaValues::calculateMonochromeBitsPerComponent(LocalFrame* frame)
{
    ASSERT(frame && frame->page() && frame->page()->mainFrame());
    if (!frame->page()->mainFrame()->isLocalFrame()
        || !frame->host()->chromeClient().screenInfo().isMonochrome)
        return 0;
    return frame->host()->chromeClient().screenInfo().depthPerComponent;
}

int MediaValues::calculateDefaultFontSize(LocalFrame* frame)
{
    return frame->host()->settings().defaultFontSize();
}

const String MediaValues::calculateMediaType(LocalFrame* frame)
{
    ASSERT(frame);
    if (!frame->view())
        return emptyAtom;
    return frame->view()->mediaType();
}

WebDisplayMode MediaValues::calculateDisplayMode(LocalFrame* frame)
{
    ASSERT(frame);
    WebDisplayMode mode = frame->host()->settings().displayModeOverride();

    if (mode != WebDisplayModeUndefined)
        return mode;

    if (!frame->view())
        return WebDisplayModeBrowser;

    return frame->view()->displayMode();
}

bool MediaValues::calculateThreeDEnabled(LocalFrame* frame)
{
    ASSERT(frame && !frame->contentLayoutItem().isNull() && frame->contentLayoutItem().compositor());
    bool threeDEnabled = false;
    if (LayoutViewItem view = frame->contentLayoutItem())
        threeDEnabled = view.compositor()->hasAcceleratedCompositing();
    return threeDEnabled;
}

PointerType MediaValues::calculatePrimaryPointerType(LocalFrame* frame)
{
    ASSERT(frame && frame->settings());
    return frame->settings()->primaryPointerType();
}

int MediaValues::calculateAvailablePointerTypes(LocalFrame* frame)
{
    ASSERT(frame && frame->settings());
    return frame->settings()->availablePointerTypes();
}

HoverType MediaValues::calculatePrimaryHoverType(LocalFrame* frame)
{
    ASSERT(frame && frame->settings());
    return frame->settings()->primaryHoverType();
}

int MediaValues::calculateAvailableHoverTypes(LocalFrame* frame)
{
    ASSERT(frame && frame->settings());
    return frame->settings()->availableHoverTypes();
}

bool MediaValues::computeLengthImpl(double value, CSSPrimitiveValue::UnitType type, unsigned defaultFontSize, double viewportWidth, double viewportHeight, double& result)
{
    // The logic in this function is duplicated from CSSToLengthConversionData::zoomedComputedPixels()
    // because MediaValues::computeLength() needs nearly identical logic, but we haven't found a way to make
    // CSSToLengthConversionData::zoomedComputedPixels() more generic (to solve both cases) without hurting performance.

    // FIXME - Unite the logic here with CSSToLengthConversionData in a performant way.
    switch (type) {
    case CSSPrimitiveValue::UnitType::Ems:
    case CSSPrimitiveValue::UnitType::Rems:
        result = value * defaultFontSize;
        return true;
    case CSSPrimitiveValue::UnitType::Pixels:
    case CSSPrimitiveValue::UnitType::UserUnits:
        result = value;
        return true;
    case CSSPrimitiveValue::UnitType::Exs:
        // FIXME: We have a bug right now where the zoom will be applied twice to EX units.
    case CSSPrimitiveValue::UnitType::Chs:
        // FIXME: We don't seem to be able to cache fontMetrics related values.
        // Trying to access them is triggering some sort of microtask. Serving the spec's default instead.
        result = (value * defaultFontSize) / 2.0;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportWidth:
        result = (value * viewportWidth) / 100.0;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportHeight:
        result = (value * viewportHeight) / 100.0;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportMin:
        result = (value * std::min(viewportWidth, viewportHeight)) / 100.0;
        return true;
    case CSSPrimitiveValue::UnitType::ViewportMax:
        result = (value * std::max(viewportWidth, viewportHeight)) / 100.0;
        return true;
    case CSSPrimitiveValue::UnitType::Centimeters:
        result = value * cssPixelsPerCentimeter;
        return true;
    case CSSPrimitiveValue::UnitType::Millimeters:
        result = value * cssPixelsPerMillimeter;
        return true;
    case CSSPrimitiveValue::UnitType::Inches:
        result = value * cssPixelsPerInch;
        return true;
    case CSSPrimitiveValue::UnitType::Points:
        result = value * cssPixelsPerPoint;
        return true;
    case CSSPrimitiveValue::UnitType::Picas:
        result = value * cssPixelsPerPica;
        return true;
    default:
        return false;
    }
}

LocalFrame* MediaValues::frameFrom(Document& document)
{
    Document* executingDocument = document.importsController() ? document.importsController()->master() : &document;
    ASSERT(executingDocument);
    return executingDocument->frame();
}

} // namespace blink
