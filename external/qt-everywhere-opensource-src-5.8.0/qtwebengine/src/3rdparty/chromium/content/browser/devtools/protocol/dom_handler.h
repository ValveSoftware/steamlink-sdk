// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DOM_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DOM_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"

namespace content {

class RenderFrameHostImpl;

namespace devtools {
namespace dom {

class DOMHandler {
 public:
  typedef DevToolsProtocolClient::Response Response;

  DOMHandler();
  virtual ~DOMHandler();

  void SetRenderFrameHost(RenderFrameHostImpl* host);

  Response SetFileInputFiles(NodeId node_id,
                             const std::vector<std::string>& files);

 private:
  RenderFrameHostImpl* host_;
  DISALLOW_COPY_AND_ASSIGN(DOMHandler);
};

}  // namespace dom
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_DOM_HANDLER_H_
