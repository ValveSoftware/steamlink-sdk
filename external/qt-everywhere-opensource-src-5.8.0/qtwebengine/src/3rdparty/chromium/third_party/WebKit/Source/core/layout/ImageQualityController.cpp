/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "core/layout/ImageQualityController.h"

#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "wtf/PtrUtil.h"

namespace blink {

const double ImageQualityController::cLowQualityTimeThreshold = 0.500; // 500 ms
const double ImageQualityController::cTimerRestartThreshold = 0.250; // 250 ms

static ImageQualityController* gImageQualityController = nullptr;

ImageQualityController* ImageQualityController::imageQualityController()
{
    if (!gImageQualityController)
        gImageQualityController = new ImageQualityController;

    return gImageQualityController;
}

void ImageQualityController::remove(LayoutObject& layoutObject)
{
    if (gImageQualityController) {
        gImageQualityController->objectDestroyed(layoutObject);
        if (gImageQualityController->isEmpty()) {
            delete gImageQualityController;
            gImageQualityController = nullptr;
        }
    }
}

bool ImageQualityController::has(const LayoutObject& layoutObject)
{
    return gImageQualityController && gImageQualityController->m_objectLayerSizeMap.contains(&layoutObject);
}

InterpolationQuality ImageQualityController::chooseInterpolationQuality(const LayoutObject& object, Image* image, const void* layer, const LayoutSize& layoutSize)
{
    if (object.style()->imageRendering() == ImageRenderingPixelated)
        return InterpolationNone;

    if (InterpolationDefault == InterpolationLow)
        return InterpolationLow;

    if (shouldPaintAtLowQuality(object, image, layer, layoutSize, object.frameView()->page()->chromeClient().lastFrameTimeMonotonic()))
        return InterpolationLow;

    // For images that are potentially animated we paint them at medium quality.
    if (image && image->maybeAnimated())
        return InterpolationMedium;

    return InterpolationDefault;
}

ImageQualityController::~ImageQualityController()
{
    // This will catch users of ImageQualityController that forget to call cleanUp.
    ASSERT(!gImageQualityController || gImageQualityController->isEmpty());
}

ImageQualityController::ImageQualityController()
    : m_timer(wrapUnique(new Timer<ImageQualityController>(this, &ImageQualityController::highQualityRepaintTimerFired)))
    , m_frameTimeWhenTimerStarted(0.0)
{
}

void ImageQualityController::setTimer(Timer<ImageQualityController>* newTimer)
{
    m_timer = wrapUnique(newTimer);
}

void ImageQualityController::removeLayer(const LayoutObject& object, LayerSizeMap* innerMap, const void* layer)
{
    if (innerMap) {
        innerMap->remove(layer);
        if (innerMap->isEmpty())
            objectDestroyed(object);
    }
}

void ImageQualityController::set(const LayoutObject& object, LayerSizeMap* innerMap, const void* layer, const LayoutSize& size, bool isResizing)
{
    if (innerMap) {
        innerMap->set(layer, size);
        m_objectLayerSizeMap.find(&object)->value.isResizing = isResizing;
    } else {
        ObjectResizeInfo newResizeInfo;
        newResizeInfo.layerSizeMap.set(layer, size);
        newResizeInfo.isResizing = isResizing;
        m_objectLayerSizeMap.set(&object, newResizeInfo);
    }
}

void ImageQualityController::objectDestroyed(const LayoutObject& object)
{
    m_objectLayerSizeMap.remove(&object);
    if (m_objectLayerSizeMap.isEmpty()) {
        m_timer->stop();
    }
}

void ImageQualityController::highQualityRepaintTimerFired(Timer<ImageQualityController>*)
{
    for (auto& i : m_objectLayerSizeMap) {
        // Only invalidate the object if it is animating.
        if (!i.value.isResizing)
            continue;

        // TODO(wangxianzhu): Use LayoutObject::getMutableForPainting().
        const_cast<LayoutObject*>(i.key)->setShouldDoFullPaintInvalidation();
        i.value.isResizing = false;
    }
    m_frameTimeWhenTimerStarted = 0.0;
}

void ImageQualityController::restartTimer(double lastFrameTimeMonotonic)
{
    if (!m_timer->isActive() || lastFrameTimeMonotonic == 0.0 || m_frameTimeWhenTimerStarted == 0.0 || (lastFrameTimeMonotonic - m_frameTimeWhenTimerStarted > cTimerRestartThreshold)) {
        m_timer->startOneShot(cLowQualityTimeThreshold, BLINK_FROM_HERE);
        m_frameTimeWhenTimerStarted = lastFrameTimeMonotonic;
    }
}

bool ImageQualityController::shouldPaintAtLowQuality(const LayoutObject& object, Image* image, const void *layer, const LayoutSize& layoutSize, double lastFrameTimeMonotonic)
{
    // If the image is not a bitmap image, then none of this is relevant and we just paint at high
    // quality.
    if (!image || !image->isBitmapImage())
        return false;

    if (!layer)
        return false;

    if (object.style()->imageRendering() == ImageRenderingOptimizeContrast)
        return true;

    if (LocalFrame* frame = object.frame()) {
        if (frame->settings() && frame->settings()->useDefaultImageInterpolationQuality())
            return false;
    }

    // Look ourselves up in the hashtables.
    ObjectLayerSizeMap::iterator i = m_objectLayerSizeMap.find(&object);
    LayerSizeMap* innerMap = nullptr;
    bool objectIsResizing = false;
    if (i != m_objectLayerSizeMap.end()) {
        innerMap = &i->value.layerSizeMap;
        objectIsResizing = i->value.isResizing;
    }
    LayoutSize oldSize;
    bool isFirstResize = true;
    if (innerMap) {
        LayerSizeMap::iterator j = innerMap->find(layer);
        if (j != innerMap->end()) {
            isFirstResize = false;
            oldSize = j->value;
        }
    }

    if (layoutSize == image->size()) {
        // There is no scale in effect. If we had a scale in effect before, we can just remove this object from the list.
        removeLayer(object, innerMap, layer);
        return false;
    }

    // If an animated resize is active for this object, paint in low quality and kick the timer ahead.
    if (objectIsResizing) {
        bool sizesChanged = oldSize != layoutSize;
        set(object, innerMap, layer, layoutSize, sizesChanged);
        if (sizesChanged)
            restartTimer(lastFrameTimeMonotonic);
        return true;
    }
    // If this is the first time resizing this image, or its size is the
    // same as the last resize, draw at high res, but record the paint
    // size and set the timer.
    if (isFirstResize || oldSize == layoutSize) {
        restartTimer(lastFrameTimeMonotonic);
        set(object, innerMap, layer, layoutSize, false);
        return false;
    }
    // If the timer is no longer active, draw at high quality and don't
    // set the timer.
    if (!m_timer->isActive()) {
        removeLayer(object, innerMap, layer);
        return false;
    }
    // This object has been resized to two different sizes while the timer
    // is active, so draw at low quality, set the flag for animated resizes and
    // the object to the list for high quality redraw.
    set(object, innerMap, layer, layoutSize, true);
    restartTimer(lastFrameTimeMonotonic);
    return true;
}

} // namespace blink
