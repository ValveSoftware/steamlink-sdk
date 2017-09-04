// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/files/file_path.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "headless/lib/browser/headless_browser_context_options.h"
#include "headless/lib/browser/headless_url_request_context_getter.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"

namespace headless {
class HeadlessBrowserImpl;
class HeadlessResourceContext;
class HeadlessWebContentsImpl;

class HeadlessBrowserContextImpl : public HeadlessBrowserContext,
                                   public content::BrowserContext {
 public:
  ~HeadlessBrowserContextImpl() override;

  static HeadlessBrowserContextImpl* From(
      HeadlessBrowserContext* browser_context);
  static HeadlessBrowserContextImpl* From(
      content::BrowserContext* browser_context);

  static std::unique_ptr<HeadlessBrowserContextImpl> Create(
      HeadlessBrowserContext::Builder* builder);

  // HeadlessBrowserContext implementation:
  HeadlessWebContents::Builder CreateWebContentsBuilder() override;
  std::vector<HeadlessWebContents*> GetAllWebContents() override;
  HeadlessWebContents* GetWebContentsForDevToolsAgentHostId(
      const std::string& devtools_agent_host_id) override;
  void Close() override;
  const std::string& Id() const override;

  // BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

  HeadlessWebContents* CreateWebContents(HeadlessWebContents::Builder* builder);
  // Register web contents which were created not through Headless API
  // (calling window.open() is a best example for this).
  void RegisterWebContents(
      std::unique_ptr<HeadlessWebContentsImpl> web_contents);
  void DestroyWebContents(HeadlessWebContentsImpl* web_contents);

  HeadlessBrowserImpl* browser() const;
  const HeadlessBrowserContextOptions* options() const;

 private:
  HeadlessBrowserContextImpl(
      HeadlessBrowserImpl* browser,
      std::unique_ptr<HeadlessBrowserContextOptions> context_options);

  // Performs initialization of the HeadlessBrowserContextImpl while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  HeadlessBrowserImpl* browser_;  // Not owned.
  std::unique_ptr<HeadlessBrowserContextOptions> context_options_;
  std::unique_ptr<HeadlessResourceContext> resource_context_;
  base::FilePath path_;

  std::unordered_map<std::string, std::unique_ptr<HeadlessWebContents>>
      web_contents_map_;

  std::string id_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessBrowserContextImpl);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_BROWSER_CONTEXT_IMPL_H_
