// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/HTMLCanvasElementCapture.h"

#include "core/dom/ExceptionCode.h"
#include "core/html/HTMLCanvasElement.h"
#include "modules/mediacapturefromelement/CanvasCaptureMediaStreamTrack.h"
#include "modules/mediastream/MediaStream.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCanvasCaptureHandler.h"
#include "public/platform/WebMediaStream.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace {
const double kDefaultFrameRate = 60.0;
} // anonymous namespace

namespace blink {

MediaStream* HTMLCanvasElementCapture::captureStream(HTMLCanvasElement& element, ExceptionState& exceptionState)
{
    return HTMLCanvasElementCapture::captureStream(element, false, 0, exceptionState);
}

MediaStream* HTMLCanvasElementCapture::captureStream(HTMLCanvasElement& element, double frameRate, ExceptionState& exceptionState)
{
    if (frameRate < 0.0) {
        exceptionState.throwDOMException(NotSupportedError, "Given frame rate is not supported.");
        return nullptr;
    }

    return HTMLCanvasElementCapture::captureStream(element, true, frameRate, exceptionState);
}

MediaStream* HTMLCanvasElementCapture::captureStream(HTMLCanvasElement& element, bool givenFrameRate, double frameRate, ExceptionState& exceptionState)
{
    if (!element.originClean()) {
        exceptionState.throwSecurityError("Canvas is not origin-clean.");
        return nullptr;
    }

    WebMediaStreamTrack track;
    const WebSize size(element.width(), element.height());
    std::unique_ptr<WebCanvasCaptureHandler> handler;
    if (givenFrameRate)
        handler = wrapUnique(Platform::current()->createCanvasCaptureHandler(size, frameRate, &track));
    else
        handler = wrapUnique(Platform::current()->createCanvasCaptureHandler(size, kDefaultFrameRate, &track));

    if (!handler) {
        exceptionState.throwDOMException(NotSupportedError, "No CanvasCapture handler can be created.");
        return nullptr;
    }

    CanvasCaptureMediaStreamTrack* canvasTrack;
    if (givenFrameRate)
        canvasTrack = CanvasCaptureMediaStreamTrack::create(track, &element, std::move(handler), frameRate);
    else
        canvasTrack = CanvasCaptureMediaStreamTrack::create(track, &element, std::move(handler));
    // We want to capture a frame in the beginning.
    canvasTrack->requestFrame();

    MediaStreamTrackVector tracks;
    tracks.append(canvasTrack);
    return MediaStream::create(element.getExecutionContext(), tracks);
}

} // namespace blink
