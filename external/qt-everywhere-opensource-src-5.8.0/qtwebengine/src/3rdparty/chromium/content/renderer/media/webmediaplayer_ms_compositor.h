// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_COMPOSITOR_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_COMPOSITOR_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebMediaStream;
}

namespace gfx {
class Size;
}

namespace media {
class SkCanvasVideoRenderer;
class VideoRendererAlgorithm;
}

namespace content {
class WebMediaPlayerMS;

// This class is designed to handle the work load on compositor thread for
// WebMediaPlayerMS. It will be instantiated on the main thread, but destroyed
// on the compositor thread.
//
// WebMediaPlayerMSCompositor utilizes VideoRendererAlgorithm to store the
// incoming frames and select the best frame for rendering to maximize the
// smoothness, if REFERENCE_TIMEs are populated for incoming VideoFrames.
// Otherwise, WebMediaPlayerMSCompositor will simply store the most recent
// frame, and submit it whenever asked by the compositor.
class CONTENT_EXPORT WebMediaPlayerMSCompositor
    : public NON_EXPORTED_BASE(cc::VideoFrameProvider) {
 public:
  // This |url| represents the media stream we are rendering. |url| is used to
  // find out what web stream this WebMediaPlayerMSCompositor is playing, and
  // together with flag "--disable-rtc-smoothness-algorithm" determine whether
  // we enable algorithm or not.
  WebMediaPlayerMSCompositor(
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const blink::WebMediaStream& web_stream,
      const base::WeakPtr<WebMediaPlayerMS>& player);

  ~WebMediaPlayerMSCompositor() override;

  void EnqueueFrame(const scoped_refptr<media::VideoFrame>& frame);

  // Statistical data
  gfx::Size GetCurrentSize();
  base::TimeDelta GetCurrentTime();
  size_t total_frame_count() const;
  size_t dropped_frame_count() const;

  // VideoFrameProvider implementation.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;

  // Return the current frame being rendered.
  // Difference between GetCurrentFrame(): GetCurrentFrame() is designed for
  // chrome compositor to pull frame from WebMediaPlayerMSCompositor, and thus
  // calling GetCurrentFrame() will affect statistics like |dropped_frames_|
  // etc. Calling this function has no side effect.
  scoped_refptr<media::VideoFrame> GetCurrentFrameWithoutUpdatingStatistics();

  void StartRendering();
  void StopRendering();
  void ReplaceCurrentFrameWithACopy();

 private:
  friend class WebMediaPlayerMSTest;

  bool MapTimestampsToRenderTimeTicks(
      const std::vector<base::TimeDelta>& timestamps,
      std::vector<base::TimeTicks>* wall_clock_times);

  void SetCurrentFrame(const scoped_refptr<media::VideoFrame>& frame);

  // For algorithm enabled case only: given the render interval, update
  // current_frame_ and dropped_frame_count_.
  void Render(base::TimeTicks deadline_min, base::TimeTicks deadline_max);

  void StartRenderingInternal();
  void StopRenderingInternal();

  void SetAlgorithmEnabledForTesting(bool algorithm_enabled);

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  base::MessageLoop* main_message_loop_;

  base::WeakPtr<WebMediaPlayerMS> player_;

  size_t serial_;

  // A pointer back to the compositor to inform it about state changes. This
  // is not |nullptr| while the compositor is actively using this
  // VideoFrameProvider. This will be set to |nullptr| when the compositor stops
  // serving this VideoFrameProvider.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  // |current_frame_| is updated only on compositor thread. The object it
  // holds can be freed on the compositor thread if it is the last to hold a
  // reference but media::VideoFrame is a thread-safe ref-pointer. It is
  // however read on the compositor and main thread so locking is required
  // around all modifications and all reads on the any thread.
  scoped_refptr<media::VideoFrame> current_frame_;

  // |rendering_frame_buffer_| stores the incoming frames, and provides a frame
  // selection method which returns the best frame for the render interval.
  std::unique_ptr<media::VideoRendererAlgorithm> rendering_frame_buffer_;

  // |current_frame_used_by_compositor_| is updated on compositor thread only.
  // It's used to track whether |current_frame_| was painted for detecting
  // when to increase |dropped_frame_count_|.
  bool current_frame_used_by_compositor_;

  // Historical data about last rendering. These are for detecting whether
  // rendering is paused (one reason is that the tab is not in the front), in
  // which case we need to do background rendering.
  base::TimeTicks last_deadline_max_;
  base::TimeDelta last_render_length_;

  size_t total_frame_count_;
  size_t dropped_frame_count_;

  bool stopped_;

  std::map<base::TimeDelta, base::TimeTicks> timestamps_to_clock_times_;

  // |current_frame_lock_| protects |current_frame_used_by_compositor_|,
  // |current_frame_|, and |rendering_frame_buffer_|.
  base::Lock current_frame_lock_;

  // Make sure the weak pointer factory member is the last member of the class.
  base::WeakPtrFactory<WebMediaPlayerMSCompositor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMSCompositor);
};
}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_COMPOSITOR_H_
