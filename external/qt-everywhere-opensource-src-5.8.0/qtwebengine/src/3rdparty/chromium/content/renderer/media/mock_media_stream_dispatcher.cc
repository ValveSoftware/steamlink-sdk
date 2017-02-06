// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dispatcher.h"

#include "base/strings/string_number_conversions.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

// Used for ID for output devices and for matching output device ID for input
// devices.
const char kAudioOutputDeviceIdPrefix[] = "audio_output_device_id";

namespace content {

MockMediaStreamDispatcher::MockMediaStreamDispatcher()
    : MediaStreamDispatcher(NULL),
      audio_input_request_id_(-1),
      audio_output_request_id_(-1),
      video_request_id_(-1),
      request_stream_counter_(0),
      stop_audio_device_counter_(0),
      stop_video_device_counter_(0),
      session_id_(0),
      test_same_id_(false) {}

MockMediaStreamDispatcher::~MockMediaStreamDispatcher() {}

void MockMediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const StreamControls& controls,
    const url::Origin& url) {
  // Audio and video share the same request so we use |audio_input_request_id_|
  // only.
  audio_input_request_id_ = request_id;

  stream_label_ = "local_stream" + base::IntToString(request_id);
  audio_input_array_.clear();
  video_array_.clear();

  if (controls.audio.requested) {
    AddAudioInputDeviceToArray(false);
  }
  if (controls.video.requested) {
    AddVideoDeviceToArray(true);
  }
  ++request_stream_counter_;
}

void MockMediaStreamDispatcher::CancelGenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  EXPECT_EQ(request_id, audio_input_request_id_);
}

void MockMediaStreamDispatcher::EnumerateDevices(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    MediaStreamType type,
    const url::Origin& security_origin) {
  if (type == MEDIA_DEVICE_AUDIO_CAPTURE) {
    audio_input_request_id_ = request_id;
    audio_input_array_.clear();
    AddAudioInputDeviceToArray(true);
    AddAudioInputDeviceToArray(false);
  } else if (type == MEDIA_DEVICE_AUDIO_OUTPUT) {
    audio_output_request_id_ = request_id;
    audio_output_array_.clear();
    AddAudioOutputDeviceToArray();
  } else if (type == MEDIA_DEVICE_VIDEO_CAPTURE) {
    video_request_id_ = request_id;
    video_array_.clear();
    AddVideoDeviceToArray(true);
    AddVideoDeviceToArray(false);
  }
}

void MockMediaStreamDispatcher::StopStreamDevice(
    const StreamDeviceInfo& device_info) {
  if (IsAudioInputMediaType(device_info.device.type)) {
    ++stop_audio_device_counter_;
    return;
  }
  if (IsVideoMediaType(device_info.device.type)) {
    ++stop_video_device_counter_;
    return;
  }
  NOTREACHED();
}

bool MockMediaStreamDispatcher::IsStream(const std::string& label) {
  return true;
}

int MockMediaStreamDispatcher::video_session_id(const std::string& label,
                                                int index) {
  return -1;
}

int MockMediaStreamDispatcher::audio_session_id(const std::string& label,
                                                int index) {
  return -1;
}

void MockMediaStreamDispatcher::AddAudioInputDeviceToArray(
    bool matched_output) {
  StreamDeviceInfo audio;
  audio.device.id = test_same_id_ ? "test_id" : "audio_input_device_id";
  audio.device.id = audio.device.id + base::IntToString(session_id_);
  audio.device.name = "microphone";
  audio.device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  audio.device.video_facing = MEDIA_VIDEO_FACING_NONE;
  if (matched_output) {
    audio.device.matched_output_device_id =
        kAudioOutputDeviceIdPrefix + base::IntToString(session_id_);
  }
  audio.session_id = session_id_;
  audio.device.input.sample_rate = media::AudioParameters::kAudioCDSampleRate;
  audio.device.input.channel_layout = media::CHANNEL_LAYOUT_STEREO;
  audio.device.input.frames_per_buffer = audio.device.input.sample_rate / 100;
  audio_input_array_.push_back(audio);
}

void MockMediaStreamDispatcher::AddAudioOutputDeviceToArray() {
  StreamDeviceInfo audio;
  audio.device.id = kAudioOutputDeviceIdPrefix + base::IntToString(session_id_);
  audio.device.name = "speaker";
  audio.device.type = MEDIA_DEVICE_AUDIO_OUTPUT;
  audio.device.video_facing = MEDIA_VIDEO_FACING_NONE;
  audio.session_id = session_id_;
  audio_output_array_.push_back(audio);
}

void MockMediaStreamDispatcher::AddVideoDeviceToArray(bool facing_user) {
  StreamDeviceInfo video;
  video.device.id = test_same_id_ ? "test_id" : "video_device_id";
  video.device.id = video.device.id + base::IntToString(session_id_);
  video.device.name = "usb video camera";
  video.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video.device.video_facing = facing_user ? MEDIA_VIDEO_FACING_USER
                                          : MEDIA_VIDEO_FACING_ENVIRONMENT;
  video.session_id = session_id_;
  video_array_.push_back(video);
}

}  // namespace content
