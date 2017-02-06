// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/audio/audio_device_description.h"
#include "media/base/media_resources.h"

namespace media {
const char AudioDeviceDescription::kDefaultDeviceId[] = "default";
const char AudioDeviceDescription::kCommunicationsDeviceId[] = "communications";
const char AudioDeviceDescription::kLoopbackInputDeviceId[] = "loopback";

// static
bool AudioDeviceDescription::IsDefaultDevice(const std::string& device_id) {
  return device_id.empty() ||
         device_id == AudioDeviceDescription::kDefaultDeviceId;
}

// static
bool AudioDeviceDescription::UseSessionIdToSelectDevice(
    int session_id,
    const std::string& device_id) {
  return session_id && device_id.empty();
}

// static
std::string AudioDeviceDescription::GetDefaultDeviceName() {
#if !defined(OS_IOS)
  return GetLocalizedStringUTF8(DEFAULT_AUDIO_DEVICE_NAME);
#else
  NOTREACHED();
  return "";
#endif
}

// static
std::string AudioDeviceDescription::GetCommunicationsDeviceName() {
#if defined(OS_WIN)
  return GetLocalizedStringUTF8(COMMUNICATIONS_AUDIO_DEVICE_NAME);
#else
  NOTREACHED();
  return "";
#endif
}

}  // namespace media
