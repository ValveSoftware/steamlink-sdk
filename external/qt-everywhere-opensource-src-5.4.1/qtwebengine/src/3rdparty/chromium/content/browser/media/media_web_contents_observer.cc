// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_web_contents_observer.h"

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/media/cdm/browser_cdm_manager.h"
#include "content/common/media/cdm_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message_macros.h"

#if defined(OS_ANDROID)
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/common/media/media_player_messages_android.h"
#include "media/base/android/media_player_android.h"
#endif  // defined(OS_ANDROID)

namespace content {

MediaWebContentsObserver::MediaWebContentsObserver(
    RenderViewHost* render_view_host)
    : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)) {
}

MediaWebContentsObserver::~MediaWebContentsObserver() {
}

void MediaWebContentsObserver::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  uintptr_t key = reinterpret_cast<uintptr_t>(render_frame_host);
  // Always destroy the media players before CDMs because we do not support
  // detaching CDMs from media players yet. See http://crbug.com/330324
#if defined(OS_ANDROID)
  media_player_managers_.erase(key);
#endif
  cdm_managers_.erase(key);
}

bool MediaWebContentsObserver::OnMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
#if defined(OS_ANDROID)
  // Handles MediaPlayer messages first because MediaPlayers messages are much
  // more frequent than CDM messages.
  if (OnMediaPlayerMessageReceived(msg, render_frame_host))
    return true;

  if (OnMediaPlayerSetCdmMessageReceived(msg, render_frame_host))
    return true;
#endif  // defined(OS_ANDROID)

  if (OnCdmMessageReceived(msg, render_frame_host))
    return true;

  return false;
}

bool MediaWebContentsObserver::OnCdmMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserver, msg)
    IPC_MESSAGE_FORWARD(CdmHostMsg_InitializeCdm,
                        GetCdmManager(render_frame_host),
                        BrowserCdmManager::OnInitializeCdm)
    IPC_MESSAGE_FORWARD(CdmHostMsg_CreateSession,
                        GetCdmManager(render_frame_host),
                        BrowserCdmManager::OnCreateSession)
    IPC_MESSAGE_FORWARD(CdmHostMsg_UpdateSession,
                        GetCdmManager(render_frame_host),
                        BrowserCdmManager::OnUpdateSession)
    IPC_MESSAGE_FORWARD(CdmHostMsg_ReleaseSession,
                        GetCdmManager(render_frame_host),
                        BrowserCdmManager::OnReleaseSession)
    IPC_MESSAGE_FORWARD(CdmHostMsg_DestroyCdm,
                        GetCdmManager(render_frame_host),
                        BrowserCdmManager::OnDestroyCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

BrowserCdmManager* MediaWebContentsObserver::GetCdmManager(
    RenderFrameHost* render_frame_host) {
  uintptr_t key = reinterpret_cast<uintptr_t>(render_frame_host);
  if (!cdm_managers_.contains(key)) {
    cdm_managers_.set(
        key, make_scoped_ptr(BrowserCdmManager::Create(render_frame_host)));
  }
  return cdm_managers_.get(key);
}

#if defined(OS_ANDROID)
bool MediaWebContentsObserver::OnMediaPlayerMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaWebContentsObserver, msg)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_EnterFullscreen,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnEnterFullscreen)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_ExitFullscreen,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnExitFullscreen)
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
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_Release,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnReleaseResources)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_DestroyMediaPlayer,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnDestroyPlayer)
#if defined(VIDEO_HOLE)
    IPC_MESSAGE_FORWARD(MediaPlayerHostMsg_NotifyExternalSurface,
                        GetMediaPlayerManager(render_frame_host),
                        BrowserMediaPlayerManager::OnNotifyExternalSurface)
#endif  // defined(VIDEO_HOLE)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool MediaWebContentsObserver::OnMediaPlayerSetCdmMessageReceived(
    const IPC::Message& msg,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(
      MediaWebContentsObserver, msg, render_frame_host)
    IPC_MESSAGE_HANDLER(MediaPlayerHostMsg_SetCdm, OnSetCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaWebContentsObserver::OnSetCdm(RenderFrameHost* render_frame_host,
                                        int player_id,
                                        int cdm_id) {
  media::MediaPlayerAndroid* media_player =
      GetMediaPlayerManager(render_frame_host)->GetPlayer(player_id);
  if (!media_player) {
    NOTREACHED() << "OnSetCdm: MediaPlayer not found for " << player_id;
    return;
  }

  media::BrowserCdm* cdm = GetCdmManager(render_frame_host)->GetCdm(cdm_id);
  if (!cdm) {
    NOTREACHED() << "OnSetCdm: CDM not found for " << cdm_id;
    return;
  }

  // TODO(xhwang): This could possibly fail. In that case we should reject the
  // promise.
  media_player->SetCdm(cdm);
}

BrowserMediaPlayerManager* MediaWebContentsObserver::GetMediaPlayerManager(
    RenderFrameHost* render_frame_host) {
  uintptr_t key = reinterpret_cast<uintptr_t>(render_frame_host);
  if (!media_player_managers_.contains(key)) {
    media_player_managers_.set(
        key,
        make_scoped_ptr(BrowserMediaPlayerManager::Create(render_frame_host)));
  }
  return media_player_managers_.get(key);
}

void MediaWebContentsObserver::PauseVideo() {
  for (MediaPlayerManagerMap::iterator iter = media_player_managers_.begin();
      iter != media_player_managers_.end(); ++iter) {
    BrowserMediaPlayerManager* manager = iter->second;
    manager->PauseVideo();
  }
}

#if defined(VIDEO_HOLE)
void MediaWebContentsObserver::OnFrameInfoUpdated() {
  for (MediaPlayerManagerMap::iterator iter = media_player_managers_.begin();
      iter != media_player_managers_.end(); ++iter) {
    BrowserMediaPlayerManager* manager = iter->second;
    manager->OnFrameInfoUpdated();
  }
}
#endif  // defined(VIDEO_HOLE)

#endif  // defined(OS_ANDROID)

}  // namespace content
