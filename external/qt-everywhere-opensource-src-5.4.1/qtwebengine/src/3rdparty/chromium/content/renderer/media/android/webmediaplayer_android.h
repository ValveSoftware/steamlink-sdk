// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_

#include <jni.h>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/layers/video_frame_provider.h"
#include "content/common/media/media_player_messages_enums_android.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/renderer/media/android/media_info_loader.h"
#include "content/renderer/media/android/media_source_delegate.h"
#include "content/renderer/media/android/stream_texture_factory.h"
#include "content/renderer/media/crypto/proxy_decryptor.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "media/base/android/media_player_android.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "ui/gfx/rect_f.h"

namespace base {
class MessageLoopProxy;
}

namespace blink {
class WebContentDecryptionModule;
class WebFrame;
class WebURL;
}

namespace gpu {
struct MailboxHolder;
}

namespace media {
class MediaLog;
}

namespace content {
class RendererCdmManager;
class RendererMediaPlayerManager;
class WebContentDecryptionModuleImpl;
class WebLayerImpl;
class WebMediaPlayerDelegate;

// This class implements blink::WebMediaPlayer by keeping the android
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class WebMediaPlayerAndroid : public blink::WebMediaPlayer,
                              public cc::VideoFrameProvider,
                              public RenderFrameObserver {
 public:
  // Construct a WebMediaPlayerAndroid object. This class communicates with the
  // MediaPlayerAndroid object in the browser process through |proxy|.
  // TODO(qinmin): |frame| argument is used to determine whether the current
  // player can enter fullscreen. This logic should probably be moved into
  // blink, so that enterFullscreen() will not be called if another video is
  // already in fullscreen.
  WebMediaPlayerAndroid(blink::WebFrame* frame,
                        blink::WebMediaPlayerClient* client,
                        base::WeakPtr<WebMediaPlayerDelegate> delegate,
                        RendererMediaPlayerManager* player_manager,
                        RendererCdmManager* cdm_manager,
                        scoped_refptr<StreamTextureFactory> factory,
                        const scoped_refptr<base::MessageLoopProxy>& media_loop,
                        media::MediaLog* media_log);
  virtual ~WebMediaPlayerAndroid();

  // blink::WebMediaPlayer implementation.
  virtual void enterFullscreen();
  virtual void exitFullscreen();
  virtual bool canEnterFullscreen() const;

  // Resource loading.
  virtual void load(LoadType load_type,
                    const blink::WebURL& url,
                    CORSMode cors_mode);

  // Playback controls.
  virtual void play();
  virtual void pause();
  virtual void seek(double seconds);
  virtual bool supportsSave() const;
  virtual void setRate(double rate);
  virtual void setVolume(double volume);
  virtual blink::WebTimeRanges buffered() const;
  virtual double maxTimeSeekable() const;

  // Poster image, as defined in the <video> element.
  virtual void setPoster(const blink::WebURL& poster) OVERRIDE;

  // Methods for painting.
  virtual void paint(blink::WebCanvas* canvas,
                     const blink::WebRect& rect,
                     unsigned char alpha);

  virtual bool copyVideoTextureToPlatformTexture(
      blink::WebGraphicsContext3D* web_graphics_context,
      unsigned int texture,
      unsigned int level,
      unsigned int internal_format,
      unsigned int type,
      bool premultiply_alpha,
      bool flip_y);

  // True if the loaded media has a playable video/audio track.
  virtual bool hasVideo() const;
  virtual bool hasAudio() const;

  // Dimensions of the video.
  virtual blink::WebSize naturalSize() const;

  // Getters of playback state.
  virtual bool paused() const;
  virtual bool seeking() const;
  virtual double duration() const;
  virtual double timelineOffset() const;
  virtual double currentTime() const;

  virtual bool didLoadingProgress();

  // Internal states of loading and network.
  virtual blink::WebMediaPlayer::NetworkState networkState() const;
  virtual blink::WebMediaPlayer::ReadyState readyState() const;

  virtual bool hasSingleSecurityOrigin() const;
  virtual bool didPassCORSAccessCheck() const;

  virtual double mediaTimeForTimeValue(double timeValue) const;

  // Provide statistics.
  virtual unsigned decodedFrameCount() const;
  virtual unsigned droppedFrameCount() const;
  virtual unsigned audioDecodedByteCount() const;
  virtual unsigned videoDecodedByteCount() const;

  // cc::VideoFrameProvider implementation. These methods are running on the
  // compositor thread.
  virtual void SetVideoFrameProviderClient(
      cc::VideoFrameProvider::Client* client) OVERRIDE;
  virtual scoped_refptr<media::VideoFrame> GetCurrentFrame() OVERRIDE;
  virtual void PutCurrentFrame(const scoped_refptr<media::VideoFrame>& frame)
      OVERRIDE;

  // Media player callback handlers.
  void OnMediaMetadataChanged(const base::TimeDelta& duration, int width,
                              int height, bool success);
  void OnPlaybackComplete();
  void OnBufferingUpdate(int percentage);
  void OnSeekRequest(const base::TimeDelta& time_to_seek);
  void OnSeekComplete(const base::TimeDelta& current_time);
  void OnMediaError(int error_type);
  void OnVideoSizeChanged(int width, int height);
  void OnDurationChanged(const base::TimeDelta& duration);

  // Called to update the current time.
  void OnTimeUpdate(const base::TimeDelta& current_time);

  // Functions called when media player status changes.
  void OnConnectedToRemoteDevice(const std::string& remote_playback_message);
  void OnDisconnectedFromRemoteDevice();
  void OnDidEnterFullscreen();
  void OnDidExitFullscreen();
  void OnMediaPlayerPlay();
  void OnMediaPlayerPause();
  void OnRequestFullscreen();

  // Called when the player is released.
  virtual void OnPlayerReleased();

  // This function is called by the RendererMediaPlayerManager to pause the
  // video and release the media player and surface texture when we switch tabs.
  // However, the actual GlTexture is not released to keep the video screenshot.
  virtual void ReleaseMediaResources();

  // RenderFrameObserver implementation.
  virtual void OnDestruct() OVERRIDE;

#if defined(VIDEO_HOLE)
  // Calculate the boundary rectangle of the media player (i.e. location and
  // size of the video frame).
  // Returns true if the geometry has been changed since the last call.
  bool UpdateBoundaryRectangle();

  const gfx::RectF GetBoundaryRectangle();
#endif  // defined(VIDEO_HOLE)

  virtual MediaKeyException generateKeyRequest(
      const blink::WebString& key_system,
      const unsigned char* init_data,
      unsigned init_data_length);
  virtual MediaKeyException addKey(
      const blink::WebString& key_system,
      const unsigned char* key,
      unsigned key_length,
      const unsigned char* init_data,
      unsigned init_data_length,
      const blink::WebString& session_id);
  virtual MediaKeyException cancelKeyRequest(
      const blink::WebString& key_system,
      const blink::WebString& session_id);
  virtual void setContentDecryptionModule(
      blink::WebContentDecryptionModule* cdm);

  void OnKeyAdded(const std::string& session_id);
  void OnKeyError(const std::string& session_id,
                  media::MediaKeys::KeyError error_code,
                  uint32 system_code);
  void OnKeyMessage(const std::string& session_id,
                    const std::vector<uint8>& message,
                    const GURL& destination_url);

  void OnMediaSourceOpened(blink::WebMediaSource* web_media_source);

  void OnNeedKey(const std::string& type,
                 const std::vector<uint8>& init_data);

  // TODO(xhwang): Implement WebMediaPlayer::setContentDecryptionModule().
  // See: http://crbug.com/224786

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
  void DidLoadMediaInfo(MediaInfoLoader::Status status,
                        const GURL& redirected_url,
                        const GURL& first_party_for_cookies,
                        bool allow_stored_credentials);
  bool IsKeySystemSupported(const std::string& key_system);

  // Actually do the work for generateKeyRequest/addKey so they can easily
  // report results to UMA.
  MediaKeyException GenerateKeyRequestInternal(const std::string& key_system,
                                               const unsigned char* init_data,
                                               unsigned init_data_length);
  MediaKeyException AddKeyInternal(const std::string& key_system,
                                   const unsigned char* key,
                                   unsigned key_length,
                                   const unsigned char* init_data,
                                   unsigned init_data_length,
                                   const std::string& session_id);
  MediaKeyException CancelKeyRequestInternal(const std::string& key_system,
                                             const std::string& session_id);

  // Requests that this object notifies when a decryptor is ready through the
  // |decryptor_ready_cb| provided.
  // If |decryptor_ready_cb| is null, the existing callback will be fired with
  // NULL immediately and reset.
  void SetDecryptorReadyCB(const media::DecryptorReadyCB& decryptor_ready_cb);

  blink::WebFrame* const frame_;

  blink::WebMediaPlayerClient* const client_;

  // |delegate_| is used to notify the browser process of the player status, so
  // that the browser process can control screen locks.
  // TODO(qinmin): Currently android mediaplayer takes care of the screen
  // lock. So this is only used for media source. Will apply this to regular
  // media tag once http://crbug.com/247892 is fixed.
  base::WeakPtr<WebMediaPlayerDelegate> delegate_;

  // Save the list of buffered time ranges.
  blink::WebTimeRanges buffered_;

  // Size of the video.
  blink::WebSize natural_size_;

  // Size that has been sent to StreamTexture.
  blink::WebSize cached_stream_texture_size_;

  // The video frame object used for rendering by the compositor.
  scoped_refptr<media::VideoFrame> current_frame_;
  base::Lock current_frame_lock_;

  base::ThreadChecker main_thread_checker_;

  // Message loop for media thread.
  const scoped_refptr<base::MessageLoopProxy> media_loop_;

  // URL of the media file to be fetched.
  GURL url_;

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
  RendererMediaPlayerManager* player_manager_;

  // Delegates EME calls to the browser process. Owned by RenderFrameImpl.
  // TODO(xhwang): Remove |cdm_manager_| when prefixed EME is deprecated. See
  // http://crbug.com/249976
  RendererCdmManager* cdm_manager_;

  // Player ID assigned by the |player_manager_|.
  int player_id_;

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

  // Whether the mediaplayer is playing.
  bool is_playing_;

  // Whether media player needs to re-establish the surface texture peer.
  bool needs_establish_peer_;

  // Whether |stream_texture_proxy_| is initialized.
  bool stream_texture_proxy_initialized_;

  // Whether the video size info is available.
  bool has_size_info_;

  // Object for allocating stream textures.
  scoped_refptr<StreamTextureFactory> stream_texture_factory_;

  // Object for calling back the compositor thread to repaint the video when a
  // frame available. It should be initialized on the compositor thread.
  ScopedStreamTextureProxy stream_texture_proxy_;

  // Whether media player needs external surface.
  // Only used for the VIDEO_HOLE logic.
  bool needs_external_surface_;

  // A pointer back to the compositor to inform it about state changes. This is
  // not NULL while the compositor is actively using this webmediaplayer.
  cc::VideoFrameProvider::Client* video_frame_provider_client_;

  scoped_ptr<WebLayerImpl> video_weblayer_;

#if defined(VIDEO_HOLE)
  // A rectangle represents the geometry of video frame, when computed last
  // time.
  gfx::RectF last_computed_rect_;

  // Whether to use the video overlay for all embedded video.
  // True only for testing.
  bool force_use_overlay_embedded_video_;
#endif  // defined(VIDEO_HOLE)

  scoped_ptr<MediaSourceDelegate,
             MediaSourceDelegate::Destroyer> media_source_delegate_;

  // Internal pending playback state.
  // Store a playback request that cannot be started immediately.
  bool pending_playback_;

  MediaPlayerHostMsg_Initialize_Type player_type_;

  // The current playing time. Because the media player is in the browser
  // process, it will regularly update the |current_time_| by calling
  // OnTimeUpdate().
  double current_time_;

  // Whether the browser is currently connected to a remote media player.
  bool is_remote_;

  media::MediaLog* media_log_;

  scoped_ptr<MediaInfoLoader> info_loader_;

  // The currently selected key system. Empty string means that no key system
  // has been selected.
  std::string current_key_system_;

  // Temporary for EME v0.1. In the future the init data type should be passed
  // through GenerateKeyRequest() directly from WebKit.
  std::string init_data_type_;

  // Manages decryption keys and decrypts encrypted frames.
  scoped_ptr<ProxyDecryptor> proxy_decryptor_;

  // Non-owned pointer to the CDM. Updated via calls to
  // setContentDecryptionModule().
  WebContentDecryptionModuleImpl* web_cdm_;

  // This is only Used by Clear Key key system implementation, where a renderer
  // side CDM will be used. This is similar to WebMediaPlayerImpl. For other key
  // systems, a browser side CDM will be used and we set CDM by calling
  // player_manager_->SetCdm() directly.
  media::DecryptorReadyCB decryptor_ready_cb_;

  // Whether stored credentials are allowed to be passed to the server.
  bool allow_stored_credentials_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebMediaPlayerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIAPLAYER_ANDROID_H_
