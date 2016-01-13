// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame_metadata.h"

namespace cc {

CompositorFrameMetadata::CompositorFrameMetadata()
    : device_scale_factor(0.f),
      page_scale_factor(0.f),
      min_page_scale_factor(0.f),
      max_page_scale_factor(0.f),
      overdraw_bottom_height(0.f) {
}

CompositorFrameMetadata::~CompositorFrameMetadata() {
}

}  // namespace cc
