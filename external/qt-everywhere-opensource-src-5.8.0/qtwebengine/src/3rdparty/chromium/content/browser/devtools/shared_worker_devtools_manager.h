// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_

#include <map>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

class DevToolsAgentHostImpl;
class SharedWorkerDevToolsAgentHost;
class SharedWorkerInstance;

// Manages WorkerDevToolsAgentHost's for Shared Workers.
// This class lives on UI thread.
class CONTENT_EXPORT SharedWorkerDevToolsManager {
 public:
  using WorkerId = std::pair<int, int>;

  // Returns the SharedWorkerDevToolsManager singleton.
  static SharedWorkerDevToolsManager* GetInstance();

  DevToolsAgentHostImpl* GetDevToolsAgentHostForWorker(int worker_process_id,
                                                       int worker_route_id);
  void AddAllAgentHosts(
      std::vector<scoped_refptr<SharedWorkerDevToolsAgentHost>>* result);

  // Returns true when the worker must be paused on start because a DevTool
  // window for the same former SharedWorkerInstance is still opened.
  bool WorkerCreated(int worker_process_id,
                     int worker_route_id,
                     const SharedWorkerInstance& instance);
  void WorkerReadyForInspection(int worker_process_id, int worker_route_id);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);
  void RemoveInspectedWorkerData(WorkerId id);

 private:
  friend struct base::DefaultSingletonTraits<SharedWorkerDevToolsManager>;
  friend class SharedWorkerDevToolsAgentHost;
  friend class SharedWorkerDevToolsManagerTest;
  FRIEND_TEST_ALL_PREFIXES(SharedWorkerDevToolsManagerTest, BasicTest);
  FRIEND_TEST_ALL_PREFIXES(SharedWorkerDevToolsManagerTest, AttachTest);

  using AgentHostMap = std::map<WorkerId, SharedWorkerDevToolsAgentHost*>;

  SharedWorkerDevToolsManager();
  ~SharedWorkerDevToolsManager();

  AgentHostMap::iterator FindExistingWorkerAgentHost(
      const SharedWorkerInstance& instance);

  // Resets to its initial state as if newly created.
  void ResetForTesting();

  AgentHostMap workers_;
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SHARED_WORKER_DEVTOOLS_MANAGER_H_
