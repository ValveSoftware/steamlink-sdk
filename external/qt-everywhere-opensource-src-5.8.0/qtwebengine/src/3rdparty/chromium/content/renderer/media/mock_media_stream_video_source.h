// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "content/renderer/media/media_stream_video_source.h"

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMediaStreamVideoSource : public MediaStreamVideoSource {
 public:
  explicit MockMediaStreamVideoSource(bool manual_get_supported_formats);
  MockMediaStreamVideoSource(bool manual_get_supported_formats,
                             bool respond_to_request_refresh_frame);
  virtual ~MockMediaStreamVideoSource();

  MOCK_METHOD1(DoSetMutedState, void(bool muted_state));

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

  const media::VideoCaptureFormat& start_format() const { return format_; }
  int max_requested_height() const { return max_requested_height_; }
  int max_requested_width() const { return max_requested_width_; }
  double max_requested_frame_rate() const { return max_requested_frame_rate_; }

  void SetMutedState(bool muted_state) override {
    MediaStreamVideoSource::SetMutedState(muted_state);
    DoSetMutedState(muted_state);
  }

  // Implements MediaStreamVideoSource.
  void RequestRefreshFrame() override;

 protected:
  // Implements MediaStreamVideoSource.
  void GetCurrentSupportedFormats(
      int max_requested_height,
      int max_requested_width,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;
  void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const blink::WebMediaConstraints& constraints,
      const VideoCaptureDeliverFrameCB& frame_callback) override;
  void StopSourceImpl() override;

 private:
  media::VideoCaptureFormat format_;
  media::VideoCaptureFormats supported_formats_;
  bool manual_get_supported_formats_;
  bool respond_to_request_refresh_frame_;
  int max_requested_height_;
  int max_requested_width_;
  double max_requested_frame_rate_;
  bool attempted_to_start_;
  VideoCaptureDeviceFormatsCB formats_callback_;
  VideoCaptureDeliverFrameCB frame_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamVideoSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_
