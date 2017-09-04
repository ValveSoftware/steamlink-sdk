// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/BarcodeDetector.h"

#include "core/dom/DOMException.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/html/canvas/CanvasImageSource.h"

namespace blink {

BarcodeDetector* BarcodeDetector::create(ScriptState* scriptState) {
  return new BarcodeDetector(*scriptState->domWindow()->frame());
}

BarcodeDetector::BarcodeDetector(LocalFrame& frame) : ShapeDetector(frame) {}

ScriptPromise BarcodeDetector::detect(
    ScriptState* scriptState,
    const CanvasImageSourceUnion& imageSource) {
  return detectShapes(scriptState, ShapeDetector::DetectorType::Barcode,
                      imageSource);
}

DEFINE_TRACE(BarcodeDetector) {
  ShapeDetector::trace(visitor);
}

}  // namespace blink
