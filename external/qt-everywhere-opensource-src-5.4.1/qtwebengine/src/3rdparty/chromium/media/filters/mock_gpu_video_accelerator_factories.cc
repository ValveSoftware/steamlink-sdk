// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/mock_gpu_video_accelerator_factories.h"

namespace media {

MockGpuVideoAcceleratorFactories::MockGpuVideoAcceleratorFactories() {}

MockGpuVideoAcceleratorFactories::~MockGpuVideoAcceleratorFactories() {}

scoped_ptr<VideoDecodeAccelerator>
MockGpuVideoAcceleratorFactories::CreateVideoDecodeAccelerator() {
  return scoped_ptr<VideoDecodeAccelerator>(DoCreateVideoDecodeAccelerator());
}

scoped_ptr<VideoEncodeAccelerator>
MockGpuVideoAcceleratorFactories::CreateVideoEncodeAccelerator() {
  return scoped_ptr<VideoEncodeAccelerator>(DoCreateVideoEncodeAccelerator());
}

}  // namespace media
