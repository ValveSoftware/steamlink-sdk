// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"

namespace content {

class ServiceWorkerDevToolsAgentHost : public WorkerDevToolsAgentHost {
 public:
  using List = std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using Map = std::map<std::string,
                       scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using ServiceWorkerIdentifier =
      ServiceWorkerDevToolsManager::ServiceWorkerIdentifier;

  ServiceWorkerDevToolsAgentHost(WorkerId worker_id,
                                 const ServiceWorkerIdentifier& service_worker,
                                 bool is_installed_version);

  void UnregisterWorker();

  // DevToolsAgentHost overrides.
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;

  // WorkerDevToolsAgentHost overrides.
  void OnAttachedStateChanged(bool attached) override;

  void WorkerVersionInstalled();
  void WorkerVersionDoomed();

  int64_t service_worker_version_id() const;
  GURL scope() const;

  // If the ServiceWorker has been installed before the worker instance started,
  // it returns the time when the instance started. Otherwise returns the time
  // when the ServiceWorker was installed.
  base::Time version_installed_time() const { return version_installed_time_; }

  // Returns the time when the ServiceWorker was doomed.
  base::Time version_doomed_time() const { return version_doomed_time_; }

  bool Matches(const ServiceWorkerIdentifier& other);

 private:
  ~ServiceWorkerDevToolsAgentHost() override;
  std::unique_ptr<ServiceWorkerIdentifier> service_worker_;
  std::unique_ptr<devtools::network::NetworkHandler> network_handler_;
  base::Time version_installed_time_;
  base::Time version_doomed_time_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
