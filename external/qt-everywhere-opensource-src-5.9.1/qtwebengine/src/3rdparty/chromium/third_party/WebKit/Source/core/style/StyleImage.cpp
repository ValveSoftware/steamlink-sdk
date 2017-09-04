// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleImage.h"

#include "core/svg/graphics/SVGImage.h"
#include "core/svg/graphics/SVGImageForContainer.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

LayoutSize StyleImage::applyZoom(const LayoutSize& size, float multiplier) {
  if (multiplier == 1.0f)
    return size;

  LayoutUnit width(size.width() * multiplier);
  LayoutUnit height(size.height() * multiplier);

  // Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
  if (size.width() > LayoutUnit())
    width = std::max(LayoutUnit(1), width);

  if (size.height() > LayoutUnit())
    height = std::max(LayoutUnit(1), height);

  return LayoutSize(width, height);
}

LayoutSize StyleImage::imageSizeForSVGImage(
    SVGImage* svgImage,
    float multiplier,
    const LayoutSize& defaultObjectSize) const {
  FloatSize unzoomedDefaultObjectSize(defaultObjectSize);
  unzoomedDefaultObjectSize.scale(1 / multiplier);
  LayoutSize imageSize(
      roundedIntSize(svgImage->concreteObjectSize(unzoomedDefaultObjectSize)));
  return applyZoom(imageSize, multiplier);
}

}  // namespace blink
