// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_options.h"

#include "base/logging.h"

namespace content {

const char kMediaStreamSourceTab[] = "tab";
const char kMediaStreamSourceScreen[] = "screen";
const char kMediaStreamSourceDesktop[] = "desktop";
const char kMediaStreamSourceSystem[] = "system";

TrackControls::TrackControls()
    : requested(false) {}

TrackControls::TrackControls(bool request)
    : requested(request) {}

TrackControls::TrackControls(const TrackControls& other) = default;

TrackControls::~TrackControls() {}

StreamControls::StreamControls()
    : audio(false), video(false), hotword_enabled(false) {}

StreamControls::StreamControls(bool request_audio, bool request_video)
    : audio(request_audio), video(request_video), hotword_enabled(false) {}

StreamControls::~StreamControls() {}

// static
const int StreamDeviceInfo::kNoId = -1;

StreamDeviceInfo::StreamDeviceInfo()
    : session_id(kNoId) {}

StreamDeviceInfo::StreamDeviceInfo(MediaStreamType service_param,
                                   const std::string& name_param,
                                   const std::string& device_param)
    : device(service_param, device_param, name_param),
      session_id(kNoId) {
}

StreamDeviceInfo::StreamDeviceInfo(MediaStreamType service_param,
                                   const std::string& name_param,
                                   const std::string& device_param,
                                   int sample_rate,
                                   int channel_layout,
                                   int frames_per_buffer)
    : device(service_param, device_param, name_param, sample_rate,
             channel_layout, frames_per_buffer),
      session_id(kNoId) {
}

// static
bool StreamDeviceInfo::IsEqual(const StreamDeviceInfo& first,
                               const StreamDeviceInfo& second) {
  return first.device.IsEqual(second.device) &&
      first.session_id == second.session_id;
}

}  // namespace content
