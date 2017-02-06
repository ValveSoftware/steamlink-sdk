// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_stream_request.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace content {

bool IsAudioInputMediaType(MediaStreamType type) {
  return (type == MEDIA_DEVICE_AUDIO_CAPTURE ||
          type == content::MEDIA_TAB_AUDIO_CAPTURE ||
          type == content::MEDIA_DESKTOP_AUDIO_CAPTURE);
}

bool IsVideoMediaType(MediaStreamType type) {
  return (type == MEDIA_DEVICE_VIDEO_CAPTURE ||
          type == content::MEDIA_TAB_VIDEO_CAPTURE ||
          type == content::MEDIA_DESKTOP_VIDEO_CAPTURE);
}

MediaStreamDevice::MediaStreamDevice()
    : type(MEDIA_NO_SERVICE),
      video_facing(MEDIA_VIDEO_FACING_NONE) {
}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamType type,
    const std::string& id,
    const std::string& name)
    : type(type),
      id(id),
      video_facing(MEDIA_VIDEO_FACING_NONE),
      name(name) {
#if defined(OS_ANDROID)
  if (name.find("front") != std::string::npos) {
    video_facing = MEDIA_VIDEO_FACING_USER;
  } else if (name.find("back") != std::string::npos) {
    video_facing = MEDIA_VIDEO_FACING_ENVIRONMENT;
  }
#endif
}

MediaStreamDevice::MediaStreamDevice(
    MediaStreamType type,
    const std::string& id,
    const std::string& name,
    int sample_rate,
    int channel_layout,
    int frames_per_buffer)
    : type(type),
      id(id),
      video_facing(MEDIA_VIDEO_FACING_NONE),
      name(name),
      input(sample_rate, channel_layout, frames_per_buffer) {
}

MediaStreamDevice::MediaStreamDevice(const MediaStreamDevice& other) = default;

MediaStreamDevice::~MediaStreamDevice() {}

bool MediaStreamDevice::IsEqual(const MediaStreamDevice& second) const {
  const AudioDeviceParameters& input_second = second.input;
  return type == second.type &&
      name == second.name &&
      id == second.id &&
      input.sample_rate == input_second.sample_rate &&
      input.channel_layout == input_second.channel_layout;
}

MediaStreamDevices::MediaStreamDevices() {}

MediaStreamDevices::MediaStreamDevices(size_t count,
                                       const MediaStreamDevice& value)
    : std::vector<MediaStreamDevice>(count, value) {
}

const MediaStreamDevice* MediaStreamDevices::FindById(
    const std::string& device_id) const {
  for (const_iterator iter = begin(); iter != end(); ++iter) {
    if (iter->id == device_id)
      return &(*iter);
  }
  return NULL;
}

MediaStreamDevice::AudioDeviceParameters::AudioDeviceParameters()
    : sample_rate(), channel_layout(), frames_per_buffer(), effects() {}

MediaStreamDevice::AudioDeviceParameters::AudioDeviceParameters(
    int sample_rate,
    int channel_layout,
    int frames_per_buffer)
    : sample_rate(sample_rate),
      channel_layout(channel_layout),
      frames_per_buffer(frames_per_buffer),
      effects() {}

MediaStreamDevice::AudioDeviceParameters::AudioDeviceParameters(
    const AudioDeviceParameters& other) = default;

MediaStreamDevice::AudioDeviceParameters::~AudioDeviceParameters() {}

MediaStreamRequest::MediaStreamRequest(
    int render_process_id,
    int render_frame_id,
    int page_request_id,
    const GURL& security_origin,
    bool user_gesture,
    MediaStreamRequestType request_type,
    const std::string& requested_audio_device_id,
    const std::string& requested_video_device_id,
    MediaStreamType audio_type,
    MediaStreamType video_type)
    : render_process_id(render_process_id),
      render_frame_id(render_frame_id),
      page_request_id(page_request_id),
      security_origin(security_origin),
      user_gesture(user_gesture),
      request_type(request_type),
      requested_audio_device_id(requested_audio_device_id),
      requested_video_device_id(requested_video_device_id),
      audio_type(audio_type),
      video_type(video_type),
      all_ancestors_have_same_origin(false) {
}

MediaStreamRequest::MediaStreamRequest(const MediaStreamRequest& other) =
    default;

MediaStreamRequest::~MediaStreamRequest() {}

}  // namespace content
