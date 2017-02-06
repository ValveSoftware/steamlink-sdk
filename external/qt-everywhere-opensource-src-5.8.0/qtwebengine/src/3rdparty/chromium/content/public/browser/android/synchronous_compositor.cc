// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/synchronous_compositor.h"

#include <utility>

#include "cc/output/compositor_frame.h"

namespace content {

SynchronousCompositor::Frame::Frame() : output_surface_id(0u) {}

SynchronousCompositor::Frame::~Frame() {}

SynchronousCompositor::Frame::Frame(Frame&& rhs)
    : output_surface_id(rhs.output_surface_id), frame(std::move(rhs.frame)) {}

SynchronousCompositor::Frame& SynchronousCompositor::Frame::operator=(
    Frame&& rhs) {
  output_surface_id = rhs.output_surface_id;
  frame = std::move(rhs.frame);
  return *this;
}

}  // namespace content
