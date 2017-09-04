// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/filters/FEBoxReflect.h"

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "wtf/Assertions.h"

namespace blink {

FEBoxReflect::FEBoxReflect(Filter* filter, const BoxReflection& reflection)
    : FilterEffect(filter), m_reflection(reflection) {}

FEBoxReflect::~FEBoxReflect() {}

FloatRect FEBoxReflect::mapEffect(const FloatRect& rect) const {
  return m_reflection.mapRect(rect);
}

TextStream& FEBoxReflect::externalRepresentation(TextStream& ts,
                                                 int indent) const {
  // Only called for SVG layout tree printing.
  ASSERT_NOT_REACHED();
  return ts;
}

sk_sp<SkImageFilter> FEBoxReflect::createImageFilter() {
  return SkiaImageFilterBuilder::buildBoxReflectFilter(
      m_reflection,
      SkiaImageFilterBuilder::build(inputEffect(0), operatingColorSpace()));
}

}  // namespace blink
