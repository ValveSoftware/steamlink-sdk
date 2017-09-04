// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/image_transport_surface_delegate.h"

namespace gpu {

SwapBuffersCompleteParams::SwapBuffersCompleteParams() = default;

SwapBuffersCompleteParams::SwapBuffersCompleteParams(
    SwapBuffersCompleteParams&& other) = default;

SwapBuffersCompleteParams::~SwapBuffersCompleteParams() = default;

SwapBuffersCompleteParams& SwapBuffersCompleteParams::operator=(
    SwapBuffersCompleteParams&& other) = default;

}  // namespace gpu
