// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class DevToolsFrontendHostImpl : public DevToolsFrontendHost,
                                 public WebContentsObserver {
 public:
  DevToolsFrontendHostImpl(
      RenderFrameHost* frontend_main_frame,
      const HandleMessageCallback& handle_message_callback);
  ~DevToolsFrontendHostImpl() override;

  void BadMessageRecieved() override;

 private:
  // WebContentsObserver overrides.
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  void OnDispatchOnEmbedder(const std::string& message);

  HandleMessageCallback handle_message_callback_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_
