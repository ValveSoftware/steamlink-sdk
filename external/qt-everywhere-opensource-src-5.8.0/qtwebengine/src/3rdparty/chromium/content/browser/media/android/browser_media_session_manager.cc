// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_session_manager.h"

#include "content/common/media/media_session_messages_android.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/media_metadata.h"

namespace content {

BrowserMediaSessionManager::BrowserMediaSessionManager(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

void BrowserMediaSessionManager::OnActivate(int session_id, int request_id) {
  NOTIMPLEMENTED();
  Send(new MediaSessionMsg_DidActivate(GetRoutingID(), request_id, false));
}

void BrowserMediaSessionManager::OnDeactivate(int session_id, int request_id) {
  NOTIMPLEMENTED();
  Send(new MediaSessionMsg_DidDeactivate(GetRoutingID(), request_id));
}

void BrowserMediaSessionManager::OnSetMetadata(
    int session_id,
    const MediaMetadata& insecure_metadata) {
  // When receiving a MediaMetadata, the browser process can't trust that it is
  // coming from a known and secure source. It must be processed accordingly.
  MediaMetadata metadata;
  metadata.title =
      insecure_metadata.title.substr(0, MediaMetadata::kMaxIPCStringLength);
  metadata.artist =
      insecure_metadata.artist.substr(0, MediaMetadata::kMaxIPCStringLength);
  metadata.album =
      insecure_metadata.album.substr(0, MediaMetadata::kMaxIPCStringLength);

  if (metadata != insecure_metadata) {
    render_frame_host_->GetProcess()->ShutdownForBadMessage();
    return;
  }

  NOTIMPLEMENTED();
}

int BrowserMediaSessionManager::GetRoutingID() const {
  return render_frame_host_->GetRoutingID();
}

bool BrowserMediaSessionManager::Send(IPC::Message* msg) {
  return render_frame_host_->Send(msg);
}

}  // namespace content
