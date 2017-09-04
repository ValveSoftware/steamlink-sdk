// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleImageValue.h"

namespace blink {

double CSSStyleImageValue::intrinsicWidth(bool& isNull) const {
  isNull = isCachePending();
  if (isNull)
    return 0;
  return imageLayoutSize().width().toDouble();
}

double CSSStyleImageValue::intrinsicHeight(bool& isNull) const {
  isNull = isCachePending();
  if (isNull)
    return 0;
  return imageLayoutSize().height().toDouble();
}

double CSSStyleImageValue::intrinsicRatio(bool& isNull) {
  isNull = isCachePending();
  if (isNull || intrinsicHeight(isNull) == 0) {
    isNull = true;
    return 0;
  }
  return intrinsicWidth(isNull) / intrinsicHeight(isNull);
}

FloatSize CSSStyleImageValue::elementSize(
    const FloatSize& defaultObjectSize) const {
  bool notUsed;
  return FloatSize(intrinsicWidth(notUsed), intrinsicHeight(notUsed));
}

bool CSSStyleImageValue::isAccelerated() const {
  return image() && image()->isTextureBacked();
}

int CSSStyleImageValue::sourceHeight() {
  bool notUsed;
  return intrinsicHeight(notUsed);
}

int CSSStyleImageValue::sourceWidth() {
  bool notUsed;
  return intrinsicWidth(notUsed);
}

PassRefPtr<Image> CSSStyleImageValue::image() const {
  if (isCachePending())
    return nullptr;
  // cachedImage can be null if image is StyleInvalidImage
  ImageResource* cachedImage = m_imageValue->cachedImage()->cachedImage();
  if (cachedImage) {
    // getImage() returns the nullImage() if the image is not available yet
    return cachedImage->getImage()->imageForDefaultFrame();
  }
  return nullptr;
}

}  // namespace blink
