// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "content/renderer/media/media_stream_video_source.h"

namespace content {

class MockMediaStreamVideoSource : public MediaStreamVideoSource {
 public:
  explicit MockMediaStreamVideoSource(bool manual_get_supported_formats);
  virtual ~MockMediaStreamVideoSource();

  // Simulate that the underlying source start successfully.
  void StartMockedSource();

  // Simulate that the underlying source fail to start.
  void FailToStartMockedSource();

  // Returns true if StartSource  has been called and StartMockedSource
  // or FailToStartMockedSource has not been called.
  bool SourceHasAttemptedToStart() { return attempted_to_start_; }

  void SetSupportedFormats(const media::VideoCaptureFormats& formats) {
    supported_formats_ = formats;
  }

  // Delivers |frame| to all registered tracks on the IO thread. Its up to the
  // call to make sure MockMediaStreamVideoSource is not destroyed before the
  // frame has been delivered.
  void DeliverVideoFrame(const scoped_refptr<media::VideoFrame>& frame);

  void CompleteGetSupportedFormats();

  const media::VideoCaptureParams& start_params() const { return params_; }
  int max_requested_height() const { return max_requested_height_; }
  int max_requested_width() const { return max_requested_width_; }

 protected:
  void DeliverVideoFrameOnIO(const scoped_refptr<media::VideoFrame>& frame,
                             media::VideoCaptureFormat format,
                             const base::TimeTicks& estimated_capture_time,
                             const VideoCaptureDeliverFrameCB& frame_callback);

  // Implements MediaStreamVideoSource.
  virtual void GetCurrentSupportedFormats(
      int max_requested_height,
      int max_requested_width,
      const VideoCaptureDeviceFormatsCB& callback) OVERRIDE;
  virtual void StartSourceImpl(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& frame_callback) OVERRIDE;
  virtual void StopSourceImpl() OVERRIDE;

 private:
  media::VideoCaptureParams params_;
  media::VideoCaptureFormats supported_formats_;
  bool manual_get_supported_formats_;
  int max_requested_height_;
  int max_requested_width_;
  bool attempted_to_start_;
  VideoCaptureDeviceFormatsCB formats_callback_;
  VideoCaptureDeliverFrameCB frame_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_
