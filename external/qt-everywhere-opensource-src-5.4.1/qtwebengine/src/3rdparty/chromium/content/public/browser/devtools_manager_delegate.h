// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_

namespace base {
class DictionaryValue;
}

namespace content {

class BrowserContext;
class DevToolsAgentHost;

class DevToolsManagerDelegate {
 public:
  virtual ~DevToolsManagerDelegate() {}

  // Opens the inspector for |agent_host|.
  virtual void Inspect(BrowserContext* browser_context,
                       DevToolsAgentHost* agent_host) = 0;

  virtual void DevToolsAgentStateChanged(DevToolsAgentHost* agent_host,
                                         bool attached) = 0;

  // Result ownership is passed to the caller.
  virtual base::DictionaryValue* HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_MANAGER_DELEGATE_H_
