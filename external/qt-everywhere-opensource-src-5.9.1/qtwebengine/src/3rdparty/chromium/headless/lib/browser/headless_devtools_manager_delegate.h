// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_

#include "content/public/browser/devtools_manager_delegate.h"

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"

namespace headless {
class HeadlessBrowserImpl;
class HeadlessBrowserContext;
class HeadlessWebContentsImpl;

class HeadlessDevToolsManagerDelegate
    : public content::DevToolsManagerDelegate {
 public:
  explicit HeadlessDevToolsManagerDelegate(
      base::WeakPtr<HeadlessBrowserImpl> browser);
  ~HeadlessDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation:
  base::DictionaryValue* HandleCommand(content::DevToolsAgentHost* agent_host,
                                       base::DictionaryValue* command) override;
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;

 private:
  std::unique_ptr<base::Value> CreateTarget(
      const base::DictionaryValue* params);
  std::unique_ptr<base::Value> CloseTarget(const base::DictionaryValue* params);
  std::unique_ptr<base::Value> CreateBrowserContext(
      const base::DictionaryValue* params);
  std::unique_ptr<base::Value> DisposeBrowserContext(
      const base::DictionaryValue* params);

  base::WeakPtr<HeadlessBrowserImpl> browser_;

  using CommandMemberFnPtr = std::unique_ptr<base::Value> (
      HeadlessDevToolsManagerDelegate::*)(const base::DictionaryValue* params);

  std::map<std::string, CommandMemberFnPtr> command_map_;

  HeadlessBrowserContext* default_browser_context_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_DEVTOOLS_MANAGER_DELEGATE_H_
