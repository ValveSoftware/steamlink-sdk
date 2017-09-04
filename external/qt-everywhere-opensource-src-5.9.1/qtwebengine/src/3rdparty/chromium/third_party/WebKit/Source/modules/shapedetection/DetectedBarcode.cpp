// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedBarcode.h"

#include "core/dom/DOMRect.h"

namespace blink {

DetectedBarcode* DetectedBarcode::create() {
  return new DetectedBarcode(emptyString(), DOMRect::create(0, 0, 0, 0));
}

DetectedBarcode* DetectedBarcode::create(String rawValue,
                                         DOMRect* boundingBox) {
  return new DetectedBarcode(rawValue, boundingBox);
}

const String& DetectedBarcode::rawValue() const {
  return m_rawValue;
}

DOMRect* DetectedBarcode::boundingBox() const {
  return m_boundingBox.get();
}

DetectedBarcode::DetectedBarcode(String rawValue, DOMRect* boundingBox)
    : m_rawValue(rawValue), m_boundingBox(boundingBox) {}

DEFINE_TRACE(DetectedBarcode) {
  visitor->trace(m_boundingBox);
}

}  // namespace blink
