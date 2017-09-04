// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_RENDERER_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_RENDERER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/renderer/media_stream_video_renderer.h"
#include "content/public/renderer/media_stream_video_sink.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {

// MediaStreamVideoRendererSink is a MediaStreamVideoRenderer designed for
// rendering Video MediaStreamTracks [1], MediaStreamVideoRendererSink
// implements MediaStreamVideoSink in order to render video frames provided from
// a MediaStreamVideoTrack, to which it connects itself when the
// MediaStreamVideoRenderer is Start()ed, and disconnects itself when the latter
// is Stop()ed.
//
// [1] http://dev.w3.org/2011/webrtc/editor/getusermedia.html#mediastreamtrack
//
// TODO(wuchengli): Add unit test. See the link below for reference.
// http://src.chromium.org/viewvc/chrome/trunk/src/content/renderer/media/rtc_vi
// deo_decoder_unittest.cc?revision=180591&view=markup
class CONTENT_EXPORT MediaStreamVideoRendererSink
    : NON_EXPORTED_BASE(public MediaStreamVideoRenderer),
      NON_EXPORTED_BASE(public MediaStreamVideoSink) {
 public:
  MediaStreamVideoRendererSink(
      const blink::WebMediaStreamTrack& video_track,
      const base::Closure& error_cb,
      const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories);

  // MediaStreamVideoRenderer implementation. Called on the main thread.
  void Start() override;
  void Stop() override;
  void Resume() override;
  void Pause() override;

  void SetGpuMemoryBufferVideoForTesting(
      media::GpuMemoryBufferVideoFramePool* gpu_memory_buffer_pool);

 protected:
  ~MediaStreamVideoRendererSink() override;

 private:
  friend class MediaStreamVideoRendererSinkTest;
  enum State {
    STARTED,
    PAUSED,
    STOPPED,
  };

  void OnVideoFrame(const scoped_refptr<media::VideoFrame>& frame,
                    base::TimeTicks estimated_capture_time);
  void FrameReady(const scoped_refptr<media::VideoFrame>& frame);

  // MediaStreamVideoSink implementation. Called on the main thread.
  void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) override;

  void RenderSignalingFrame();

  const base::Closure error_cb_;
  const RepaintCB repaint_cb_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  State state_;
  gfx::Size frame_size_;
  const blink::WebMediaStreamTrack video_track_;

  // Pool of GpuMemoryBuffers and resources used to create hardware frames.
  std::unique_ptr<media::GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  base::WeakPtrFactory<MediaStreamVideoRendererSink> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoRendererSink);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_RENDERER_H_
