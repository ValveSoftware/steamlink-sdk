// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_encode_accelerator.h"

namespace media {

VideoEncodeAccelerator::~VideoEncodeAccelerator() {}

}  // namespace media

namespace base {

void DefaultDeleter<media::VideoEncodeAccelerator>::operator()(
    void* video_encode_accelerator) const {
  static_cast<media::VideoEncodeAccelerator*>(video_encode_accelerator)->
      Destroy();
}

}  // namespace base

