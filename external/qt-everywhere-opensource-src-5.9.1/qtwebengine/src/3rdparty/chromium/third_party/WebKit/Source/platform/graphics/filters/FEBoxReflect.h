// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FEBoxReflect_h
#define FEBoxReflect_h

#include "platform/PlatformExport.h"
#include "platform/graphics/BoxReflection.h"
#include "platform/graphics/filters/FilterEffect.h"

namespace blink {

// Used to implement the -webkit-box-reflect property as a filter.
class PLATFORM_EXPORT FEBoxReflect final : public FilterEffect {
 public:
  static FEBoxReflect* create(Filter* filter, const BoxReflection& reflection) {
    return new FEBoxReflect(filter, reflection);
  }

  // FilterEffect implementation
  TextStream& externalRepresentation(TextStream&, int indentation) const final;

 private:
  FEBoxReflect(Filter*, const BoxReflection&);
  ~FEBoxReflect() final;

  FloatRect mapEffect(const FloatRect&) const final;

  sk_sp<SkImageFilter> createImageFilter() final;

  BoxReflection m_reflection;
};

}  // namespace blink

#endif  // FEBoxReflect_h
