// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "cc/layers/video_frame_provider.h"
#include "content/renderer/media/android/media_info_loader.h"
#include "content/renderer/media/android/media_source_delegate.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/android/media_player_android.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/media_keys.h"
#include "media/base/time_delta_interpolator.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebSetSinkIdCallbacks.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect_f.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebContentDecryptionModule;
class WebContentDecryptionModuleResult;
class WebFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
class WebURL;
}

namespace cc_blink {
class WebLayerImpl;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
struct MailboxHolder;
}

namespace media {
class CdmContext;
class MediaLog;
class WebContentDecryptionModuleImpl;
}

namespace content {

class RendererMediaPlayerManager;

// This class implements blink::WebMediaPlayer by keeping the android
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class WebMediaPlayerAndroid
    : public blink::WebMediaPlayer,
      public cc::VideoFrameProvider,
      public media::RendererMediaPlayerInterface,
      public NON_EXPORTED_BASE(media::WebMediaPlayerDelegate::Observer) {
 public:
  // Construct a WebMediaPlayerAndroid object. This class communicates with the
  // MediaPlayerAndroid object in the browser process through |proxy|.
  // TODO(qinmin): |frame| argument is used to determine whether the current
  // player can enter fullscreen. This logic should probably be moved into
  // blink, so that enteredFullscreen() will not be called if another video is
  // already in fullscreen.
  WebMediaPlayerAndroid(
      blink::WebFrame* frame,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
      RendererMediaPlayerManager* player_manager,
      scoped_refptr<StreamTextureFactory> factory,
      int frame_id,
      bool enable_texture_copy,
      const media::WebMediaPlayerParams& params);
  ~WebMediaPlayerAndroid() override;

  // blink::WebMediaPlayer implementation.
  bool supportsOverlayFullscreenVideo() override;
  void enteredFullscreen() override;

  // Resource loading.
  void load(LoadType load_type,
            const blink::WebMediaPlayerSource& source,
            CORSMode cors_mode) override;

  // Playback controls.
  void play() override;
  void pause() override;
  void seek(double seconds) override;
  bool supportsSave() const override;
  void setRate(double rate) override;
  void setVolume(double volume) override;
  void setSinkId(const blink::WebString& sink_id,
                 const blink::WebSecurityOrigin& security_origin,
                 blink::WebSetSinkIdCallbacks* web_callback) override;
  void requestRemotePlayback() override;
  void requestRemotePlaybackControl() override;
  blink::WebTimeRanges buffered() const override;
  blink::WebTimeRanges seekable() const override;

  // Poster image, as defined in the <video> element.
  void setPoster(const blink::WebURL& poster) override;

  // Methods for painting.
  // FIXME: This path "only works" on Android. It is a workaround for the
  // issue that Skia could not handle Android's GL_TEXTURE_EXTERNAL_OES texture
  // internally. It should be removed and replaced by the normal paint path.
  // https://code.google.com/p/skia/issues/detail?id=1189
  void paint(blink::WebCanvas* canvas,
             const blink::WebRect& rect,
             unsigned char alpha,
             SkXfermode::Mode mode) override;

  bool copyVideoTextureToPlatformTexture(gpu::gles2::GLES2Interface* gl,
                                         unsigned int texture,
                                         unsigned int internal_format,
                                         unsigned int type,
                                         bool premultiply_alpha,
                                         bool flip_y) override;

  // True if the loaded media has a playable video/audio track.
  bool hasVideo() const override;
  bool hasAudio() const override;

  bool isRemote() const override;

  // Dimensions of the video.
  blink::WebSize naturalSize() const override;

  // Getters of playback state.
  bool paused() const override;
  bool seeking() const override;
  double duration() const override;
  virtual double timelineOffset() const;
  double currentTime() const override;

  bool didLoadingProgress() override;

  // Internal states of loading and network.
  blink::WebMediaPlayer::NetworkState getNetworkState() const override;
  blink::WebMediaPlayer::ReadyState getReadyState() const override;

  blink::WebString getErrorMessage() override;

  bool hasSingleSecurityOrigin() const override;
  bool didPassCORSAccessCheck() const override;

  double mediaTimeForTimeValue(double timeValue) const override;

  // Provide statistics.
  unsigned decodedFrameCount() const override;
  unsigned droppedFrameCount() const override;
  size_t audioDecodedByteCount() const override;
  size_t videoDecodedByteCount() const override;

  // cc::VideoFrameProvider implementation. These methods are running on the
  // compositor thread.
  void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) override;
  bool UpdateCurrentFrame(base::TimeTicks deadline_min,
                          base::TimeTicks deadline_max) override;
  bool HasCurrentFrame() override;
  scoped_refptr<media::VideoFrame> GetCurrentFrame() override;
  void PutCurrentFrame() override;

  // Media player callback handlers.
  void OnMediaMetadataChanged(base::TimeDelta duration, int width,
                              int height, bool success) override;
  void OnPlaybackComplete() override;
  void OnBufferingUpdate(int percentage) override;
  void OnSeekRequest(const base::TimeDelta& time_to_seek) override;
  void OnSeekComplete(const base::TimeDelta& current_time) override;
  void OnMediaError(int error_type) override;
  void OnVideoSizeChanged(int width, int height) override;
  void OnDurationChanged(const base::TimeDelta& duration);

  // Called to update the current time.
  void OnTimeUpdate(base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks) override;

  // Functions called when media player status changes.
  void OnConnectedToRemoteDevice(const std::string& remote_playback_message)
      override;
  void OnDisconnectedFromRemoteDevice() override;
  void OnCancelledRemotePlaybackRequest() override;
  void OnDidExitFullscreen() override;
  void OnMediaPlayerPlay() override;
  void OnMediaPlayerPause() override;
  void OnRemoteRouteAvailabilityChanged(bool routes_available) override;

  // Called when the player is released.
  void OnPlayerReleased() override;

  // This function is called by the RendererMediaPlayerManager to pause the
  // video and release the media player and surface texture when we switch tabs.
  // However, the actual GlTexture is not released to keep the video screenshot.
  void SuspendAndReleaseResources() override;

#if defined(VIDEO_HOLE)
  // Calculate the boundary rectangle of the media player (i.e. location and
  // size of the video frame).
  // Returns true if the geometry has been changed since the last call.
  bool UpdateBoundaryRectangle() override;

  const gfx::RectF GetBoundaryRectangle() override;
#endif  // defined(VIDEO_HOLE)

  void setContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm,
      blink::WebContentDecryptionModuleResult result) override;

  void OnMediaSourceOpened(blink::WebMediaSource* web_media_source);

  void OnEncryptedMediaInitData(media::EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);

  // Called when a decoder detects that the key needed to decrypt the stream
  // is not available.
  void OnWaitingForDecryptionKey() override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnHidden() override;
  void OnShown() override;
  void OnSuspendRequested(bool must_suspend) override;
  void OnPlay() override;
  void OnPause() override;
  void OnVolumeMultiplierUpdate(double multiplier) override;

 protected:
  // Helper method to update the playing state.
  void UpdatePlayingState(bool is_playing_);

  // Helper methods for posting task for setting states and update WebKit.
  void UpdateNetworkState(blink::WebMediaPlayer::NetworkState state);
  void UpdateReadyState(blink::WebMediaPlayer::ReadyState state);
  void TryCreateStreamTextureProxyIfNeeded();
  void DoCreateStreamTexture();

  // Helper method to reestablish the surface texture peer for android
  // media player.
  void EstablishSurfaceTexturePeer();

  // Requesting whether the surface texture peer needs to be reestablished.
  void SetNeedsEstablishPeer(bool needs_establish_peer);

 private:
  void InitializePlayer(const GURL& url,
                        const GURL& first_party_for_cookies,
                        bool allowed_stored_credentials,
                        int demuxer_client_id);
  void Pause(bool is_media_related_action);
  void DrawRemotePlaybackText(const std::string& remote_playback_message);
  void ReallocateVideoFrame();
  void SetCurrentFrameInternal(scoped_refptr<media::VideoFrame>& frame);
  void RemoveSurfaceTextureAndProxy();
  void DidLoadMediaInfo(MediaInfoLoader::Status status,
                        const GURL& redirected_url,
                        const GURL& first_party_for_cookies,
                        bool allow_stored_credentials);
  bool IsKeySystemSupported(const std::string& key_system);
  bool IsLocalResource();

  // Called when |cdm_context| is ready.
  void OnCdmContextReady(media::CdmContext* cdm_context);

  // Sets the CDM. Should only be called when |is_player_initialized_| is true
  // and a new non-null |cdm_context_| is available. Fires |cdm_attached_cb_| on
  // the main thread with the result after the CDM is attached.
  void SetCdmInternal(const media::CdmAttachedCB& cdm_attached_cb);

  // Called when the CDM is attached.
  void OnCdmAttached(const media::CdmAttachedCB& cdm_attached_cb, bool success);

  // Requests that this object notifies when a CDM is ready through the
  // |cdm_ready_cb| provided.
  // If |cdm_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetCdmReadyCB(const MediaSourceDelegate::CdmReadyCB& cdm_ready_cb);

  // Called when the ContentDecryptionModule has been attached to the
  // pipeline/decoders.
  void ContentDecryptionModuleAttached(
      blink::WebContentDecryptionModuleResult result,
      bool success);

  bool IsHLSStream() const;
  // Report whether the loaded url, after following redirects, points to a HLS
  // playlist, and record the origin of the player.
  void ReportHLSMetrics() const;

  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type, const blink::WebURL& url, CORSMode cors_mode);

  blink::WebFrame* const frame_;

  blink::WebMediaPlayerClient* const client_;
  blink::WebMediaPlayerEncryptedMediaClient* const encrypted_client_;

  // WebMediaPlayer notifies the |delegate_| of playback state changes using
  // |delegate_id_|; an id provided after registering with the delegate.  The
  // WebMediaPlayer may also receive directives (play, pause) from the delegate
  // via the WebMediaPlayerDelegate::Observer interface after registration.
  base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;
  int delegate_id_;

  // Callback responsible for determining if loading of media should be deferred
  // for external reasons; called during load().
  media::WebMediaPlayerParams::DeferLoadCB defer_load_cb_;

  // Save the list of buffered time ranges.
  blink::WebTimeRanges buffered_;

  // Size of the video.
  blink::WebSize natural_size_;

  // Size that has been sent to gpu::StreamTexture.
  blink::WebSize cached_stream_texture_size_;

  // The video frame object used for rendering by the compositor.
  scoped_refptr<media::VideoFrame> current_frame_;
  base::Lock current_frame_lock_;

  // A lazily created transparent video frame to be displayed in fullscreen.
  scoped_refptr<media::VideoFrame> fullscreen_frame_;

  base::ThreadChecker main_thread_checker_;

  // Message loop for media thread.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // URL of the media file to be fetched.
  GURL url_;

  // URL of the media file after |media_info_loader_| resolves all the
  // redirections.
  GURL redirected_url_;

  // Media duration.
  base::TimeDelta duration_;

  // Flag to remember if we have a trusted duration_ value provided by
  // MediaSourceDelegate notifying OnDurationChanged(). In this case, ignore
  // any subsequent duration value passed to OnMediaMetadataChange().
  bool ignore_metadata_duration_change_;

  // Seek gets pending if another seek is in progress. Only last pending seek
  // will have effect.
  bool pending_seek_;
  base::TimeDelta pending_seek_time_;

  // Internal seek state.
  bool seeking_;
  base::TimeDelta seek_time_;

  // Whether loading has progressed since the last call to didLoadingProgress.
  bool did_loading_progress_;

  // Manages this object and delegates player calls to the browser process.
  // Owned by RenderFrameImpl.
  RendererMediaPlayerManager* const player_manager_;

  // Player ID assigned by the |player_manager_|.
  int player_id_;

  // User created media session id, if any.
  //
  // blink::WebMediaSession::DefaultID represents the non web
  // exposed default media session. User created session ids are
  // greater than blink::WebMediaSession::DefaultID.
  const int media_session_id_;

  // Current player states.
  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // GL texture ID allocated to the video.
  unsigned int texture_id_;

  // GL texture mailbox for texture_id_ to provide in the VideoFrame, and sync
  // point for when the mailbox was produced.
  gpu::Mailbox texture_mailbox_;

  // Stream texture ID allocated to the video.
  unsigned int stream_id_;

  // Whether the media player has been initialized.
  bool is_player_initialized_;

  // Whether the media player is playing.
  bool is_playing_;

  // Whether the media player is pending to play.
  bool is_play_pending_;

  // Whether media player needs to re-establish the surface texture peer.
  bool needs_establish_peer_;

  // Whether the video size info is available.
  bool has_size_info_;

  const scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Object for allocating stream textures.
  scoped_refptr<StreamTextureFactory> stream_texture_factory_;

  // Object for calling back the compositor thread to repaint the video when a
  // frame available. It should be initialized on the compositor thread.
  // Accessed on main thread and on compositor thread when main thread is
  // blocked.
  ScopedStreamTextureProxy stream_texture_proxy_;

  // Whether media player needs external surface.
  // Only used for the VIDEO_HOLE logic.
  bool needs_external_surface_;

  // Whether the player is in fullscreen.
  bool is_fullscreen_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  // Accessed on main thread and on compositor thread when main thread is
  // blocked.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  std::unique_ptr<cc_blink::WebLayerImpl> video_weblayer_;

#if defined(VIDEO_HOLE)
  // A rectangle represents the geometry of video frame, when computed last
  // time.
  gfx::RectF last_computed_rect_;

  // Whether to use the video overlay for all embedded video.
  // True only for testing.
  bool force_use_overlay_embedded_video_;
#endif  // defined(VIDEO_HOLE)

  MediaPlayerHostMsg_Initialize_Type player_type_;

  // Whether the browser is currently connected to a remote media player.
  bool is_remote_;

  scoped_refptr<media::MediaLog> media_log_;

  std::unique_ptr<MediaInfoLoader> info_loader_;

  // Non-owned pointer to the CdmContext. Updated in the constructor,
  // generateKeyRequest() or setContentDecryptionModule().
  media::CdmContext* cdm_context_;

  // This is only Used by Clear Key key system implementation, where a renderer
  // side CDM will be used. This is similar to WebMediaPlayerImpl. For other key
  // systems, a browser side CDM will be used and we set CDM by calling
  // player_manager_->SetCdm() directly.
  MediaSourceDelegate::CdmReadyCB cdm_ready_cb_;

  SkBitmap bitmap_;

  // Whether stored credentials are allowed to be passed to the server.
  bool allow_stored_credentials_;

  // Whether the resource is local.
  bool is_local_resource_;

  // base::TickClock used by |interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  media::TimeDeltaInterpolator interpolator_;

  std::unique_ptr<MediaSourceDelegate> media_source_delegate_;

  int frame_id_;

  // Whether to require that surface textures are copied in order to support
  // sharing between render and gpu threads in WebView.
  bool enable_texture_copy_;

  // Whether to delete the existing texture and re-create it.
  bool suppress_deleting_texture_;

  // Whether OnPlaybackComplete() has been called since the last playback.
  bool playback_completed_;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate().  The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;
  double volume_multiplier_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebMediaPlayerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
