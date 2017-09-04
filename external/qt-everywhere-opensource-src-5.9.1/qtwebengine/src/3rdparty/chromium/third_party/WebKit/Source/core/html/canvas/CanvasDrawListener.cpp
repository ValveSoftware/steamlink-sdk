// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/canvas/CanvasDrawListener.h"

#include "third_party/skia/include/core/SkImage.h"
#include <memory>

namespace blink {

CanvasDrawListener::~CanvasDrawListener() {}

void CanvasDrawListener::sendNewFrame(sk_sp<SkImage> image) {
  m_handler->sendNewFrame(image.get());
}

bool CanvasDrawListener::needsNewFrame() const {
  return m_frameCaptureRequested && m_handler->needsNewFrame();
}

void CanvasDrawListener::requestFrame() {
  m_frameCaptureRequested = true;
}

CanvasDrawListener::CanvasDrawListener(
    std::unique_ptr<WebCanvasCaptureHandler> handler)
    : m_frameCaptureRequested(true), m_handler(std::move(handler)) {}

}  // namespace blink
