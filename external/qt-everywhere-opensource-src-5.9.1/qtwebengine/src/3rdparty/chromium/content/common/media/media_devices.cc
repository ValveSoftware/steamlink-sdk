// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_devices.h"

namespace content {

MediaDeviceInfo::MediaDeviceInfo(const std::string& device_id,
                                 const std::string& label,
                                 const std::string& group_id)
    : device_id(device_id), label(label), group_id(group_id) {}

bool operator==(const MediaDeviceInfo& first, const MediaDeviceInfo& second) {
  return first.device_id == second.device_id && first.label == second.label;
}

}  // namespace content
