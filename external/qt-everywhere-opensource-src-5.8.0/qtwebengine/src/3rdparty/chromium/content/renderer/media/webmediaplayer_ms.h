// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "url/origin.h"

namespace blink {
class WebFrame;
class WebMediaPlayerClient;
class WebSecurityOrigin;
class WebString;
}

namespace media {
class MediaLog;
class VideoFrame;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace content {
class MediaStreamAudioRenderer;
class MediaStreamRendererFactory;
class MediaStreamVideoRenderer;
class WebMediaPlayerMSCompositor;
class RenderFrameObserver;

// WebMediaPlayerMS delegates calls from WebCore::MediaPlayerPrivate to
// Chrome's media player when "src" is from media stream.
//
// All calls to WebMediaPlayerMS methods must be from the main thread of
// Renderer process.
//
// WebMediaPlayerMS works with multiple objects, the most important ones are:
//
// MediaStreamVideoRenderer
//   provides video frames for rendering.
//
// blink::WebMediaPlayerClient
//   WebKit client of this media player object.
class CONTENT_EXPORT WebMediaPlayerMS
    : public NON_EXPORTED_BASE(blink::WebMediaPlayer),
      public NON_EXPORTED_BASE(media::WebMediaPlayerDelegate::Observer),
      public NON_EXPORTED_BASE(base::SupportsWeakPtr<WebMediaPlayerMS>) {
 public:
  // Construct a WebMediaPlayerMS with reference to the client, and
  // a MediaStreamClient which provides MediaStreamVideoRenderer.
  WebMediaPlayerMS(
      blink::WebFrame* frame,
      blink::WebMediaPlayerClient* client,
      base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
      media::MediaLog* media_log,
      std::unique_ptr<MediaStreamRendererFactory> factory,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const blink::WebString& sink_id,
      const blink::WebSecurityOrigin& security_origin);

  ~WebMediaPlayerMS() override;

  void load(LoadType load_type,
            const blink::WebMediaPlayerSource& source,
            CORSMode cors_mode) override;

  // Playback controls.
  void play() override;
  void pause() override;
  bool supportsSave() const override;
  void seek(double seconds) override;
  void setRate(double rate) override;
  void setVolume(double volume) override;
  void setSinkId(const blink::WebString& sink_id,
                 const blink::WebSecurityOrigin& security_origin,
                 blink::WebSetSinkIdCallbacks* web_callback) override;
  void setPreload(blink::WebMediaPlayer::Preload preload) override;
  blink::WebTimeRanges buffered() const override;
  blink::WebTimeRanges seekable() const override;

  // Methods for painting.
  void paint(blink::WebCanvas* canvas,
             const blink::WebRect& rect,
             unsigned char alpha,
             SkXfermode::Mode mode) override;
  media::SkCanvasVideoRenderer* GetSkCanvasVideoRenderer();
  void ResetCanvasCache();

  // Methods to trigger resize event.
  void TriggerResize();

  // True if the loaded media has a playable video/audio track.
  bool hasVideo() const override;
  bool hasAudio() const override;

  // Dimensions of the video.
  blink::WebSize naturalSize() const override;

  // Getters of playback state.
  bool paused() const override;
  bool seeking() const override;
  double duration() const override;
  double currentTime() const override;

  // Internal states of loading and network.
  blink::WebMediaPlayer::NetworkState getNetworkState() const override;
  blink::WebMediaPlayer::ReadyState getReadyState() const override;

  blink::WebString getErrorMessage() override;
  bool didLoadingProgress() override;

  bool hasSingleSecurityOrigin() const override;
  bool didPassCORSAccessCheck() const override;

  double mediaTimeForTimeValue(double timeValue) const override;

  unsigned decodedFrameCount() const override;
  unsigned droppedFrameCount() const override;
  size_t audioDecodedByteCount() const override;
  size_t videoDecodedByteCount() const override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnHidden() override;
  void OnShown() override;
  void OnSuspendRequested(bool must_suspend) override;
  void OnPlay() override;
  void OnPause() override;
  void OnVolumeMultiplierUpdate(double multiplier) override;

  bool copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface* gl,
                                         unsigned int texture,
                                         unsigned int internal_format,
                                         unsigned int type,
                                         bool premultiply_alpha,
                                         bool flip_y) override;

 private:
  friend class WebMediaPlayerMSTest;

  // The callback for MediaStreamVideoRenderer to signal a new frame is
  // available.
  void OnFrameAvailable(const scoped_refptr<media::VideoFrame>& frame);
  // Need repaint due to state change.
  void RepaintInternal();

  // The callback for source to report error.
  void OnSourceError();

  // Helpers that set the network/ready state and notifies the client if
  // they've changed.
  void SetNetworkState(blink::WebMediaPlayer::NetworkState state);
  void SetReadyState(blink::WebMediaPlayer::ReadyState state);

  // Getter method to |client_|.
  blink::WebMediaPlayerClient* get_client() { return client_; }

  blink::WebFrame* const frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  const blink::WebTimeRanges buffered_;

  blink::WebMediaPlayerClient* const client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  const base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;
  int delegate_id_;

  scoped_refptr<MediaStreamVideoRenderer> video_frame_provider_; // Weak

  std::unique_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  scoped_refptr<MediaStreamAudioRenderer> audio_renderer_; // Weak
  media::SkCanvasVideoRenderer video_renderer_;

  bool last_frame_opaque_;
  bool paused_;
  bool render_frame_suspended_;
  bool received_first_frame_;

  scoped_refptr<media::MediaLog> media_log_;

  std::unique_ptr<MediaStreamRendererFactory> renderer_factory_;

  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  const scoped_refptr<base::TaskRunner> worker_task_runner_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;

  // Used for DCHECKs to ensure methods calls executed in the correct thread.
  base::ThreadChecker thread_checker_;

  // WebMediaPlayerMS owns |compositor_| and destroys it on
  // |compositor_task_runner_|.
  std::unique_ptr<WebMediaPlayerMSCompositor> compositor_;

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  const std::string initial_audio_output_device_id_;
  const url::Origin initial_security_origin_;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;
  double volume_multiplier_;

  // True if playback should be started upon the next call to OnShown(). Only
  // used on Android.
  bool should_play_upon_shown_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerMS);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_MS_H_
