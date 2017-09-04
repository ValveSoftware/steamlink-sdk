// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame.h"

namespace cc {

CompositorFrame::CompositorFrame() {}

CompositorFrame::CompositorFrame(CompositorFrame&& other) = default;

CompositorFrame::~CompositorFrame() {}

CompositorFrame& CompositorFrame::operator=(CompositorFrame&& other) = default;

}  // namespace cc
