// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_
#define CHROMECAST_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_

#include "base/macros.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"

namespace chromecast {
namespace shell {

class CastDevToolsDelegate :
    public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  CastDevToolsDelegate();
  ~CastDevToolsDelegate() override;

  // devtools_http_handler::DevToolsHttpHandlerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;
  std::string GetPageThumbnailData(const GURL& url) override;
  content::DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDevToolsDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_CAST_DEV_TOOLS_DELEGATE_H_
