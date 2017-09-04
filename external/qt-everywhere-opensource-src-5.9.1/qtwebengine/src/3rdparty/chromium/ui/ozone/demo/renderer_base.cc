// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/renderer_base.h"

#include <cmath>

namespace ui {

namespace {
const int kAnimationSteps = 240;
}  // namespace

RendererBase::RendererBase(gfx::AcceleratedWidget widget, const gfx::Size& size)
    : widget_(widget), size_(size) {
}

RendererBase::~RendererBase() {
}

float RendererBase::NextFraction() {
  float fraction = (sinf(iteration_ * 2 * M_PI / kAnimationSteps) + 1) / 2;

  iteration_++;
  iteration_ %= kAnimationSteps;

  return fraction;
}

}  // namespace ui
