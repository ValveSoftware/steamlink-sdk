// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediasession_android.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/common/media_metadata.h"
#include "content/renderer/media/android/renderer_media_session_manager.h"
#include "third_party/WebKit/public/platform/modules/mediasession/WebMediaMetadata.h"

namespace content {

WebMediaSessionAndroid::WebMediaSessionAndroid(
    RendererMediaSessionManager* session_manager)
    : session_manager_(session_manager) {
  DCHECK(session_manager_);
  media_session_id_ = session_manager_->RegisterMediaSession(this);
}

WebMediaSessionAndroid::~WebMediaSessionAndroid() {
  session_manager_->UnregisterMediaSession(media_session_id_);
}

void WebMediaSessionAndroid::activate(
    blink::WebMediaSessionActivateCallback* callback) {
  session_manager_->Activate(media_session_id_, base::WrapUnique(callback));
}

void WebMediaSessionAndroid::deactivate(
    blink::WebMediaSessionDeactivateCallback* callback) {
  session_manager_->Deactivate(media_session_id_, base::WrapUnique(callback));
}

void WebMediaSessionAndroid::setMetadata(
    const blink::WebMediaMetadata* web_metadata) {
  MediaMetadata metadata;
  if (web_metadata) {
    metadata.title = web_metadata->title;
    metadata.artist = web_metadata->artist;
    metadata.album = web_metadata->album;
  }

  session_manager_->SetMetadata(media_session_id_, metadata);
}

}  // namespace content
