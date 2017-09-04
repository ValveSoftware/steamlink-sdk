// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

#include <string>
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/devtools_agent_host.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace net {
class IPEndPoint;
}

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT DevToolsManagerDelegate {
 public:
  // Opens the inspector for |agent_host|.
  virtual void Inspect(DevToolsAgentHost* agent_host);

  // Returns DevToolsAgentHost type to use for given |host| target.
  virtual std::string GetTargetType(RenderFrameHost* host);

  // Returns DevToolsAgentHost title to use for given |host| target.
  virtual std::string GetTargetTitle(RenderFrameHost* host);

  // Returns DevToolsAgentHost title to use for given |host| target.
  virtual std::string GetTargetDescription(RenderFrameHost* host);

  // Returns all targets embedder would like to report as discoverable.
  // If returns false, all targets content is aware of and only those
  // should be discoverable.
  virtual bool DiscoverTargets(
      const DevToolsAgentHost::DiscoveryCallback& callback);

  // Creates new inspectable target given the |url|.
  virtual scoped_refptr<DevToolsAgentHost> CreateNewTarget(const GURL& url);

  // Result ownership is passed to the caller.
  virtual base::DictionaryValue* HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command);

  // Should return discovery page HTML that should list available tabs
  // and provide attach links.
  virtual std::string GetDiscoveryPageHTML();

  // Returns frontend resource data by |path|.
  virtual std::string GetFrontendResource(const std::string& path);

  virtual void Initialized(const net::IPEndPoint* ) { }

  virtual ~DevToolsManagerDelegate();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
