// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/output_surface_frame.h"

namespace cc {

OutputSurfaceFrame::OutputSurfaceFrame() = default;

OutputSurfaceFrame::OutputSurfaceFrame(OutputSurfaceFrame&& other) = default;

OutputSurfaceFrame::~OutputSurfaceFrame() = default;

OutputSurfaceFrame& OutputSurfaceFrame::operator=(OutputSurfaceFrame&& other) =
    default;

}  // namespace cc
