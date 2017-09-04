// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_media_device_impl.h"

namespace video_capture {

MockMediaDeviceImpl::MockMediaDeviceImpl(mojom::MockMediaDeviceRequest request)
    : binding_(this, std::move(request)) {}

MockMediaDeviceImpl::~MockMediaDeviceImpl() = default;

void MockMediaDeviceImpl::AllocateAndStart(mojom::MockDeviceClientPtr client) {
  AllocateAndStartPtr(&client);
}

}  // namespace video_capture
