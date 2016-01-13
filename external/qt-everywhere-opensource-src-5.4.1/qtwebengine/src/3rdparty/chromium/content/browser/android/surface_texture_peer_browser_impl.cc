// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/surface_texture_peer_browser_impl.h"

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/android/media_player_android.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace content {

namespace {

// Pass a java surface object to the MediaPlayerAndroid object
// identified by render process handle, render frame ID and player ID.
static void SetSurfacePeer(
    scoped_refptr<gfx::SurfaceTexture> surface_texture,
    base::ProcessHandle render_process_handle,
    int render_frame_id,
    int player_id) {
  int render_process_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      render_process_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }
  if (!render_process_id) {
    DVLOG(1) << "Cannot find render process for render_process_handle "
             << render_process_handle;
    return;
  }

  RenderFrameHostImpl* frame =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (!frame) {
    DVLOG(1) << "Cannot find frame for render_frame_id " << render_frame_id;
    return;
  }

  RenderViewHostImpl* view =
      static_cast<RenderViewHostImpl*>(frame->GetRenderViewHost());
  BrowserMediaPlayerManager* player_manager =
      view->media_web_contents_observer()->GetMediaPlayerManager(frame);
  if (!player_manager) {
    DVLOG(1) << "Cannot find the media player manager for frame " << frame;
    return;
  }

  media::MediaPlayerAndroid* player = player_manager->GetPlayer(player_id);
  if (!player) {
    DVLOG(1) << "Cannot find media player for player_id " << player_id;
    return;
  }

  if (player != player_manager->GetFullscreenPlayer()) {
    gfx::ScopedJavaSurface scoped_surface(surface_texture);
    player->SetVideoSurface(scoped_surface.Pass());
  }
}

}  // anonymous namespace

SurfaceTexturePeerBrowserImpl::SurfaceTexturePeerBrowserImpl() {
}

SurfaceTexturePeerBrowserImpl::~SurfaceTexturePeerBrowserImpl() {
}

void SurfaceTexturePeerBrowserImpl::EstablishSurfaceTexturePeer(
    base::ProcessHandle render_process_handle,
    scoped_refptr<gfx::SurfaceTexture> surface_texture,
    int render_frame_id,
    int player_id) {
  if (!surface_texture.get())
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &SetSurfacePeer, surface_texture, render_process_handle,
      render_frame_id, player_id));
}

}  // namespace content
