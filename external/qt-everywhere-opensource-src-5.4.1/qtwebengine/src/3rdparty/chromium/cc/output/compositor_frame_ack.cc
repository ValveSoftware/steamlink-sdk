// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/compositor_frame_ack.h"

namespace cc {

CompositorFrameAck::CompositorFrameAck()
    : last_software_frame_id(0) {}

CompositorFrameAck::~CompositorFrameAck() {}

}  // namespace cc
