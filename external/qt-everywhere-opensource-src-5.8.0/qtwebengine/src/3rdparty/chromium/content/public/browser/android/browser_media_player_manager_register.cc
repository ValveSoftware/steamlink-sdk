// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/browser_media_player_manager_register.h"

#include "content/browser/media/android/browser_media_player_manager.h"

namespace content {

void RegisterMediaUrlInterceptor(
    media::MediaUrlInterceptor* media_url_interceptor) {
  content::BrowserMediaPlayerManager::RegisterMediaUrlInterceptor(
      media_url_interceptor);
}

}  // namespace content
