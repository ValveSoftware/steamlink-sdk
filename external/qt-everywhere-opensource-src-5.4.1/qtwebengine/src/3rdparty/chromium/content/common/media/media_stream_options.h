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

// MediaStreamConstraint keys for constraints that are passed to getUserMedia.
CONTENT_EXPORT extern const char kMediaStreamSource[];
CONTENT_EXPORT extern const char kMediaStreamSourceId[];
CONTENT_EXPORT extern const char kMediaStreamSourceInfoId[];
CONTENT_EXPORT extern const char kMediaStreamSourceTab[];
CONTENT_EXPORT extern const char kMediaStreamSourceScreen[];
CONTENT_EXPORT extern const char kMediaStreamSourceDesktop[];
CONTENT_EXPORT extern const char kMediaStreamSourceSystem[];

// Experimental constraint to do device matching.  When this optional constraint
// is set, WebRTC audio renderer will render audio from media streams to an
// output device that belongs to the same hardware as the requested source
// device belongs to.
CONTENT_EXPORT extern const char kMediaStreamRenderToAssociatedSink[];

// Controls whether ducking of audio is enabled on platforms that support it.
CONTENT_EXPORT extern const char kMediaStreamAudioDucking[];

// StreamOptions is a Chromium representation of constraints
// used in WebUserMediaRequest.
// It describes properties requested by JS in a request for a new
// media stream.
class CONTENT_EXPORT StreamOptions {
 public:
  StreamOptions();
  StreamOptions(bool request_audio, bool request_video);
  ~StreamOptions();

  struct CONTENT_EXPORT Constraint {
    Constraint();
    Constraint(const std::string& name,
               const std::string& value);

    std::string name;
    std::string value;
  };
  typedef std::vector<Constraint> Constraints;

  bool audio_requested;
  Constraints mandatory_audio;
  Constraints optional_audio;

  bool video_requested;
  Constraints mandatory_video;
  Constraints optional_video;

  // Fetches |value| from the first audio constraint with a name that matches
  // |name| from |mandatory_audio| and |optional_audio|. First mandatory
  // constraints are searched, then optional.
  // |is_mandatory| may be NULL but if it is provided, it is set
  // to true if the found constraint is mandatory.
  // Returns false if no constraint is found.
  bool GetFirstAudioConstraintByName(const std::string& name,
                                     std::string* value,
                                     bool* is_mandatory) const;

  // Fetches |value| from the first video constraint with a name that matches
  // |name| from |mandatory_video| and |optional_video|. First mandatory
  // constraints are searched, then optional.
  // |is_mandatory| may be NULL but if it is provided, it is set
  // to true if the found constraint is mandatory.
  // Returns false if no constraint is found.
  bool GetFirstVideoConstraintByName(const std::string& name,
                                     std::string* value,
                                     bool* is_mandatory) const;

  // Fetches |values| from all constraint with a name that matches |name|
  // from |constraints|.
  static void GetConstraintsByName(
      const StreamOptions::Constraints& constraints,
      const std::string& name,
      std::vector<std::string>* values);
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
