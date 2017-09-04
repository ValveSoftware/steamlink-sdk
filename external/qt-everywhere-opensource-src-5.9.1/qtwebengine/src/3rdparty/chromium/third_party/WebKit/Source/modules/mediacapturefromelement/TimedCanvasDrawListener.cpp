// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/TimedCanvasDrawListener.h"

#include "third_party/skia/include/core/SkImage.h"
#include <memory>

namespace blink {

TimedCanvasDrawListener::TimedCanvasDrawListener(
    std::unique_ptr<WebCanvasCaptureHandler> handler,
    double frameRate)
    : CanvasDrawListener(std::move(handler)),
      m_frameInterval(1 / frameRate),
      m_requestFrameTimer(this,
                          &TimedCanvasDrawListener::requestFrameTimerFired) {}

TimedCanvasDrawListener::~TimedCanvasDrawListener() {}

// static
TimedCanvasDrawListener* TimedCanvasDrawListener::create(
    std::unique_ptr<WebCanvasCaptureHandler> handler,
    double frameRate) {
  TimedCanvasDrawListener* listener =
      new TimedCanvasDrawListener(std::move(handler), frameRate);
  listener->m_requestFrameTimer.startRepeating(listener->m_frameInterval,
                                               BLINK_FROM_HERE);
  return listener;
}

void TimedCanvasDrawListener::sendNewFrame(sk_sp<SkImage> image) {
  m_frameCaptureRequested = false;
  CanvasDrawListener::sendNewFrame(std::move(image));
}

void TimedCanvasDrawListener::requestFrameTimerFired(TimerBase*) {
  // TODO(emircan): Measure the jitter and log, see crbug.com/589974.
  m_frameCaptureRequested = true;
}

}  // namespace blink
