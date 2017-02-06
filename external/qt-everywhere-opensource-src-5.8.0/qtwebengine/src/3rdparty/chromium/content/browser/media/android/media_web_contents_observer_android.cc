// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_web_contents_observer_android.h"

#include "base/memory/ptr_util.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/android/browser_media_session_manager.h"
#include "content/browser/media/android/browser_surface_view_manager.h"
#include "content/browser/media/cdm/browser_cdm_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/common/media/media_player_messages_android.h"
#include "content/common/media/media_session_messages_android.h"
#include "content/common/media/surface_view_manager_messages_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/android/media_player_android.h"

namespace content {

static void SuspendAllMediaPlayersInRenderFrame(
    RenderFrameHost* render_frame_host) {
  render_frame_host->Send(new MediaPlayerDelegateMsg_SuspendAllMediaPlayers(
      render_frame_host->GetRoutingID()));
}

MediaWebContentsObserverAndroid::MediaWebContentsObserverAndroid(
    WebContents* web_contents)
    : MediaWebContentsObserver(web_contents) {}

MediaWebContentsObserverAndroid::~MediaWebContentsObserverAndroid() {}

// static
MediaWebContentsObserverAndroid*
MediaWebContentsObserverAndroid::FromWebContents(WebContents* web_contents) {
  return static_cast<MediaWebContentsObserverAndroid*>(
      static_cast<WebContentsImpl*>(web_contents)
          ->media_web_contents_observer());
}

BrowserMediaPlayerManager*
MediaWebContentsObserverAndroid::GetMediaPlayerManager(
    RenderFrameHost* render_frame_host) {
  auto it = media_player_managers_.find(render_frame_host);
  if (it != media_player_managers_.end())
    return it->second;

  BrowserMediaPlayerManager* manager =
      BrowserMediaPlayerManager::Create(render_frame_host);
  media_player_managers_.set(render_frame_host, base::WrapUnique(manager));
  return manager;
}

BrowserMediaSessionManager*
MediaWebContentsObserverAndroid::GetMediaSessionManager(
    RenderFrameHost* render_frame_host) {
  auto it = media_session_managers_.find(render_frame_host);
  if (it != media_session_managers_.end())
    return it->second;

  BrowserMediaSessionManager* manager =
      new BrowserMediaSessionManager(render_frame_host);
  media_session_managers_.set(render_frame_host, base::WrapUnique(manager));
  return manager;
}

BrowserSurfaceViewManager*
MediaWebContentsObserverAndroid::GetSurfaceViewManager(
    RenderFrameHost* render_frame_host) {
  auto it = surface_view_managers_.find(render_frame_host);
  if (it != surface_view_managers_.end())
    return it->second;

  BrowserSurfaceViewManager* manager =
      new BrowserSurfaceViewManager(render_frame_host);
  surface_view_managers_.set(render_frame_host, base::WrapUnique(manager));
  return manager;
}

void MediaWebContentsObserverAndroid::SuspendAllMediaPlayers() {
  web_contents()->ForEachFrame(
      base::Bind(&SuspendAllMediaPlayersInRenderFrame));
}

bool MediaWebContentsObserverAndroid::RequestPlay(
    RenderFrameHost* render_frame_host,
    int delegate_id,
    bool has_audio,
    bool is_remote,
    base::TimeDelta duration) {
  return session_controllers_manager()->RequestPlay(
      MediaPlayerId(render_frame_host, delegate_id),
      has_audio, is_remote, duration);
}

void MediaWebContentsObserverAndroid::DisconnectMediaSession(
    RenderFrameHost* render_frame_host,
    int delegate_id) {
  session_controllers_manager()->OnEnd(
      MediaPlayerId(render_frame_host, delegate_id));
}

#if defined(VIDEO_HOLE)
void MediaWebContentsObserverAndroid::OnFrameInfoUpdated() {
  for (auto it = media_player_managers_.begin();
       it != media_player_managers_.end(); ++it) {
    it->second->OnFrameInfoUpdated();
  }
}
#endif  // defined(VIDEO_HOLE)

void MediaWebContentsObserverAndroid::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  MediaWebContentsObserver::RenderFrameDeleted(render_frame_host);

  // Always destroy the media players before CDMs because we do not support
  // detaching CDMs from media players yet. See http://crbug.com/330324
  media_player_managers_.erase(render_frame_host);
  media_session_managers_.erase(render_frame_host);
  surface_view_managers_.erase(render_frame_host);

  // TODO(xhwang): Currently MediaWebContentsObserver, BrowserMediaPlayerManager
  // and BrowserCdmManager all run on browser UI thread. So this call is okay.
  // In the future we need to support the case where MediaWebContentsObserver
  // get notified on browser UI thread, but BrowserMediaPlayerManager and
  // BrowserCdmManager run on a different thread.
  BrowserCdmManager* browser_cdm_manager =
      BrowserCdmManager::FromProcess(render_frame_host->GetProcess()->GetID());
  if (browser_cdm_manager)
    browser_cdm_manager->RenderFrameDeleted(render_frame_host->GetRoutingID());
}

bool MediaWebContentsObserverAndroid::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  if (MediaWebContentsObserver::OnMessageReceived(msg, render_frame_host))
    return true;

  if (OnMediaPlayerMessageReceived(msg, render_frame_host))
    return true;

  if (OnMediaPlayerSetCdmMessageReceived(msg, render_frame_host))
    return true;

  if (OnMediaSessionMessageReceived(msg, render_frame_host))
    return true;

  if (OnSurfaceViewManagerMessageReceived(msg, render_frame_host))
    return true;

  return false;
}

bool MediaWebContentsObserverAndroid::OnMediaPlayerMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserverAndroid, msg)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_EnterFullscreen,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnEnterFullscreen)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Initialize,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnInitialize)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Start,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnStart)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Seek,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSeek)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Pause,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnPause)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetVolume,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSetVolume)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SetPoster,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSetPoster)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_SuspendAndRelease,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnSuspendAndReleaseResources)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_DestroyMediaPlayer,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnDestroyPlayer)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_RequestRemotePlayback,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnRequestRemotePlayback)
    IPC_MESSAGE_FORWARD(
        MediaPlayerHostMsg_RequestRemotePlaybackControl,
        GetMediaPlayerManager(render_frame_host),
        BrowserMediaPlayerManager::OnRequestRemotePlaybackControl)
#if defined(VIDEO_HOLE)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_NotifyExternalSurface,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnNotifyExternalSurface)
#endif  // defined(VIDEO_HOLE)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool MediaWebContentsObserverAndroid::OnMediaPlayerSetCdmMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(MediaWebContentsObserverAndroid, msg,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_SetCdm, OnSetCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool MediaWebContentsObserverAndroid::OnMediaSessionMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserver, msg)
    IPC_MESSAGE_FORWARD(MediaSessionHostMsg_Activate,
                        GetMediaSessionManager(render_frame_host),
                        BrowserMediaSessionManager::OnActivate)
    IPC_MESSAGE_FORWARD(MediaSessionHostMsg_Deactivate,
                        GetMediaSessionManager(render_frame_host),
                        BrowserMediaSessionManager::OnDeactivate)
    IPC_MESSAGE_FORWARD(MediaSessionHostMsg_SetMetadata,
                        GetMediaSessionManager(render_frame_host),
                        BrowserMediaSessionManager::OnSetMetadata)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool MediaWebContentsObserverAndroid::OnSurfaceViewManagerMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserverAndroid, msg)
    IPC_MESSAGE_FORWARD(SurfaceViewManagerHostMsg_CreateFullscreenSurface,
                        GetSurfaceViewManager(render_frame_host),
                        BrowserSurfaceViewManager::OnCreateFullscreenSurface)
    IPC_MESSAGE_FORWARD(SurfaceViewManagerHostMsg_NaturalSizeChanged,
                        GetSurfaceViewManager(render_frame_host),
                        BrowserSurfaceViewManager::OnNaturalSizeChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserverAndroid::OnSetCdm(
    RenderFrameHost* render_frame_host,
    int player_id,
    int cdm_id) {
  media::MediaPlayerAndroid* media_player =
      GetMediaPlayerManager(render_frame_host)->GetPlayer(player_id);
  if (!media_player) {
    NOTREACHED() << "OnSetCdm: MediaPlayer not found for " << player_id;
    return;
  }

  // MediaPlayerAndroid runs on the same thread as BrowserCdmManager.
  BrowserCdmManager* browser_cdm_manager =
      BrowserCdmManager::FromProcess(render_frame_host->GetProcess()->GetID());
  if (!browser_cdm_manager) {
    NOTREACHED() << "OnSetCdm: CDM not found for " << cdm_id;
    return;
  }

  scoped_refptr<media::MediaKeys> cdm =
      browser_cdm_manager->GetCdm(render_frame_host->GetRoutingID(), cdm_id);
  if (!cdm) {
    NOTREACHED() << "OnSetCdm: CDM not found for " << cdm_id;
    return;
  }

  // TODO(xhwang): This could possibly fail. In that case we should reject the
  // promise.
  media_player->SetCdm(cdm);
}

}  // namespace content
