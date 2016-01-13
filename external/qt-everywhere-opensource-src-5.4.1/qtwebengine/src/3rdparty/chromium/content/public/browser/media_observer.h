// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_

#include "base/callback_forward.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/common/media_stream_request.h"

namespace content {

// An embedder may implement MediaObserver and return it from
// ContentBrowserClient to receive callbacks as media events occur.
class MediaObserver {
 public:
  // Called when a audio capture device is plugged in or unplugged.
  virtual void OnAudioCaptureDevicesChanged() = 0;

  // Called when a video capture device is plugged in or unplugged.
  virtual void OnVideoCaptureDevicesChanged() = 0;

  // Called when a media request changes state.
  virtual void OnMediaRequestStateChanged(
      int render_process_id,
      int render_view_id,
      int page_request_id,
      const GURL& security_origin,
      const MediaStreamDevice& device,
      MediaRequestState state) = 0;

  // Called when an audio stream is being created.
  virtual void OnCreatingAudioStream(int render_process_id,
                                     int render_frame_id) = 0;

  // Called when an audio stream transitions into a playing state.
  // |power_read_callback| is a thread-safe callback, provided for polling the
  // current audio signal power level, and any copies/references to it must be
  // destroyed when OnAudioStreamStopped() is called.
  //
  // The callback returns the current power level (in dBFS units) and the clip
  // status (true if any part of the audio signal has clipped since the last
  // callback run).  See media/audio/audio_power_monitor.h for more info.
  typedef base::Callback<std::pair<float, bool>()> ReadPowerAndClipCallback;
  virtual void OnAudioStreamPlaying(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const ReadPowerAndClipCallback& power_read_callback) = 0;

  // Called when an audio stream has stopped.
  virtual void OnAudioStreamStopped(
      int render_process_id,
      int render_frame_id,
      int stream_id) = 0;

 protected:
  virtual ~MediaObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_OBSERVER_H_
