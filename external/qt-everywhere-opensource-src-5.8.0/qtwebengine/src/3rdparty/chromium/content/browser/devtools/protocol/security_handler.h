// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_

#include "base/macros.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/security_style.h"

namespace content {
namespace devtools {
namespace security {

class SecurityHandler : public WebContentsObserver {
 public:
  typedef DevToolsProtocolClient::Response Response;

  SecurityHandler();
  ~SecurityHandler() override;

  void SetClient(std::unique_ptr<Client> client);
  void SetRenderFrameHost(RenderFrameHost* host);

  Response Enable();
  Response Disable();

 private:
  void AttachToRenderFrameHost();

  // WebContentsObserver overrides
  void SecurityStyleChanged(
      SecurityStyle security_style,
      const SecurityStyleExplanations& security_style_explanations) override;

  std::unique_ptr<Client> client_;
  bool enabled_;
  RenderFrameHost* host_;

  DISALLOW_COPY_AND_ASSIGN(SecurityHandler);
};

}  // namespace security
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SECURITY_HANDLER_H_
