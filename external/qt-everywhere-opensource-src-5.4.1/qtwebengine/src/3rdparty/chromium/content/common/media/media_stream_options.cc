// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/media_stream_options.h"

#include "base/logging.h"

namespace content {

const char kMediaStreamSource[] = "chromeMediaSource";
const char kMediaStreamSourceId[] = "chromeMediaSourceId";
const char kMediaStreamSourceInfoId[] = "sourceId";
const char kMediaStreamSourceTab[] = "tab";
const char kMediaStreamSourceScreen[] = "screen";
const char kMediaStreamSourceDesktop[] = "desktop";
const char kMediaStreamSourceSystem[] = "system";
const char kMediaStreamRenderToAssociatedSink[] =
    "chromeRenderToAssociatedSink";
// The prefix of this constant is 'goog' to match with other getUserMedia
// constraints for audio.
const char kMediaStreamAudioDucking[] = "googDucking";

namespace {

bool GetFirstConstraintByName(const StreamOptions::Constraints& constraints,
                              const std::string& name,
                              std::string* value) {
  for (StreamOptions::Constraints::const_iterator it = constraints.begin();
      it != constraints.end(); ++it ) {
    if (it->name == name) {
      *value = it->value;
      return true;
    }
  }
  return false;
}

bool GetFirstConstraintByName(const StreamOptions::Constraints& mandatory,
                              const StreamOptions::Constraints& optional,
                              const std::string& name,
                              std::string* value,
                              bool* is_mandatory) {
  if (GetFirstConstraintByName(mandatory, name, value)) {
      if (is_mandatory)
        *is_mandatory = true;
      return true;
    }
    if (is_mandatory)
      *is_mandatory = false;
    return GetFirstConstraintByName(optional, name, value);
}

} // namespace

StreamOptions::StreamOptions()
    : audio_requested(false),
      video_requested(false) {}

StreamOptions::StreamOptions(bool request_audio, bool request_video)
    :  audio_requested(request_audio), video_requested(request_video) {
}

StreamOptions::~StreamOptions() {}

StreamOptions::Constraint::Constraint() {}

StreamOptions::Constraint::Constraint(const std::string& name,
                                      const std::string& value)
    : name(name), value(value) {
}

bool StreamOptions::GetFirstAudioConstraintByName(const std::string& name,
                                                  std::string* value,
                                                  bool* is_mandatory) const {
  return GetFirstConstraintByName(mandatory_audio, optional_audio, name, value,
                                  is_mandatory);
}

bool StreamOptions::GetFirstVideoConstraintByName(const std::string& name,
                                                  std::string* value,
                                                  bool* is_mandatory) const {
  return GetFirstConstraintByName(mandatory_video, optional_video, name, value,
                                  is_mandatory);
}

// static
void StreamOptions::GetConstraintsByName(
    const StreamOptions::Constraints& constraints,
    const std::string& name,
    std::vector<std::string>* values) {
  for (StreamOptions::Constraints::const_iterator it = constraints.begin();
      it != constraints.end(); ++it ) {
    if (it->name == name)
      values->push_back(it->value);
  }
}

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
