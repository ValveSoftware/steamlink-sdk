// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_
#define CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

// Types of media streams.
enum MediaStreamType {
  MEDIA_NO_SERVICE = 0,

  // A device provided by the operating system (e.g., webcam input).
  MEDIA_DEVICE_AUDIO_CAPTURE,
  MEDIA_DEVICE_VIDEO_CAPTURE,

  // Mirroring of a browser tab.
  MEDIA_TAB_AUDIO_CAPTURE,
  MEDIA_TAB_VIDEO_CAPTURE,

  // Desktop media sources.
  MEDIA_DESKTOP_VIDEO_CAPTURE,

  // Capture system audio (post-mix loopback stream).
  MEDIA_DESKTOP_AUDIO_CAPTURE,

  // This is used for enumerating audio output devices.
  // TODO(grunell): Output isn't really a part of media streams. Device
  // enumeration should be decoupled from media streams and related code.
  MEDIA_DEVICE_AUDIO_OUTPUT,

  NUM_MEDIA_TYPES
};

// Types of media stream requests that can be made to the media controller.
enum MediaStreamRequestType {
  MEDIA_DEVICE_ACCESS = 0,
  MEDIA_GENERATE_STREAM,
  MEDIA_ENUMERATE_DEVICES,
  MEDIA_OPEN_DEVICE_PEPPER_ONLY  // Only used in requests made by Pepper.
};

// Facing mode for video capture.
enum VideoFacingMode {
  MEDIA_VIDEO_FACING_NONE = 0,
  MEDIA_VIDEO_FACING_USER,
  MEDIA_VIDEO_FACING_ENVIRONMENT,

  NUM_MEDIA_VIDEO_FACING_MODE
};

// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before NUM_MEDIA_REQUEST_RESULTS.
enum MediaStreamRequestResult {
  MEDIA_DEVICE_OK = 0,
  MEDIA_DEVICE_PERMISSION_DENIED = 1,
  MEDIA_DEVICE_PERMISSION_DISMISSED = 2,
  MEDIA_DEVICE_INVALID_STATE = 3,
  MEDIA_DEVICE_NO_HARDWARE = 4,
  MEDIA_DEVICE_INVALID_SECURITY_ORIGIN = 5,
  MEDIA_DEVICE_TAB_CAPTURE_FAILURE = 6,
  MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE = 7,
  MEDIA_DEVICE_CAPTURE_FAILURE = 8,
  MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED = 9,
  MEDIA_DEVICE_TRACK_START_FAILURE = 10,
  MEDIA_DEVICE_NOT_SUPPORTED = 11,
  MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN = 12,
  MEDIA_DEVICE_KILL_SWITCH_ON = 13,
  NUM_MEDIA_REQUEST_RESULTS
};

// Convenience predicates to determine whether the given type represents some
// audio or some video device.
CONTENT_EXPORT bool IsAudioInputMediaType(MediaStreamType type);
CONTENT_EXPORT bool IsVideoMediaType(MediaStreamType type);

// TODO(xians): Change the structs to classes.
// Represents one device in a request for media stream(s).
struct CONTENT_EXPORT MediaStreamDevice {
  MediaStreamDevice();

  MediaStreamDevice(
      MediaStreamType type,
      const std::string& id,
      const std::string& name);

  MediaStreamDevice(
      MediaStreamType type,
      const std::string& id,
      const std::string& name,
      int sample_rate,
      int channel_layout,
      int frames_per_buffer);

  MediaStreamDevice(const MediaStreamDevice& other);

  ~MediaStreamDevice();

  bool IsEqual(const MediaStreamDevice& second) const;

  // The device's type.
  MediaStreamType type;

  // The device's unique ID.
  std::string id;

  // The facing mode for video capture device.
  VideoFacingMode video_facing;

  // The device id of a matched output device if any (otherwise empty).
  // Only applicable to audio devices.
  std::string matched_output_device_id;

  // The device's "friendly" name. Not guaranteed to be unique.
  std::string name;

  // Contains properties that match directly with those with the same name
  // in media::AudioParameters.
  // TODO(ajm): Remove this type and use media::AudioParameters directly.
  struct CONTENT_EXPORT AudioDeviceParameters {
    AudioDeviceParameters();
    AudioDeviceParameters(int sample_rate,
                          int channel_layout,
                          int frames_per_buffer);
    AudioDeviceParameters(const AudioDeviceParameters& other);

    ~AudioDeviceParameters();

    // Preferred sample rate in samples per second for the device.
    int sample_rate;

    // Preferred channel configuration for the device.
    // TODO(henrika): ideally, we would like to use media::ChannelLayout here
    // but including media/base/channel_layout.h violates checkdeps rules.
    int channel_layout;

    // Preferred number of frames per buffer for the device.  This is filled
    // in on the browser side and can be used by the renderer to match the
    // expected browser side settings and avoid unnecessary buffering.
    // See media::AudioParameters for more.
    int frames_per_buffer;

    // See media::AudioParameters::PlatformEffectsMask.
    int effects;

    std::vector<media::Point> mic_positions;
  };

  // These below two member variables are valid only when the type of device is
  // audio (i.e. IsAudioInputMediaType returns true).

  // Contains the device properties of the capture device.
  AudioDeviceParameters input;

  // If the capture device has an associated output device (e.g. headphones),
  // this will contain the properties for the output device.  If no such device
  // exists (e.g. webcam w/mic), then the value of this member will be all
  // zeros.
  AudioDeviceParameters matched_output;
};

class CONTENT_EXPORT MediaStreamDevices
    : public std::vector<MediaStreamDevice> {
 public:
  MediaStreamDevices();
  MediaStreamDevices(size_t count, const MediaStreamDevice& value);

  // Looks for a MediaStreamDevice based on its ID.
  // Returns NULL if not found.
  const MediaStreamDevice* FindById(const std::string& device_id) const;
};

typedef std::map<MediaStreamType, MediaStreamDevices> MediaStreamDeviceMap;

// Represents a request for media streams (audio/video).
// TODO(vrk): Decouple MediaStreamDevice from this header file so that
// media_stream_options.h no longer depends on this file.
// TODO(vrk,justinlin,wjia): Figure out a way to share this code cleanly between
// vanilla WebRTC, Tab Capture, and Pepper Video Capture. Right now there is
// Tab-only stuff and Pepper-only stuff being passed around to all clients,
// which is icky.
struct CONTENT_EXPORT MediaStreamRequest {
  MediaStreamRequest(
      int render_process_id,
      int render_frame_id,
      int page_request_id,
      const GURL& security_origin,
      bool user_gesture,
      MediaStreamRequestType request_type,
      const std::string& requested_audio_device_id,
      const std::string& requested_video_device_id,
      MediaStreamType audio_type,
      MediaStreamType video_type);

  MediaStreamRequest(const MediaStreamRequest& other);

  ~MediaStreamRequest();

  // This is the render process id for the renderer associated with generating
  // frames for a MediaStream. Any indicators associated with a capture will be
  // displayed for this renderer.
  int render_process_id;

  // This is the render frame id for the renderer associated with generating
  // frames for a MediaStream. Any indicators associated with a capture will be
  // displayed for this renderer.
  int render_frame_id;

  // The unique id combined with render_process_id and render_frame_id for
  // identifying this request. This is used for cancelling request.
  int page_request_id;

  // The WebKit security origin for the current request (e.g. "html5rocks.com").
  GURL security_origin;

  // Set to true if the call was made in the context of a user gesture.
  bool user_gesture;

  // Stores the type of request that was made to the media controller. Right now
  // this is only used to distinguish between WebRTC and Pepper requests, as the
  // latter should not be subject to user approval but only to policy check.
  // Pepper requests are signified by the |MEDIA_OPEN_DEVICE| value.
  MediaStreamRequestType request_type;

  // Stores the requested raw device id for physical audio or video devices.
  std::string requested_audio_device_id;
  std::string requested_video_device_id;

  // Flag to indicate if the request contains audio.
  MediaStreamType audio_type;

  // Flag to indicate if the request contains video.
  MediaStreamType video_type;

  // True if all ancestors of the requesting frame have the same origin.
  bool all_ancestors_have_same_origin;
};

// Interface used by the content layer to notify chrome about changes in the
// state of a media stream. Instances of this class are passed to content layer
// when MediaStream access is approved using MediaResponseCallback.
class MediaStreamUI {
 public:
  virtual ~MediaStreamUI() {}

  // Called when MediaStream capturing is started. Chrome layer can call |stop|
  // to stop the stream. Returns the platform-dependent window ID for the UI, or
  // 0 if not applicable.
  virtual gfx::NativeViewId OnStarted(const base::Closure& stop) = 0;
};

// Callback used return results of media access requests.
typedef base::Callback<void(const MediaStreamDevices& devices,
                            content::MediaStreamRequestResult result,
                            std::unique_ptr<MediaStreamUI> ui)>
    MediaResponseCallback;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_
