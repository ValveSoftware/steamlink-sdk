// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_SESSION_MANAGER_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_SESSION_MANAGER_H_

#include <map>
#include <memory>

#include "base/id_map.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/mediasession/WebMediaSession.h"

namespace content {

class WebMediaSessionAndroid;
struct MediaMetadata;

class CONTENT_EXPORT RendererMediaSessionManager : public RenderFrameObserver {
 public:
  RendererMediaSessionManager(RenderFrame* render_frame);
  ~RendererMediaSessionManager() override;

  // RenderFrameObserver override.
  bool OnMessageReceived(const IPC::Message& msg) override;

  int RegisterMediaSession(WebMediaSessionAndroid* session);
  void UnregisterMediaSession(int session_id);

  void Activate(
      int session_id,
      std::unique_ptr<blink::WebMediaSessionActivateCallback> callback);
  void Deactivate(
      int session_id,
      std::unique_ptr<blink::WebMediaSessionDeactivateCallback> callback);
  void SetMetadata(int session_id, const MediaMetadata& metadata);

  void OnDidActivate(int request_id, bool success);
  void OnDidDeactivate(int request_id);

 private:
  friend class WebMediaSessionTest;

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  std::map<int, WebMediaSessionAndroid*> sessions_;
  int next_session_id_;

  using ActivationRequests =
      IDMap<blink::WebMediaSessionActivateCallback, IDMapOwnPointer>;
  ActivationRequests pending_activation_requests_;

  using DeactivationRequests =
      IDMap<blink::WebMediaSessionDeactivateCallback, IDMapOwnPointer>;
  DeactivationRequests pending_deactivation_requests_;

  DISALLOW_COPY_AND_ASSIGN(RendererMediaSessionManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_RENDERER_MEDIA_SESSION_MANAGER_H_
