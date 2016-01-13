// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"

namespace content {

VideoCaptureControllerID::VideoCaptureControllerID(int did)
    : device_id(did) {
}

bool VideoCaptureControllerID::operator<(
    const VideoCaptureControllerID& vc) const {
  return this->device_id < vc.device_id;
}

bool VideoCaptureControllerID::operator==(
    const VideoCaptureControllerID& vc) const {
  return this->device_id == vc.device_id;
}

}  // namespace content
