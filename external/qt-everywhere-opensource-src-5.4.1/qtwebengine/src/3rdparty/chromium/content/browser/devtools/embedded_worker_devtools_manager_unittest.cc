// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/embedded_worker_devtools_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/devtools/devtools_manager_impl.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/worker_host/worker_storage_partition.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestDevToolsClientHost : public DevToolsClientHost {
 public:
  TestDevToolsClientHost() {}
  virtual ~TestDevToolsClientHost() {}
  virtual void DispatchOnInspectorFrontend(
      const std::string& message) OVERRIDE {}
  virtual void InspectedContentsClosing() OVERRIDE {}
  virtual void ReplacedWithAnotherClient() OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDevToolsClientHost);
};
}

class EmbeddedWorkerDevToolsManagerTest : public testing::Test {
 public:
  EmbeddedWorkerDevToolsManagerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        browser_context_(new TestBrowserContext()),
        partition_(
            new WorkerStoragePartition(browser_context_->GetRequestContext(),
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)),
        partition_id_(*partition_.get()) {}

 protected:
  virtual void SetUp() OVERRIDE {
    manager_ = EmbeddedWorkerDevToolsManager::GetInstance();
  }
  virtual void TearDown() OVERRIDE {
    EmbeddedWorkerDevToolsManager::GetInstance()->ResetForTesting();
  }

  void CheckWorkerState(int worker_process_id,
                        int worker_route_id,
                        EmbeddedWorkerDevToolsManager::WorkerState state) {
    const EmbeddedWorkerDevToolsManager::WorkerId id(worker_process_id,
                                                     worker_route_id);
    EmbeddedWorkerDevToolsManager::WorkerInfoMap::iterator it =
        manager_->workers_.find(id);
    EXPECT_TRUE(manager_->workers_.end() != it);
    EXPECT_EQ(state, it->second->state());
  }

  void CheckWorkerNotExist(int worker_process_id, int worker_route_id) {
    const EmbeddedWorkerDevToolsManager::WorkerId id(worker_process_id,
                                                     worker_route_id);
    EXPECT_TRUE(manager_->workers_.end() == manager_->workers_.find(id));
  }

  void CheckWorkerCount(size_t size) {
    EXPECT_EQ(size, manager_->workers_.size());
  }

  void RegisterDevToolsClientHostFor(DevToolsAgentHost* agent_host,
                                     DevToolsClientHost* client_host) {
    DevToolsManagerImpl::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host, client_host);
  }

  void ClientHostClosing(DevToolsClientHost* client_host) {
    DevToolsManagerImpl::GetInstance()->ClientHostClosing(client_host);
  }

  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl ui_thread_;
  scoped_ptr<TestBrowserContext> browser_context_;
  scoped_ptr<WorkerStoragePartition> partition_;
  const WorkerStoragePartitionId partition_id_;
  EmbeddedWorkerDevToolsManager* manager_;
};

TEST_F(EmbeddedWorkerDevToolsManagerTest, BasicTest) {
  scoped_refptr<DevToolsAgentHost> agent_host;

  SharedWorkerInstance instance1(GURL("http://example.com/w.js"),
                                 base::string16(),
                                 base::string16(),
                                 blink::WebContentSecurityPolicyTypeReport,
                                 browser_context_->GetResourceContext(),
                                 partition_id_);

  agent_host = manager_->GetDevToolsAgentHostForWorker(1, 1);
  EXPECT_FALSE(agent_host.get());

  // Created -> Started -> Destroyed
  CheckWorkerNotExist(1, 1);
  manager_->SharedWorkerCreated(1, 1, instance1);
  CheckWorkerState(1, 1, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  manager_->WorkerContextStarted(1, 1);
  CheckWorkerState(1, 1, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 1);
  CheckWorkerNotExist(1, 1);

  // Created -> GetDevToolsAgentHost -> Started -> Destroyed
  CheckWorkerNotExist(1, 2);
  manager_->SharedWorkerCreated(1, 2, instance1);
  CheckWorkerState(1, 2, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  agent_host = manager_->GetDevToolsAgentHostForWorker(1, 2);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 2, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  EXPECT_EQ(agent_host.get(), manager_->GetDevToolsAgentHostForWorker(1, 2));
  manager_->WorkerContextStarted(1, 2);
  CheckWorkerState(1, 2, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  manager_->WorkerDestroyed(1, 2);
  CheckWorkerState(1, 2, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  agent_host = NULL;
  CheckWorkerNotExist(1, 2);

  // Created -> Started -> GetDevToolsAgentHost -> Destroyed
  CheckWorkerNotExist(1, 3);
  manager_->SharedWorkerCreated(1, 3, instance1);
  CheckWorkerState(1, 3, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  manager_->WorkerContextStarted(1, 3);
  CheckWorkerState(1, 3, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  agent_host = manager_->GetDevToolsAgentHostForWorker(1, 3);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 3, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  manager_->WorkerDestroyed(1, 3);
  CheckWorkerState(1, 3, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  agent_host = NULL;
  CheckWorkerNotExist(1, 3);

  // Created -> Destroyed
  CheckWorkerNotExist(1, 4);
  manager_->SharedWorkerCreated(1, 4, instance1);
  CheckWorkerState(1, 4, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  manager_->WorkerDestroyed(1, 4);
  CheckWorkerNotExist(1, 4);

  // Created -> GetDevToolsAgentHost -> Destroyed
  CheckWorkerNotExist(1, 5);
  manager_->SharedWorkerCreated(1, 5, instance1);
  CheckWorkerState(1, 5, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  agent_host = manager_->GetDevToolsAgentHostForWorker(1, 5);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 5, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  manager_->WorkerDestroyed(1, 5);
  CheckWorkerState(1, 5, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  agent_host = NULL;
  CheckWorkerNotExist(1, 5);

  // Created -> GetDevToolsAgentHost -> Free agent_host -> Destroyed
  CheckWorkerNotExist(1, 6);
  manager_->SharedWorkerCreated(1, 6, instance1);
  CheckWorkerState(1, 6, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  agent_host = manager_->GetDevToolsAgentHostForWorker(1, 6);
  EXPECT_TRUE(agent_host.get());
  CheckWorkerState(1, 6, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  agent_host = NULL;
  manager_->WorkerDestroyed(1, 6);
  CheckWorkerNotExist(1, 6);
}

TEST_F(EmbeddedWorkerDevToolsManagerTest, AttachTest) {
  scoped_refptr<DevToolsAgentHost> agent_host1;
  scoped_refptr<DevToolsAgentHost> agent_host2;

  SharedWorkerInstance instance1(GURL("http://example.com/w1.js"),
                                 base::string16(),
                                 base::string16(),
                                 blink::WebContentSecurityPolicyTypeReport,
                                 browser_context_->GetResourceContext(),
                                 partition_id_);
  SharedWorkerInstance instance2(GURL("http://example.com/w2.js"),
                                 base::string16(),
                                 base::string16(),
                                 blink::WebContentSecurityPolicyTypeReport,
                                 browser_context_->GetResourceContext(),
                                 partition_id_);

  // Created -> GetDevToolsAgentHost -> Register -> Started -> Destroyed
  scoped_ptr<TestDevToolsClientHost> client_host1(new TestDevToolsClientHost());
  CheckWorkerNotExist(2, 1);
  manager_->SharedWorkerCreated(2, 1, instance1);
  CheckWorkerState(2, 1, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  agent_host1 = manager_->GetDevToolsAgentHostForWorker(2, 1);
  EXPECT_TRUE(agent_host1.get());
  CheckWorkerState(2, 1, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  EXPECT_EQ(agent_host1.get(), manager_->GetDevToolsAgentHostForWorker(2, 1));
  RegisterDevToolsClientHostFor(agent_host1.get(), client_host1.get());
  CheckWorkerState(2, 1, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  manager_->WorkerContextStarted(2, 1);
  CheckWorkerState(2, 1, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  manager_->WorkerDestroyed(2, 1);
  CheckWorkerState(2, 1, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  EXPECT_EQ(agent_host1.get(), manager_->GetDevToolsAgentHostForWorker(2, 1));

  // Created -> Started -> GetDevToolsAgentHost -> Register -> Destroyed
  scoped_ptr<TestDevToolsClientHost> client_host2(new TestDevToolsClientHost());
  manager_->SharedWorkerCreated(2, 2, instance2);
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  manager_->WorkerContextStarted(2, 2);
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_UNINSPECTED);
  agent_host2 = manager_->GetDevToolsAgentHostForWorker(2, 2);
  EXPECT_TRUE(agent_host2.get());
  EXPECT_NE(agent_host1.get(), agent_host2.get());
  EXPECT_EQ(agent_host2.get(), manager_->GetDevToolsAgentHostForWorker(2, 2));
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  RegisterDevToolsClientHostFor(agent_host2.get(), client_host2.get());
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  manager_->WorkerDestroyed(2, 2);
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  EXPECT_EQ(agent_host2.get(), manager_->GetDevToolsAgentHostForWorker(2, 2));

  // Re-created -> Started -> ClientHostClosing -> Destroyed
  CheckWorkerState(2, 1, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  manager_->SharedWorkerCreated(2, 3, instance1);
  CheckWorkerNotExist(2, 1);
  CheckWorkerState(
      2, 3, EmbeddedWorkerDevToolsManager::WORKER_PAUSED_FOR_REATTACH);
  EXPECT_EQ(agent_host1.get(), manager_->GetDevToolsAgentHostForWorker(2, 3));
  manager_->WorkerContextStarted(2, 3);
  CheckWorkerState(2, 3, EmbeddedWorkerDevToolsManager::WORKER_INSPECTED);
  ClientHostClosing(client_host1.get());
  manager_->WorkerDestroyed(2, 3);
  CheckWorkerState(2, 3, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  agent_host1 = NULL;
  CheckWorkerNotExist(2, 3);

  // Re-created -> Destroyed
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);
  manager_->SharedWorkerCreated(2, 4, instance2);
  CheckWorkerNotExist(2, 2);
  CheckWorkerState(
      2, 4, EmbeddedWorkerDevToolsManager::WORKER_PAUSED_FOR_REATTACH);
  EXPECT_EQ(agent_host2.get(), manager_->GetDevToolsAgentHostForWorker(2, 4));
  manager_->WorkerDestroyed(2, 4);
  CheckWorkerNotExist(2, 4);
  CheckWorkerState(2, 2, EmbeddedWorkerDevToolsManager::WORKER_TERMINATED);

  // Re-created -> ClientHostClosing -> Destroyed
  manager_->SharedWorkerCreated(2, 5, instance2);
  CheckWorkerNotExist(2, 2);
  CheckWorkerState(
      2, 5, EmbeddedWorkerDevToolsManager::WORKER_PAUSED_FOR_REATTACH);
  EXPECT_EQ(agent_host2.get(), manager_->GetDevToolsAgentHostForWorker(2, 5));
  ClientHostClosing(client_host2.get());
  CheckWorkerCount(1);
  agent_host2 = NULL;
  CheckWorkerCount(1);
  manager_->WorkerDestroyed(2, 5);
  CheckWorkerCount(0);
}

}  // namespace content
