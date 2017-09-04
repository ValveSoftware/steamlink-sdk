// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ANDROID_APP_WEB_MESSAGE_PORT_CLIENT_H_
#define CONTENT_RENDERER_ANDROID_APP_WEB_MESSAGE_PORT_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/renderer/render_frame_observer.h"

namespace content {

// Renderer side of Android webview specific message port service. This service
// is used to convert messages from WebSerializedScriptValue to a base value.
class AppWebMessagePortClient : public content::RenderFrameObserver {
 public:
  explicit AppWebMessagePortClient(content::RenderFrame* render_frame);

 private:
  ~AppWebMessagePortClient() override;

  // RenderFrameObserver
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;

  void OnWebToAppMessage(int message_port_id,
                         const base::string16& message,
                         const std::vector<int>& sent_message_port_ids);
  void OnAppToWebMessage(int message_port_id,
                         const base::string16& message,
                         const std::vector<int>& sent_message_port_ids);
  void OnClosePort(int message_port_id);

  DISALLOW_COPY_AND_ASSIGN(AppWebMessagePortClient);
};
}

#endif  // CONTENT_RENDERER_ANDROID_APP_WEB_MESSAGE_PORT_CLIENT_H_
