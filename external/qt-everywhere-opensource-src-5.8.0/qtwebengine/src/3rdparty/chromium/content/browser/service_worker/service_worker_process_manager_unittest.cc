// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/public/common/child_process_host.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

void DidAllocateWorkerProcess(const base::Closure& quit_closure,
                              ServiceWorkerStatusCode* status_out,
                              int* process_id_out,
                              bool* is_new_process_out,
                              ServiceWorkerStatusCode status,
                              int process_id,
                              bool is_new_process,
                              const EmbeddedWorkerSettings& settings) {
  *status_out = status;
  *process_id_out = process_id;
  *is_new_process_out = is_new_process;
  quit_closure.Run();
}

}  // namespace

class ServiceWorkerProcessManagerTest : public testing::Test {
 public:
  ServiceWorkerProcessManagerTest() {}

  void SetUp() override {
    browser_context_.reset(new TestBrowserContext);
    process_manager_.reset(
        new ServiceWorkerProcessManager(browser_context_.get()));
    pattern_ = GURL("http://www.example.com/");
    script_url_ = GURL("http://www.example.com/sw.js");
  }

  void TearDown() override {
    process_manager_->Shutdown();
    process_manager_.reset();
  }

  std::unique_ptr<MockRenderProcessHost> CreateRenderProcessHost() {
    return base::WrapUnique(new MockRenderProcessHost(browser_context_.get()));
  }

 protected:
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<ServiceWorkerProcessManager> process_manager_;
  GURL pattern_;
  GURL script_url_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProcessManagerTest);
};

TEST_F(ServiceWorkerProcessManagerTest, SortProcess) {
  // Process 1 has 2 refs, 2 has 3 refs and 3 has 1 ref.
  process_manager_->AddProcessReferenceToPattern(pattern_, 1);
  process_manager_->AddProcessReferenceToPattern(pattern_, 1);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 2);
  process_manager_->AddProcessReferenceToPattern(pattern_, 3);

  // Process 2 has the biggest # of references and it should be chosen.
  EXPECT_THAT(process_manager_->SortProcessesForPattern(pattern_),
              testing::ElementsAre(2, 1, 3));

  process_manager_->RemoveProcessReferenceFromPattern(pattern_, 1);
  process_manager_->RemoveProcessReferenceFromPattern(pattern_, 1);
  // Scores for each process: 2 : 3, 3 : 1, process 1 is removed.
  EXPECT_THAT(process_manager_->SortProcessesForPattern(pattern_),
              testing::ElementsAre(2, 3));
}

TEST_F(ServiceWorkerProcessManagerTest, FindAvailableProcess) {
  std::unique_ptr<MockRenderProcessHost> host1(CreateRenderProcessHost());
  std::unique_ptr<MockRenderProcessHost> host2(CreateRenderProcessHost());
  std::unique_ptr<MockRenderProcessHost> host3(CreateRenderProcessHost());

  // Process 1 has 2 refs, 2 has 3 refs and 3 has 1 ref.
  process_manager_->AddProcessReferenceToPattern(pattern_, host1->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host1->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host2->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host2->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host2->GetID());
  process_manager_->AddProcessReferenceToPattern(pattern_, host3->GetID());

  // When all processes are in foreground, process 2 that has the highest
  // refcount should be chosen.
  EXPECT_EQ(host2->GetID(), process_manager_->FindAvailableProcess(pattern_));

  // Backgrounded process 2 should be deprioritized.
  host2->set_is_process_backgrounded(true);
  EXPECT_EQ(host1->GetID(), process_manager_->FindAvailableProcess(pattern_));

  // When all processes are in background, process 2 that has the highest
  // refcount should be chosen.
  host1->set_is_process_backgrounded(true);
  host3->set_is_process_backgrounded(true);
  EXPECT_EQ(host2->GetID(), process_manager_->FindAvailableProcess(pattern_));

  // Process 3 should be chosen because it is the only foreground process.
  host3->set_is_process_backgrounded(false);
  EXPECT_EQ(host3->GetID(), process_manager_->FindAvailableProcess(pattern_));
}

TEST_F(ServiceWorkerProcessManagerTest,
       AllocateWorkerProcess_FindAvailableProcess) {
  const int kEmbeddedWorkerId1 = 100;
  const int kEmbeddedWorkerId2 = 200;
  const int kEmbeddedWorkerId3 = 300;
  GURL scope1("http://example.com/scope1");
  GURL scope2("http://example.com/scope2");

  // Set up mock renderer process hosts.
  std::unique_ptr<MockRenderProcessHost> host1(CreateRenderProcessHost());
  std::unique_ptr<MockRenderProcessHost> host2(CreateRenderProcessHost());
  process_manager_->AddProcessReferenceToPattern(scope1, host1->GetID());
  process_manager_->AddProcessReferenceToPattern(scope2, host2->GetID());
  ASSERT_EQ(0, host1->worker_ref_count());
  ASSERT_EQ(0, host2->worker_ref_count());

  std::map<int, ServiceWorkerProcessManager::ProcessInfo>& instance_info =
      process_manager_->instance_info_;

  // (1) Allocate a process to a worker.
  base::RunLoop run_loop1;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  int process_id = -10;
  bool is_new_process = true;
  process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId1, scope1, script_url_,
      true /* can_use_existing_process */,
      base::Bind(&DidAllocateWorkerProcess, run_loop1.QuitClosure(), &status,
                 &process_id, &is_new_process));
  run_loop1.Run();

  // An existing process should be allocated to the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host1->GetID(), process_id);
  EXPECT_FALSE(is_new_process);
  EXPECT_EQ(1, host1->worker_ref_count());
  EXPECT_EQ(0, host2->worker_ref_count());
  EXPECT_EQ(1u, instance_info.size());
  std::map<int, ServiceWorkerProcessManager::ProcessInfo>::iterator found =
      instance_info.find(kEmbeddedWorkerId1);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host1->GetID(), found->second.process_id);

  // (2) Allocate a process to another worker whose scope is the same with the
  // first worker.
  base::RunLoop run_loop2;
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  process_id = -10;
  is_new_process = true;
  process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId2, scope1, script_url_,
      true /* can_use_existing_process */,
      base::Bind(&DidAllocateWorkerProcess, run_loop2.QuitClosure(), &status,
                 &process_id, &is_new_process));
  run_loop2.Run();

  // The same process should be allocated to the second worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host1->GetID(), process_id);
  EXPECT_FALSE(is_new_process);
  EXPECT_EQ(2, host1->worker_ref_count());
  EXPECT_EQ(0, host2->worker_ref_count());
  EXPECT_EQ(2u, instance_info.size());
  found = instance_info.find(kEmbeddedWorkerId2);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host1->GetID(), found->second.process_id);

  // (3) Allocate a process to a third worker whose scope is different from
  // other workers.
  base::RunLoop run_loop3;
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  process_id = -10;
  is_new_process = true;
  process_manager_->AllocateWorkerProcess(
      kEmbeddedWorkerId3, scope2, script_url_,
      true /* can_use_existing_process */,
      base::Bind(&DidAllocateWorkerProcess, run_loop3.QuitClosure(), &status,
                 &process_id, &is_new_process));
  run_loop3.Run();

  // A different existing process should be allocated to the third worker.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(host2->GetID(), process_id);
  EXPECT_FALSE(is_new_process);
  EXPECT_EQ(2, host1->worker_ref_count());
  EXPECT_EQ(1, host2->worker_ref_count());
  EXPECT_EQ(3u, instance_info.size());
  found = instance_info.find(kEmbeddedWorkerId3);
  ASSERT_TRUE(found != instance_info.end());
  EXPECT_EQ(host2->GetID(), found->second.process_id);

  // The instance map should be updated by process release.
  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId3);
  EXPECT_EQ(2, host1->worker_ref_count());
  EXPECT_EQ(0, host2->worker_ref_count());
  EXPECT_EQ(2u, instance_info.size());
  EXPECT_TRUE(ContainsKey(instance_info, kEmbeddedWorkerId1));
  EXPECT_TRUE(ContainsKey(instance_info, kEmbeddedWorkerId2));

  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId1);
  EXPECT_EQ(1, host1->worker_ref_count());
  EXPECT_EQ(0, host2->worker_ref_count());
  EXPECT_EQ(1u, instance_info.size());
  EXPECT_TRUE(ContainsKey(instance_info, kEmbeddedWorkerId2));

  process_manager_->ReleaseWorkerProcess(kEmbeddedWorkerId2);
  EXPECT_EQ(0, host1->worker_ref_count());
  EXPECT_EQ(0, host2->worker_ref_count());
  EXPECT_TRUE(instance_info.empty());
}

TEST_F(ServiceWorkerProcessManagerTest, AllocateWorkerProcess_InShutdown) {
  process_manager_->Shutdown();
  ASSERT_TRUE(process_manager_->IsShutdown());

  base::RunLoop run_loop;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  int process_id = -10;
  bool is_new_process = true;
  process_manager_->AllocateWorkerProcess(
      1, pattern_, script_url_, true /* can_use_existing_process */,
      base::Bind(&DidAllocateWorkerProcess, run_loop.QuitClosure(), &status,
                 &process_id, &is_new_process));
  run_loop.Run();

  // Allocating a process in shutdown should abort.
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, process_id);
  EXPECT_FALSE(is_new_process);
  EXPECT_TRUE(process_manager_->instance_info_.empty());
}

}  // namespace content
