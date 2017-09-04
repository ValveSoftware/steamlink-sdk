/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "platform/graphics/filters/FEGaussianBlur.h"

#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/text/TextStream.h"

#include "SkBlurImageFilter.h"

namespace blink {

namespace {

inline unsigned approximateBoxWidth(float s) {
  return static_cast<unsigned>(
      floorf(s * (3 / 4.f * sqrtf(twoPiFloat)) + 0.5f));
}

IntSize calculateKernelSize(const FloatSize& std) {
  DCHECK(std.width() >= 0 && std.height() >= 0);
  IntSize kernelSize;
  if (std.width()) {
    int size = std::max<unsigned>(2, approximateBoxWidth(std.width()));
    kernelSize.setWidth(size);
  }
  if (std.height()) {
    int size = std::max<unsigned>(2, approximateBoxWidth(std.height()));
    kernelSize.setHeight(size);
  }
  return kernelSize;
}

}

FEGaussianBlur::FEGaussianBlur(Filter* filter, float x, float y)
    : FilterEffect(filter), m_stdX(x), m_stdY(y) {}

FEGaussianBlur* FEGaussianBlur::create(Filter* filter, float x, float y) {
  return new FEGaussianBlur(filter, x, y);
}

FloatRect FEGaussianBlur::mapEffect(const FloatSize& stdDeviation,
                                    const FloatRect& rect) {
  IntSize kernelSize = calculateKernelSize(stdDeviation);
  // We take the half kernel size and multiply it by three, because we run box
  // blur three times.
  FloatRect result = rect;
  result.inflateX(3.0f * kernelSize.width() * 0.5f);
  result.inflateY(3.0f * kernelSize.height() * 0.5f);
  return result;
}

FloatRect FEGaussianBlur::mapEffect(const FloatRect& rect) const {
  FloatSize stdError(getFilter()->applyHorizontalScale(m_stdX),
                     getFilter()->applyVerticalScale(m_stdY));
  return mapEffect(stdError, rect);
}

sk_sp<SkImageFilter> FEGaussianBlur::createImageFilter() {
  sk_sp<SkImageFilter> input(
      SkiaImageFilterBuilder::build(inputEffect(0), operatingColorSpace()));
  float stdX = getFilter()->applyHorizontalScale(m_stdX);
  float stdY = getFilter()->applyVerticalScale(m_stdY);
  SkImageFilter::CropRect rect = getCropRect();
  return SkBlurImageFilter::Make(SkFloatToScalar(stdX), SkFloatToScalar(stdY),
                                 std::move(input), &rect);
}

TextStream& FEGaussianBlur::externalRepresentation(TextStream& ts,
                                                   int indent) const {
  writeIndent(ts, indent);
  ts << "[feGaussianBlur";
  FilterEffect::externalRepresentation(ts);
  ts << " stdDeviation=\"" << m_stdX << ", " << m_stdY << "\"]\n";
  inputEffect(0)->externalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink
