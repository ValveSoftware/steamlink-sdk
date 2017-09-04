// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INSPECTOR_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INSPECTOR_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {

class RenderFrameHostImpl;

namespace devtools {
namespace inspector {

class InspectorHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  InspectorHandler();
  virtual ~InspectorHandler();

  void SetClient(std::unique_ptr<Client> client);
  void SetRenderFrameHost(RenderFrameHostImpl* host);

  void TargetCrashed();
  void TargetDetached(const std::string& reason);

  Response Enable();
  Response Disable();

 private:
  std::unique_ptr<Client> client_;
  RenderFrameHostImpl* host_;

  DISALLOW_COPY_AND_ASSIGN(InspectorHandler);
};

}  // namespace inspector
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_INSPECTOR_HANDLER_H_
