// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_MANAGER_H_

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/common/content_export.h"

namespace content {

class DevToolsAgentHost;
class ServiceWorkerContextCore;

// EmbeddedWorkerDevToolsManager is used instead of WorkerDevToolsManager when
// "enable-embedded-shared-worker" flag is set.
// This class lives on UI thread.
class CONTENT_EXPORT EmbeddedWorkerDevToolsManager {
 public:
  typedef std::pair<int, int> WorkerId;
  class EmbeddedWorkerDevToolsAgentHost;

  class ServiceWorkerIdentifier {
   public:
    ServiceWorkerIdentifier(
        const ServiceWorkerContextCore* const service_worker_context,
        int64 service_worker_version_id);
    explicit ServiceWorkerIdentifier(const ServiceWorkerIdentifier& other);
    ~ServiceWorkerIdentifier() {}

    bool Matches(const ServiceWorkerIdentifier& other) const;

   private:
    const ServiceWorkerContextCore* const service_worker_context_;
    const int64 service_worker_version_id_;
  };

  // Returns the EmbeddedWorkerDevToolsManager singleton.
  static EmbeddedWorkerDevToolsManager* GetInstance();

  DevToolsAgentHost* GetDevToolsAgentHostForWorker(int worker_process_id,
                                                   int worker_route_id);
  DevToolsAgentHost* GetDevToolsAgentHostForServiceWorker(
      const ServiceWorkerIdentifier& service_worker_id);

  // Returns true when the worker must be paused on start because a DevTool
  // window for the same former SharedWorkerInstance is still opened.
  bool SharedWorkerCreated(int worker_process_id,
                           int worker_route_id,
                           const SharedWorkerInstance& instance);
  // Returns true when the worker must be paused on start because a DevTool
  // window for the same former ServiceWorkerIdentifier is still opened or
  // debug-on-start is enabled in chrome://serviceworker-internals.
  bool ServiceWorkerCreated(int worker_process_id,
                            int worker_route_id,
                            const ServiceWorkerIdentifier& service_worker_id);
  void WorkerContextStarted(int worker_process_id, int worker_route_id);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);

  void set_debug_service_worker_on_start(bool debug_on_start) {
    debug_service_worker_on_start_ = debug_on_start;
  }
  bool debug_service_worker_on_start() const {
    return debug_service_worker_on_start_;
  }

 private:
  friend struct DefaultSingletonTraits<EmbeddedWorkerDevToolsManager>;
  friend class EmbeddedWorkerDevToolsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerDevToolsManagerTest, BasicTest);
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerDevToolsManagerTest, AttachTest);

  enum WorkerState {
    WORKER_UNINSPECTED,
    WORKER_INSPECTED,
    WORKER_TERMINATED,
    WORKER_PAUSED_FOR_DEBUG_ON_START,
    WORKER_PAUSED_FOR_REATTACH,
  };

  class WorkerInfo {
   public:
    // Creates WorkerInfo for SharedWorker.
    explicit WorkerInfo(const SharedWorkerInstance& instance);
    // Creates WorkerInfo for ServiceWorker.
    explicit WorkerInfo(const ServiceWorkerIdentifier& service_worker_id);
    ~WorkerInfo();

    WorkerState state() { return state_; }
    void set_state(WorkerState new_state) { state_ = new_state; }
    EmbeddedWorkerDevToolsAgentHost* agent_host() { return agent_host_; }
    void set_agent_host(EmbeddedWorkerDevToolsAgentHost* agent_host) {
      agent_host_ = agent_host;
    }
    bool Matches(const SharedWorkerInstance& other);
    bool Matches(const ServiceWorkerIdentifier& other);

   private:
    scoped_ptr<SharedWorkerInstance> shared_worker_instance_;
    scoped_ptr<ServiceWorkerIdentifier> service_worker_id_;
    WorkerState state_;
    EmbeddedWorkerDevToolsAgentHost* agent_host_;
  };

  typedef base::ScopedPtrHashMap<WorkerId, WorkerInfo> WorkerInfoMap;

  EmbeddedWorkerDevToolsManager();
  virtual ~EmbeddedWorkerDevToolsManager();

  void RemoveInspectedWorkerData(EmbeddedWorkerDevToolsAgentHost* agent_host);

  WorkerInfoMap::iterator FindExistingSharedWorkerInfo(
      const SharedWorkerInstance& instance);
  WorkerInfoMap::iterator FindExistingServiceWorkerInfo(
      const ServiceWorkerIdentifier& service_worker_id);

  void MoveToPausedState(const WorkerId& id, const WorkerInfoMap::iterator& it);

  // Resets to its initial state as if newly created.
  void ResetForTesting();

  WorkerInfoMap workers_;

  bool debug_service_worker_on_start_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_EMBEDDED_WORKER_DEVTOOLS_MANAGER_H_
