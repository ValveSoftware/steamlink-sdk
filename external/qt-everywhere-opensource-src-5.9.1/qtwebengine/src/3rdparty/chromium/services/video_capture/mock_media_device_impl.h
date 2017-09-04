// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_MEDIA_DEVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_MEDIA_DEVICE_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/mock_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockMediaDeviceImpl : public mojom::MockMediaDevice {
 public:
  MockMediaDeviceImpl(mojom::MockMediaDeviceRequest request);
  ~MockMediaDeviceImpl() override;

  // Use forwarding method to work around gmock not supporting move-only types.
  void AllocateAndStart(mojom::MockDeviceClientPtr client) override;

  MOCK_METHOD1(AllocateAndStartPtr, void(mojom::MockDeviceClientPtr*));
  MOCK_METHOD0(StopAndDeAllocate, void());

 private:
  mojo::Binding<mojom::MockMediaDevice> binding_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_MEDIA_DEVICE_IMPL_H_
