// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"

namespace content {

// Names for media stream source capture types.
// These are values of the "TrackControls.stream_source" field, and are
// set via the "chromeMediaSource" constraint.
CONTENT_EXPORT extern const char kMediaStreamSourceTab[];
CONTENT_EXPORT extern const char kMediaStreamSourceScreen[];
CONTENT_EXPORT extern const char kMediaStreamSourceDesktop[];
CONTENT_EXPORT extern const char kMediaStreamSourceSystem[];

struct CONTENT_EXPORT TrackControls {
 public:
  TrackControls();
  TrackControls(bool request);
  TrackControls(const TrackControls& other);
  ~TrackControls();
  bool requested;

  // Source. This is "tab", "screen", "desktop", "system", or blank.
  // Consider replacing with MediaStreamType enum variables.
  std::string stream_source;  // audio.kMediaStreamSource

  // Device ID requests.
  // The first set represents required devices - either grab one or fail.
  // The second set represents optional devices - if we can't get one of
  // these, we will grab the default device (if possible).
  // The constraint names are "sourceId" and "chromeMediaSourceId".
  std::vector<std::string> device_ids;
  std::vector<std::string> alternate_device_ids;
};

// StreamControls describes what is sent to the browser process
// to the renderer process in order to control the opening of a device
// pair. This may result in opening one audio and/or one video device.
// This has to be a struct with public members in order to allow it to
// be sent in the IPC of media_stream_messages.h
struct CONTENT_EXPORT StreamControls {
 public:
  StreamControls();
  StreamControls(bool request_audio, bool request_video);
  ~StreamControls();
  TrackControls audio;
  TrackControls video;
  // Hotword functionality (chromeos only)
  // See crbug.com/564574 for discussion on possibly #ifdef'ing this out.
  bool hotword_enabled;  // kMediaStreamAudioHotword = "googHotword";
};

// StreamDeviceInfo describes information about a device.
struct CONTENT_EXPORT StreamDeviceInfo {
  static const int kNoId;

  StreamDeviceInfo();
  StreamDeviceInfo(MediaStreamType service_param,
                   const std::string& name_param,
                   const std::string& device_param);
  StreamDeviceInfo(MediaStreamType service_param,
                   const std::string& name_param,
                   const std::string& device_param,
                   int sample_rate,
                   int channel_layout,
                   int frames_per_buffer);
  static bool IsEqual(const StreamDeviceInfo& first,
                      const StreamDeviceInfo& second);

  MediaStreamDevice device;

  // Id for this capture session. Unique for all sessions of the same type.
  int session_id;
};

typedef std::vector<StreamDeviceInfo> StreamDeviceInfoArray;

}  // namespace content

#endif  // CONTENT_COMMON_MEDIA_MEDIA_STREAM_OPTIONS_H_
