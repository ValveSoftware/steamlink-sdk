// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_
#define CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebDevToolsFrontendClient.h"

namespace blink {
class WebDevToolsFrontend;
class WebString;
}

namespace content {

class RenderViewImpl;

// Developer tools UI end of communication channel between the render process of
// the page being inspected and tools UI renderer process. All messages will
// go through browser process. On the side of the inspected page there's
// corresponding DevToolsAgent object.
// TODO(yurys): now the client is almost empty later it will delegate calls to
// code in glue
class CONTENT_EXPORT DevToolsClient
  : public RenderViewObserver,
    NON_EXPORTED_BASE(public blink::WebDevToolsFrontendClient) {
 public:
  explicit DevToolsClient(RenderViewImpl* render_view);
  virtual ~DevToolsClient();

 private:
  // RenderView::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebDevToolsFrontendClient implementation.
  virtual void sendMessageToBackend(const blink::WebString&) OVERRIDE;
  virtual void sendMessageToEmbedder(const blink::WebString&) OVERRIDE;

  virtual bool isUnderTest() OVERRIDE;

  void OnDispatchOnInspectorFrontend(const std::string& message);

  scoped_ptr<blink::WebDevToolsFrontend> web_tools_frontend_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_DEVTOOLS_CLIENT_H_
