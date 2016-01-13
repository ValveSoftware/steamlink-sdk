// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SINK_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_VIDEO_SINK_H_

#include "content/public/renderer/media_stream_video_sink.h"

#include "base/memory/weak_ptr.h"
#include "content/common/media/video_capture.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMediaStreamVideoSink : public MediaStreamVideoSink {
 public:
  MockMediaStreamVideoSink();
  virtual ~MockMediaStreamVideoSink();

  virtual void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) OVERRIDE;
  virtual void OnEnabledChanged(bool enabled) OVERRIDE;

  // Triggered when OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame)
  // is called.
  MOCK_METHOD0(OnVideoFrame, void());

  VideoCaptureDeliverFrameCB GetDeliverFrameCB();

  int number_of_frames() const { return number_of_frames_; }
  media::VideoFrame::Format format() const { return format_; }
  gfx::Size frame_size() const { return frame_size_; }
  scoped_refptr<media::VideoFrame> last_frame() const { return last_frame_; };

  bool enabled() const { return enabled_; }
  blink::WebMediaStreamSource::ReadyState state() const { return state_; }

 private:
  void DeliverVideoFrame(
      const scoped_refptr<media::VideoFrame>& frame,
      const media::VideoCaptureFormat& format,
      const base::TimeTicks& estimated_capture_time);

  int number_of_frames_;
  bool enabled_;
  media::VideoFrame::Format format_;
  blink::WebMediaStreamSource::ReadyState state_;
  gfx::Size frame_size_;
  scoped_refptr<media::VideoFrame> last_frame_;
  base::WeakPtrFactory<MockMediaStreamVideoSink> weak_factory_;
};

}  // namespace content

#endif
