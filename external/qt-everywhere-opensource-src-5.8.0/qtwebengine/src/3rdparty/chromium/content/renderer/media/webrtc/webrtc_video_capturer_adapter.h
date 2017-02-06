// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_

#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"
#include "third_party/webrtc/media/base/videocapturer.h"

namespace content {

// WebRtcVideoCapturerAdapter implements a simple cricket::VideoCapturer that is
// used for VideoCapturing in libJingle and especially in PeerConnections.
// The class is created and destroyed on the main render thread.
// PeerConnection access cricket::VideoCapturer from a libJingle worker thread.
// An instance of WebRtcVideoCapturerAdapter is owned by an instance of
// webrtc::VideoTrackSourceInterface in libJingle. The implementation of
// webrtc::VideoTrackSourceInterface guarantees that this object is not deleted
// while it is still used in libJingle.
class CONTENT_EXPORT WebRtcVideoCapturerAdapter
    : NON_EXPORTED_BASE(public cricket::VideoCapturer) {
 public:
  explicit WebRtcVideoCapturerAdapter(bool is_screencast);
  ~WebRtcVideoCapturerAdapter() override;

  // OnFrameCaptured delivers video frames to libjingle. It must be called on
  // libjingles worker thread.
  // This method is virtual for testing purposes.
  virtual void OnFrameCaptured(const scoped_refptr<media::VideoFrame>& frame);

 private:
  // cricket::VideoCapturer implementation.
  // These methods are accessed from a libJingle worker thread.
  cricket::CaptureState Start(
      const cricket::VideoFormat& capture_format) override;
  void Stop() override;
  bool IsRunning() override;
  bool GetPreferredFourccs(std::vector<uint32_t>* fourccs) override;
  bool GetBestCaptureFormat(const cricket::VideoFormat& desired,
                            cricket::VideoFormat* best_format) override;
  bool IsScreencast() const override;

  // |thread_checker_| is bound to the libjingle worker thread.
  base::ThreadChecker thread_checker_;

  const bool is_screencast_;
  bool running_;

  class MediaVideoFrameFactory;

  DISALLOW_COPY_AND_ASSIGN(WebRtcVideoCapturerAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_CAPTURER_ADAPTER_H_
