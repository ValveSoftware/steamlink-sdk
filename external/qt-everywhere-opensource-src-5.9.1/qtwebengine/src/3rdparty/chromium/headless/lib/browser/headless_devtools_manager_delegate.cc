// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools_manager_delegate.h"

#include <string>
#include <utility>

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/target.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

HeadlessDevToolsManagerDelegate::HeadlessDevToolsManagerDelegate(
    base::WeakPtr<HeadlessBrowserImpl> browser)
    : browser_(std::move(browser)), default_browser_context_(nullptr) {
  command_map_["Target.createTarget"] =
      &HeadlessDevToolsManagerDelegate::CreateTarget;
  command_map_["Target.closeTarget"] =
      &HeadlessDevToolsManagerDelegate::CloseTarget;
  command_map_["Target.createBrowserContext"] =
      &HeadlessDevToolsManagerDelegate::CreateBrowserContext;
  command_map_["Target.disposeBrowserContext"] =
      &HeadlessDevToolsManagerDelegate::DisposeBrowserContext;
}

HeadlessDevToolsManagerDelegate::~HeadlessDevToolsManagerDelegate() {}

base::DictionaryValue* HeadlessDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!browser_)
    return nullptr;

  int id;
  std::string method;
  const base::DictionaryValue* params = nullptr;
  if (!command->GetInteger("id", &id) ||
      !command->GetString("method", &method) ||
      !command->GetDictionary("params", &params)) {
    return nullptr;
  }
  auto find_it = command_map_.find(method);
  if (find_it == command_map_.end())
    return nullptr;
  CommandMemberFnPtr command_fn_ptr = find_it->second;
  std::unique_ptr<base::Value> cmd_result(((this)->*command_fn_ptr)(params));
  if (!cmd_result)
    return nullptr;

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger("id", id);
  result->Set("result", std::move(cmd_result));
  return result.release();
}

std::string HeadlessDevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_HEADLESS_LIB_DEVTOOLS_DISCOVERY_PAGE).as_string();
}

std::string HeadlessDevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

std::unique_ptr<base::Value> HeadlessDevToolsManagerDelegate::CreateTarget(
    const base::DictionaryValue* params) {
  std::string url;
  std::string browser_context_id;
  int width = browser_->options()->window_size.width();
  int height = browser_->options()->window_size.height();
  params->GetString("url", &url);
  params->GetString("browserContextId", &browser_context_id);
  params->GetInteger("width", &width);
  params->GetInteger("height", &height);

  // TODO(alexclarke): Should we fail when user passes incorrect id?
  HeadlessBrowserContext* context =
      browser_->GetBrowserContextForId(browser_context_id);
  if (!context) {
    if (!default_browser_context_) {
      default_browser_context_ =
          browser_->CreateBrowserContextBuilder().Build();
    }
    context = default_browser_context_;
  }

  HeadlessWebContentsImpl* web_contents_impl =
      HeadlessWebContentsImpl::From(context->CreateWebContentsBuilder()
                                        .SetInitialURL(GURL(url))
                                        .SetWindowSize(gfx::Size(width, height))
                                        .Build());

  return target::CreateTargetResult::Builder()
      .SetTargetId(web_contents_impl->GetDevToolsAgentHostId())
      .Build()
      ->Serialize();
}

std::unique_ptr<base::Value> HeadlessDevToolsManagerDelegate::CloseTarget(
    const base::DictionaryValue* params) {
  std::string target_id;
  if (!params->GetString("targetId", &target_id)) {
    return nullptr;
  }
  HeadlessWebContents* web_contents =
      browser_->GetWebContentsForDevToolsAgentHostId(target_id);
  bool success = false;
  if (web_contents) {
    web_contents->Close();
    success = true;
  }
  return target::CloseTargetResult::Builder()
      .SetSuccess(success)
      .Build()
      ->Serialize();
}

std::unique_ptr<base::Value>
HeadlessDevToolsManagerDelegate::CreateBrowserContext(
    const base::DictionaryValue* params) {
  HeadlessBrowserContext* browser_context =
      browser_->CreateBrowserContextBuilder().Build();

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  return target::CreateBrowserContextResult::Builder()
      .SetBrowserContextId(browser_context->Id())
      .Build()
      ->Serialize();
}

std::unique_ptr<base::Value>
HeadlessDevToolsManagerDelegate::DisposeBrowserContext(
    const base::DictionaryValue* params) {
  std::string browser_context_id;
  if (!params->GetString("browserContextId", &browser_context_id)) {
    return nullptr;
  }
  HeadlessBrowserContext* context =
      browser_->GetBrowserContextForId(browser_context_id);

  bool success = false;
  if (context && context != default_browser_context_ &&
      context->GetAllWebContents().empty()) {
    success = true;
    context->Close();
  }

  return target::DisposeBrowserContextResult::Builder()
      .SetSuccess(success)
      .Build()
      ->Serialize();
}

}  // namespace headless
