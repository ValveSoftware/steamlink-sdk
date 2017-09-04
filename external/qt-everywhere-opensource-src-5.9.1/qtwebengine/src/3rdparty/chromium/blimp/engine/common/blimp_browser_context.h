// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_COMMON_BLIMP_BROWSER_CONTEXT_H_
#define BLIMP_ENGINE_COMMON_BLIMP_BROWSER_CONTEXT_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "blimp/engine/app/blimp_metrics_service_client.h"
#include "blimp/engine/app/blimp_system_url_request_context_getter.h"
#include "blimp/engine/app/blimp_url_request_context_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_job_factory.h"

namespace net {
class NetLog;
}

namespace blimp {
namespace engine {

class BlimpResourceContext;
class PermissionManager;

class BlimpBrowserContext : public content::BrowserContext {
 public:
  // Caller owns |net_log| and ensures it out-lives this browser context.
  BlimpBrowserContext(bool off_the_record, net::NetLog* net_log);
  ~BlimpBrowserContext() override;

  // content::BrowserContext implementation.
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

  // Provides a URLRequestContextGetter for system requests (e.g. metrics
  // uploads).
  net::URLRequestContextGetter* GetSystemRequestContextGetter();

  PrefService* GetPrefService() { return pref_service_.get(); }

 private:
  // Performs initialization of the BlimpBrowserContext while IO is still
  // allowed on the current thread.
  void InitWhileIOAllowed();

  // Helper function for PrefService initialization.
  void InitPrefService();

  // Used in metrics initialization to get a PrefService to store logs
  // temporarily. Must stay alive for lifetime of metrics_service_client_.
  std::unique_ptr<PrefService> pref_service_;

  // Used for metrics recording and reporting.
  std::unique_ptr<BlimpMetricsServiceClient> metrics_service_client_;

  std::unique_ptr<BlimpResourceContext> resource_context_;
  scoped_refptr<BlimpSystemURLRequestContextGetter> system_context_getter_;
  bool ignore_certificate_errors_;
  std::unique_ptr<content::PermissionManager> permission_manager_;
  bool off_the_record_;
  net::NetLog* net_log_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(BlimpBrowserContext);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_COMMON_BLIMP_BROWSER_CONTEXT_H_
