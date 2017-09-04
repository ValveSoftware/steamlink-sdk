// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/public/common/features.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace media {
class VideoFrame;
}  // namespace media

namespace video_track_recorder {
const int kVEAEncoderMinResolutionWidth = 640;
const int kVEAEncoderMinResolutionHeight = 480;
}  // namespace video_track_recorder

namespace content {

// VideoTrackRecorder is a MediaStreamVideoSink that encodes the video frames
// received from a Stream Video Track. This class is constructed and used on a
// single thread, namely the main Render thread. This mirrors the other
// MediaStreamVideo* classes that are constructed/configured on Main Render
// thread but that pass frames on Render IO thread. It has an internal Encoder
// with its own threading subtleties, see the implementation file.
class CONTENT_EXPORT VideoTrackRecorder
    : NON_EXPORTED_BASE(public MediaStreamVideoSink) {
 public:
  enum class CodecId {
    VP8,
    VP9,
    H264,
  };
  class Encoder;

  using OnEncodedVideoCB =
      base::Callback<void(const scoped_refptr<media::VideoFrame>& video_frame,
                          std::unique_ptr<std::string> encoded_data,
                          base::TimeTicks capture_timestamp,
                          bool is_key_frame)>;

  VideoTrackRecorder(CodecId codec,
                     const blink::WebMediaStreamTrack& track,
                     const OnEncodedVideoCB& on_encoded_video_cb,
                     int32_t bits_per_second);
  ~VideoTrackRecorder() override;

  void Pause();
  void Resume();

  void OnVideoFrameForTesting(const scoped_refptr<media::VideoFrame>& frame,
                              base::TimeTicks capture_time);
 private:
  friend class VideoTrackRecorderTest;

  void InitializeEncoder(CodecId codec,
                         const OnEncodedVideoCB& on_encoded_video_callback,
                         int32_t bits_per_second,
                         const scoped_refptr<media::VideoFrame>& frame,
                         base::TimeTicks capture_time);

  // Used to check that we are destroyed on the same thread we were created.
  base::ThreadChecker main_render_thread_checker_;

  // We need to hold on to the Blink track to remove ourselves on dtor.
  blink::WebMediaStreamTrack track_;

  // Inner class to encode using whichever codec is configured.
  scoped_refptr<Encoder> encoder_;

  base::Callback<void(const scoped_refptr<media::VideoFrame>& frame,
                      base::TimeTicks capture_time)>
      initialize_encoder_callback_;

  // Used to track the paused state during the initialization process.
  bool paused_before_init_;

  base::WeakPtrFactory<VideoTrackRecorder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoTrackRecorder);
};

}  // namespace content
#endif  // CONTENT_RENDERER_MEDIA_VIDEO_TRACK_RECORDER_H_
