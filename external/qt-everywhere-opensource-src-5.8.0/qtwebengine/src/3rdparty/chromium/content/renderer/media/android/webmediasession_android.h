// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIASESSION_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIASESSION_ANDROID_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/mediasession/WebMediaSession.h"

namespace content {

class RendererMediaSessionManager;

class CONTENT_EXPORT WebMediaSessionAndroid : public blink::WebMediaSession {
 public:
  WebMediaSessionAndroid(RendererMediaSessionManager* session_manager);
  ~WebMediaSessionAndroid() override;

  void activate(blink::WebMediaSessionActivateCallback*) override;
  void deactivate(blink::WebMediaSessionDeactivateCallback*) override;
  void setMetadata(const blink::WebMediaMetadata*) override;

  int media_session_id() const { return media_session_id_; }

 private:
  RendererMediaSessionManager* const session_manager_;
  int media_session_id_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaSessionAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_WEBMEDIASESSION_ANDROID_H_
