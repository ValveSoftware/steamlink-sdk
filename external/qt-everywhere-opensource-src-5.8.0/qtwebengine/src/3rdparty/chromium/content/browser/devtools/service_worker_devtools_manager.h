// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class BrowserContext;
class DevToolsAgentHostImpl;
class ServiceWorkerDevToolsAgentHost;
class ServiceWorkerContextCore;

// Manages WorkerDevToolsAgentHost's for Service Workers.
// This class lives on UI thread.
class CONTENT_EXPORT ServiceWorkerDevToolsManager {
 public:
  using WorkerId = std::pair<int, int>;
  using AgentList = std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>;

  class Observer {
   public:
    virtual void WorkerCreated(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerReadyForInspection(
        ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {}

   protected:
    virtual ~Observer() {}
  };

  class ServiceWorkerIdentifier {
   public:
    ServiceWorkerIdentifier(
        const ServiceWorkerContextCore* context,
        base::WeakPtr<ServiceWorkerContextCore> context_weak,
        int64_t version_id,
        const GURL& url,
        const GURL& scope);
    ServiceWorkerIdentifier(const ServiceWorkerIdentifier& other);
    ~ServiceWorkerIdentifier();

    bool Matches(const ServiceWorkerIdentifier& other) const;

    const ServiceWorkerContextCore* context() const { return context_; }
    base::WeakPtr<ServiceWorkerContextCore> context_weak() const {
      return context_weak_;
    }
    int64_t version_id() const { return version_id_; }
    GURL url() const { return url_; }
    GURL scope() const { return scope_; }

   private:
    const ServiceWorkerContextCore* const context_;
    const base::WeakPtr<ServiceWorkerContextCore> context_weak_;
    const int64_t version_id_;
    const GURL url_;
    const GURL scope_;
  };

  // Returns the ServiceWorkerDevToolsManager singleton.
  static ServiceWorkerDevToolsManager* GetInstance();

  DevToolsAgentHostImpl* GetDevToolsAgentHostForWorker(int worker_process_id,
                                                       int worker_route_id);
  void AddAllAgentHosts(
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result);
  void AddAllAgentHostsForBrowserContext(
      BrowserContext* browser_context,
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result);

  // Returns true when the worker must be paused on start because a DevTool
  // window for the same former ServiceWorkerIdentifier is still opened or
  // debug-on-start is enabled in chrome://serviceworker-internals.
  bool WorkerCreated(int worker_process_id,
                     int worker_route_id,
                     const ServiceWorkerIdentifier& service_worker_id,
                     bool is_installed_version);
  void WorkerReadyForInspection(int worker_process_id, int worker_route_id);
  void WorkerVersionInstalled(int worker_process_id, int worker_route_id);
  void WorkerVersionDoomed(int worker_process_id, int worker_route_id);
  void WorkerStopIgnored(int worker_process_id, int worker_route_id);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);
  void RemoveInspectedWorkerData(WorkerId id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void set_debug_service_worker_on_start(bool debug_on_start);
  bool debug_service_worker_on_start() const {
    return debug_service_worker_on_start_;
  }

 private:
  friend struct base::DefaultSingletonTraits<ServiceWorkerDevToolsManager>;
  friend class ServiceWorkerDevToolsAgentHost;

  using AgentHostMap = std::map<WorkerId, ServiceWorkerDevToolsAgentHost*>;

  ServiceWorkerDevToolsManager();
  ~ServiceWorkerDevToolsManager();

  AgentHostMap::iterator FindExistingWorkerAgentHost(
      const ServiceWorkerIdentifier& service_worker_id);

  // Resets to its initial state as if newly created.
  void ResetForTesting();

  base::ObserverList<Observer> observer_list_;
  AgentHostMap workers_;
  bool debug_service_worker_on_start_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_
