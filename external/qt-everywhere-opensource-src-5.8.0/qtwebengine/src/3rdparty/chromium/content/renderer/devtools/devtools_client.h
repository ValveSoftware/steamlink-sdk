// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/web/WebDevToolsFrontendClient.h"

namespace blink {
class WebDevToolsFrontend;
class WebString;
}

namespace content {

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's
// corresponding DevToolsAgent object.
// TODO(yurys): now the client is almost empty later it will delegate calls to
// code in glue
class CONTENT_EXPORT DevToolsClient
    : public RenderFrameObserver,
      NON_EXPORTED_BASE(public blink::WebDevToolsFrontendClient) {
 public:
  DevToolsClient(RenderFrame* main_render_frame,
                 const std::string& compatibility_script);
  ~DevToolsClient() override;

 private:
  // RenderFrameObserver overrides.
  void DidClearWindowObject() override;
  void OnDestruct() override;

  // WebDevToolsFrontendClient implementation.
  void sendMessageToEmbedder(const blink::WebString&) override;

  bool isUnderTest() override;

  void OnDispatchOnInspectorFrontend(const std::string& message,
                                     uint32_t total_size);

  std::string compatibility_script_;
  std::unique_ptr<blink::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_
