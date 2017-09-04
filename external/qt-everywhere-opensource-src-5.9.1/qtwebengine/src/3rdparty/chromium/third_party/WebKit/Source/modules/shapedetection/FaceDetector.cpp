// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/FaceDetector.h"

#include "core/dom/DOMException.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/canvas/CanvasImageSource.h"

namespace blink {

FaceDetector* FaceDetector::create(ScriptState* scriptState) {
  return new FaceDetector(*scriptState->domWindow()->frame());
}

FaceDetector::FaceDetector(LocalFrame& frame) : ShapeDetector(frame) {}

ScriptPromise FaceDetector::detect(ScriptState* scriptState,
                                   const CanvasImageSourceUnion& imageSource) {
  return detectShapes(scriptState, ShapeDetector::DetectorType::Face,
                      imageSource);
}

DEFINE_TRACE(FaceDetector) {
  ShapeDetector::trace(visitor);
}

}  // namespace blink
