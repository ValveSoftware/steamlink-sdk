// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dispatcher.h"

#include "base/strings/string_number_conversions.h"
#include "content/public/common/media_stream_request.h"
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
      session_id_(0) {
}

MockMediaStreamDispatcher::~MockMediaStreamDispatcher() {}

void MockMediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const StreamOptions& components,
    const GURL& url) {
  // Audio and video share the same request so we use |audio_input_request_id_|
  // only.
  audio_input_request_id_ = request_id;

  stream_label_ = "local_stream" + base::IntToString(request_id);
  audio_input_array_.clear();
  video_array_.clear();

  if (components.audio_requested) {
    AddAudioInputDeviceToArray(false);
  }
  if (components.video_requested) {
    AddVideoDeviceToArray();
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
    const GURL& security_origin,
    bool hide_labels_if_no_access) {
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
    AddVideoDeviceToArray();
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
  audio.device.id = "audio_input_device_id" + base::IntToString(session_id_);
  audio.device.name = "microphone";
  audio.device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  if (matched_output) {
    audio.device.matched_output_device_id =
        kAudioOutputDeviceIdPrefix + base::IntToString(session_id_);
  }
  audio.session_id = session_id_;
  audio_input_array_.push_back(audio);
}

void MockMediaStreamDispatcher::AddAudioOutputDeviceToArray() {
  StreamDeviceInfo audio;
  audio.device.id = kAudioOutputDeviceIdPrefix + base::IntToString(session_id_);
  audio.device.name = "speaker";
  audio.device.type = MEDIA_DEVICE_AUDIO_OUTPUT;
  audio.session_id = session_id_;
  audio_output_array_.push_back(audio);
}

void MockMediaStreamDispatcher::AddVideoDeviceToArray() {
  StreamDeviceInfo video;
  video.device.id = "video_device_id" + base::IntToString(session_id_);
  video.device.name = "usb video camera";
  video.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video.session_id = session_id_;
  video_array_.push_back(video);
}

}  // namespace content
