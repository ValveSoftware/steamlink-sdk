// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/media_stream_video_source.h"

namespace media {
class VideoCapturerSource;
}  // namespace media

namespace content {

// Representation of a video stream coming from a camera, owned by Blink as
// WebMediaStreamSource. Objects of this class are created and live on main
// Render thread. Objects can be constructed either by indicating a |device| to
// look for, or by plugging in a |source| constructed elsewhere.
class CONTENT_EXPORT MediaStreamVideoCapturerSource
    : public MediaStreamVideoSource,
      public RenderFrameObserver {
 public:
  MediaStreamVideoCapturerSource(
      const SourceStoppedCallback& stop_callback,
      std::unique_ptr<media::VideoCapturerSource> source);
  MediaStreamVideoCapturerSource(const SourceStoppedCallback& stop_callback,
                                 const StreamDeviceInfo& device_info,
                                 RenderFrame* render_frame);
  ~MediaStreamVideoCapturerSource() override;

  // Implements MediaStreamVideoSource.
  void RequestRefreshFrame() override;

  void SetCapturingLinkSecured(bool is_secure) override;

 private:
  friend class CanvasCaptureHandlerTest;
  friend class MediaStreamVideoCapturerSourceTest;

  // Implements MediaStreamVideoSource.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;
  void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const blink::WebMediaConstraints& constraints,
      const VideoCaptureDeliverFrameCB& frame_callback) override;
  void StopSourceImpl() override;

  // RenderFrameObserver implementation.
  void OnDestruct() final {}

  // Method to bind as RunningCallback in VideoCapturerSource::StartCapture().
  void OnStarted(bool result);

  const char* GetPowerLineFrequencyForTesting() const;

  // The source that provides video frames.
  const std::unique_ptr<media::VideoCapturerSource> source_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
