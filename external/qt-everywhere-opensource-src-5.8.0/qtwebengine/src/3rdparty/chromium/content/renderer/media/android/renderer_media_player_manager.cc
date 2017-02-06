// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/renderer_media_player_manager.h"

#include "base/command_line.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/public/common/renderer_preferences.h"
#include "content/renderer/media/android/webmediaplayer_android.h"
#include "content/renderer/media/cdm/renderer_cdm_manager.h"
#include "content/renderer/render_view_impl.h"
#include "media/base/cdm_context.h"
#include "media/base/media_switches.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content {

RendererMediaPlayerManager::RendererMediaPlayerManager(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      next_media_player_id_(0) {
}

RendererMediaPlayerManager::~RendererMediaPlayerManager() {
  DCHECK(media_players_.empty())
      << "RendererMediaPlayerManager is owned by RenderFrameImpl and is "
         "destroyed only after all media players are destroyed.";
}

bool RendererMediaPlayerManager::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererMediaPlayerManager, msg)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaMetadataChanged,
                        OnMediaMetadataChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaPlaybackCompleted,
                        OnMediaPlaybackCompleted)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaBufferingUpdate,
                        OnMediaBufferingUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_SeekRequest, OnSeekRequest)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_SeekCompleted, OnSeekCompleted)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaError, OnMediaError)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaVideoSizeChanged,
                        OnVideoSizeChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaTimeUpdate, OnTimeUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_WaitingForDecryptionKey,
                        OnWaitingForDecryptionKey)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_MediaPlayerReleased,
                        OnMediaPlayerReleased)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_ConnectedToRemoteDevice,
                        OnConnectedToRemoteDevice)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DisconnectedFromRemoteDevice,
                        OnDisconnectedFromRemoteDevice)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_CancelledRemotePlaybackRequest,
                        OnCancelledRemotePlaybackRequest)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidExitFullscreen, OnDidExitFullscreen)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidMediaPlayerPlay, OnPlayerPlay)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_DidMediaPlayerPause, OnPlayerPause)
    IPC_MESSAGE_HANDLER(MediaPlayerMsg_RemoteRouteAvailabilityChanged,
                        OnRemoteRouteAvailabilityChanged)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RendererMediaPlayerManager::Initialize(
    MediaPlayerHostMsg_Initialize_Type type,
    int player_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    int demuxer_client_id,
    const GURL& frame_url,
    bool allow_credentials,
    int delegate_id,
    int media_session_id) {
  MediaPlayerHostMsg_Initialize_Params media_player_params;
  media_player_params.type = type;
  media_player_params.player_id = player_id;
  media_player_params.demuxer_client_id = demuxer_client_id;
  media_player_params.url = url;
  media_player_params.first_party_for_cookies = first_party_for_cookies;
  media_player_params.frame_url = frame_url;
  media_player_params.allow_credentials = allow_credentials;
  media_player_params.delegate_id = delegate_id;
  media_player_params.media_session_id = media_session_id;

  Send(new MediaPlayerHostMsg_Initialize(routing_id(), media_player_params));
}

void RendererMediaPlayerManager::Start(int player_id) {
  Send(new MediaPlayerHostMsg_Start(routing_id(), player_id));
}

void RendererMediaPlayerManager::Pause(
    int player_id,
    bool is_media_related_action) {
  Send(new MediaPlayerHostMsg_Pause(
      routing_id(), player_id, is_media_related_action));
}

void RendererMediaPlayerManager::Seek(
    int player_id,
    const base::TimeDelta& time) {
  Send(new MediaPlayerHostMsg_Seek(routing_id(), player_id, time));
}

void RendererMediaPlayerManager::SetVolume(int player_id, double volume) {
  Send(new MediaPlayerHostMsg_SetVolume(routing_id(), player_id, volume));
}

void RendererMediaPlayerManager::SetPoster(int player_id, const GURL& poster) {
  Send(new MediaPlayerHostMsg_SetPoster(routing_id(), player_id, poster));
}

void RendererMediaPlayerManager::SuspendAndReleaseResources(int player_id) {
  Send(new MediaPlayerHostMsg_SuspendAndRelease(routing_id(), player_id));
}

void RendererMediaPlayerManager::DestroyPlayer(int player_id) {
  Send(new MediaPlayerHostMsg_DestroyMediaPlayer(routing_id(), player_id));
}

void RendererMediaPlayerManager::RequestRemotePlayback(int player_id) {
  Send(new MediaPlayerHostMsg_RequestRemotePlayback(routing_id(), player_id));
}

void RendererMediaPlayerManager::RequestRemotePlaybackControl(int player_id) {
  Send(new MediaPlayerHostMsg_RequestRemotePlaybackControl(routing_id(),
                                                           player_id));
}

void RendererMediaPlayerManager::OnMediaMetadataChanged(
    int player_id,
    base::TimeDelta duration,
    int width,
    int height,
    bool success) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaMetadataChanged(duration, width, height, success);
}

void RendererMediaPlayerManager::OnMediaPlaybackCompleted(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPlaybackComplete();
}

void RendererMediaPlayerManager::OnMediaBufferingUpdate(int player_id,
                                                        int percent) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnBufferingUpdate(percent);
}

void RendererMediaPlayerManager::OnSeekRequest(
    int player_id,
    const base::TimeDelta& time_to_seek) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekRequest(time_to_seek);
}

void RendererMediaPlayerManager::OnSeekCompleted(
    int player_id,
    const base::TimeDelta& current_time) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete(current_time);
}

void RendererMediaPlayerManager::OnMediaError(int player_id, int error) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaError(error);
}

void RendererMediaPlayerManager::OnVideoSizeChanged(int player_id,
                                                    int width,
                                                    int height) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnVideoSizeChanged(width, height);
}

void RendererMediaPlayerManager::OnTimeUpdate(
    int player_id,
    base::TimeDelta current_timestamp,
    base::TimeTicks current_time_ticks) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_timestamp, current_time_ticks);
}

void RendererMediaPlayerManager::OnWaitingForDecryptionKey(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnWaitingForDecryptionKey();
}

void RendererMediaPlayerManager::OnMediaPlayerReleased(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPlayerReleased();
}

void RendererMediaPlayerManager::OnConnectedToRemoteDevice(int player_id,
    const std::string& remote_playback_message) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnConnectedToRemoteDevice(remote_playback_message);
}

void RendererMediaPlayerManager::OnDisconnectedFromRemoteDevice(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDisconnectedFromRemoteDevice();
}

void RendererMediaPlayerManager::OnCancelledRemotePlaybackRequest(
    int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnCancelledRemotePlaybackRequest();
}

void RendererMediaPlayerManager::OnDidExitFullscreen(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDidExitFullscreen();
}

void RendererMediaPlayerManager::OnPlayerPlay(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPlay();
}

void RendererMediaPlayerManager::OnPlayerPause(int player_id) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaPlayerPause();
}

void RendererMediaPlayerManager::OnRemoteRouteAvailabilityChanged(
    int player_id,
    bool routes_available) {
  media::RendererMediaPlayerInterface* player = GetMediaPlayer(player_id);
  if (player)
    player->OnRemoteRouteAvailabilityChanged(routes_available);
}

void RendererMediaPlayerManager::EnterFullscreen(int player_id) {
  Send(new MediaPlayerHostMsg_EnterFullscreen(routing_id(), player_id));
}

void RendererMediaPlayerManager::SetCdm(int player_id, int cdm_id) {
  if (cdm_id == media::CdmContext::kInvalidCdmId) {
    NOTREACHED();
    return;
  }
  Send(new MediaPlayerHostMsg_SetCdm(routing_id(), player_id, cdm_id));
}

int RendererMediaPlayerManager::RegisterMediaPlayer(
    media::RendererMediaPlayerInterface* player) {
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_++;
}

void RendererMediaPlayerManager::UnregisterMediaPlayer(int player_id) {
  media_players_.erase(player_id);
}

media::RendererMediaPlayerInterface* RendererMediaPlayerManager::GetMediaPlayer(
    int player_id) {
  std::map<int, media::RendererMediaPlayerInterface*>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return iter->second;
  return NULL;
}

void RendererMediaPlayerManager::OnDestruct() {
  delete this;
}

#if defined(VIDEO_HOLE)
void RendererMediaPlayerManager::RequestExternalSurface(
    int player_id,
    const gfx::RectF& geometry) {
  Send(new MediaPlayerHostMsg_NotifyExternalSurface(
      routing_id(), player_id, true, geometry));
}

void RendererMediaPlayerManager::DidCommitCompositorFrame() {
  std::map<int, gfx::RectF> geometry_change;
  RetrieveGeometryChanges(&geometry_change);
  for (std::map<int, gfx::RectF>::iterator it = geometry_change.begin();
       it != geometry_change.end();
       ++it) {
    Send(new MediaPlayerHostMsg_NotifyExternalSurface(
        routing_id(), it->first, false, it->second));
  }
}

void RendererMediaPlayerManager::RetrieveGeometryChanges(
    std::map<int, gfx::RectF>* changes) {
  DCHECK(changes->empty());
  for (std::map<int, media::RendererMediaPlayerInterface*>::iterator player_it =
           media_players_.begin();
       player_it != media_players_.end();
       ++player_it) {
    media::RendererMediaPlayerInterface* player = player_it->second;

    if (player && player->hasVideo()) {
      if (player->UpdateBoundaryRectangle())
        (*changes)[player_it->first] = player->GetBoundaryRectangle();
    }
  }
}

bool
RendererMediaPlayerManager::ShouldUseVideoOverlayForEmbeddedEncryptedVideo() {
  const RendererPreferences& prefs = static_cast<RenderFrameImpl*>(
      render_frame())->render_view()->renderer_preferences();
  return prefs.use_video_overlay_for_embedded_encrypted_video;
}
#endif  // defined(VIDEO_HOLE)

}  // namespace content
