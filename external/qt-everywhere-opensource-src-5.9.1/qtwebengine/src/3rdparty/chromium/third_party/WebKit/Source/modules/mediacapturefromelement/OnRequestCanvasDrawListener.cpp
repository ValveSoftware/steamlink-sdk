// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediacapturefromelement/OnRequestCanvasDrawListener.h"

#include "third_party/skia/include/core/SkImage.h"
#include <memory>

namespace blink {

OnRequestCanvasDrawListener::OnRequestCanvasDrawListener(
    std::unique_ptr<WebCanvasCaptureHandler> handler)
    : CanvasDrawListener(std::move(handler)) {}

OnRequestCanvasDrawListener::~OnRequestCanvasDrawListener() {}

// static
OnRequestCanvasDrawListener* OnRequestCanvasDrawListener::create(
    std::unique_ptr<WebCanvasCaptureHandler> handler) {
  return new OnRequestCanvasDrawListener(std::move(handler));
}

void OnRequestCanvasDrawListener::sendNewFrame(sk_sp<SkImage> image) {
  m_frameCaptureRequested = false;
  CanvasDrawListener::sendNewFrame(std::move(image));
}

}  // namespace blink
