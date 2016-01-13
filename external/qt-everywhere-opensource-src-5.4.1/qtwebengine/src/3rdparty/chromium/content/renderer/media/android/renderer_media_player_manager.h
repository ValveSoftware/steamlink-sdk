// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_PLAYER_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "content/common/media/media_player_messages_enums_android.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/base/android/media_player_android.h"
#include "url/gurl.h"

namespace blink {
class WebFrame;
}

namespace gfx {
class RectF;
}

struct MediaPlayerHostMsg_Initialize_Params;

namespace content {
class WebMediaPlayerAndroid;

// Class for managing all the WebMediaPlayerAndroid objects in the same
// RenderFrame.
class RendererMediaPlayerManager : public RenderFrameObserver {
 public:
  // Constructs a RendererMediaPlayerManager object for the |render_frame|.
  explicit RendererMediaPlayerManager(RenderFrame* render_frame);
  virtual ~RendererMediaPlayerManager();

  // RenderFrameObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Initializes a MediaPlayerAndroid object in browser process.
  void Initialize(MediaPlayerHostMsg_Initialize_Type type,
                  int player_id,
                  const GURL& url,
                  const GURL& first_party_for_cookies,
                  int demuxer_client_id,
                  const GURL& frame_url,
                  bool allow_credentials);

  // Starts the player.
  void Start(int player_id);

  // Pauses the player.
  // is_media_related_action should be true if this pause is coming from an
  // an action that explicitly pauses the video (user pressing pause, JS, etc.)
  // Otherwise it should be false if Pause is being called due to other reasons
  // (cleanup, freeing resources, etc.)
  void Pause(int player_id, bool is_media_related_action);

  // Performs seek on the player.
  void Seek(int player_id, const base::TimeDelta& time);

  // Sets the player volume.
  void SetVolume(int player_id, double volume);

  // Sets the poster image.
  void SetPoster(int player_id, const GURL& poster);

  // Releases resources for the player.
  void ReleaseResources(int player_id);

  // Destroys the player in the browser process
  void DestroyPlayer(int player_id);

  // Requests the player to enter fullscreen.
  void EnterFullscreen(int player_id, blink::WebFrame* frame);

  // Requests the player to exit fullscreen.
  void ExitFullscreen(int player_id);

  // Requests the player with |player_id| to use the CDM with |cdm_id|.
  // Does nothing if |cdm_id| is kInvalidCdmId.
  // TODO(xhwang): Update this when we implement setCdm(0).
  void SetCdm(int player_id, int cdm_id);

#if defined(VIDEO_HOLE)
  // Requests an external surface for out-of-band compositing.
  void RequestExternalSurface(int player_id, const gfx::RectF& geometry);

  // RenderFrameObserver overrides.
  virtual void DidCommitCompositorFrame() OVERRIDE;

  // Returns true if a media player should use video-overlay for the embedded
  // encrypted video.
  bool ShouldUseVideoOverlayForEmbeddedEncryptedVideo();
#endif  // defined(VIDEO_HOLE)

  // Registers and unregisters a WebMediaPlayerAndroid object.
  int RegisterMediaPlayer(WebMediaPlayerAndroid* player);
  void UnregisterMediaPlayer(int player_id);

  // Checks whether a player can enter fullscreen.
  bool CanEnterFullscreen(blink::WebFrame* frame);

  // Called when a player entered or exited fullscreen.
  void DidEnterFullscreen(blink::WebFrame* frame);
  void DidExitFullscreen();

  // Checks whether the Webframe is in fullscreen.
  bool IsInFullscreen(blink::WebFrame* frame);

  // True if a newly created media player should enter fullscreen.
  bool ShouldEnterFullscreen(blink::WebFrame* frame);

  // Gets the pointer to WebMediaPlayerAndroid given the |player_id|.
  WebMediaPlayerAndroid* GetMediaPlayer(int player_id);

#if defined(VIDEO_HOLE)
  // Gets the list of media players with video geometry changes.
  void RetrieveGeometryChanges(std::map<int, gfx::RectF>* changes);
#endif  // defined(VIDEO_HOLE)

 private:
  // Message handlers.
  void OnMediaMetadataChanged(int player_id,
                              base::TimeDelta duration,
                              int width,
                              int height,
                              bool success);
  void OnMediaPlaybackCompleted(int player_id);
  void OnMediaBufferingUpdate(int player_id, int percent);
  void OnSeekRequest(int player_id, const base::TimeDelta& time_to_seek);
  void OnSeekCompleted(int player_id, const base::TimeDelta& current_time);
  void OnMediaError(int player_id, int error);
  void OnVideoSizeChanged(int player_id, int width, int height);
  void OnTimeUpdate(int player_id, base::TimeDelta current_time);
  void OnMediaPlayerReleased(int player_id);
  void OnConnectedToRemoteDevice(int player_id,
      const std::string& remote_playback_message);
  void OnDisconnectedFromRemoteDevice(int player_id);
  void OnDidExitFullscreen(int player_id);
  void OnDidEnterFullscreen(int player_id);
  void OnPlayerPlay(int player_id);
  void OnPlayerPause(int player_id);
  void OnRequestFullscreen(int player_id);
  void OnPauseVideo();

  // Release all video player resources.
  // If something is in progress the resource will not be freed. It will
  // only be freed once the tab is destroyed or if the user navigates away
  // via WebMediaPlayerAndroid::Destroy.
  void ReleaseVideoResources();

  // Info for all available WebMediaPlayerAndroid on a page; kept so that
  // we can enumerate them to send updates about tab focus and visibility.
  std::map<int, WebMediaPlayerAndroid*> media_players_;

  int next_media_player_id_;

  // WebFrame of the fullscreen video.
  blink::WebFrame* fullscreen_frame_;

  // WebFrame of pending fullscreen request.
  blink::WebFrame* pending_fullscreen_frame_;

  DISALLOW_COPY_AND_ASSIGN(RendererMediaPlayerManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_PLAYER_MANAGER_H_
