// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kRenderProcessId = 11;

class EmbeddedWorkerInstanceTest : public testing::Test {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(kRenderProcessId));
  }

  virtual void TearDown() OVERRIDE {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    DCHECK(context());
    return context()->embedded_worker_registry();
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

static void SaveStatusAndCall(ServiceWorkerStatusCode* out,
                              const base::Closure& callback,
                              ServiceWorkerStatusCode status) {
  *out = status;
  callback.Run();
}

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  const int embedded_worker_id = worker->embedded_worker_id();
  const int64 service_worker_version_id = 55L;
  const GURL scope("http://example.com/*");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the worker.
  helper_->SimulateAddProcessToWorker(embedded_worker_id, kRenderProcessId);

  // Start should succeed.
  ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  worker->Start(
      service_worker_version_id,
      scope,
      url,
      std::vector<int>(),
      base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  base::RunLoop().RunUntilIdle();

  // Worker started message should be notified (by EmbeddedWorkerTestHelper).
  EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker->status());
  EXPECT_EQ(kRenderProcessId, worker->process_id());

  // Stop the worker.
  EXPECT_EQ(SERVICE_WORKER_OK, worker->Stop());
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // Worker stopped message should be notified (by EmbeddedWorkerTestHelper).
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Verify that we've sent two messages to start and terminate the worker.
  ASSERT_TRUE(ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  ASSERT_TRUE(ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StopWorker::ID));
}

TEST_F(EmbeddedWorkerInstanceTest, InstanceDestroyedBeforeStartFinishes) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  const int64 service_worker_version_id = 55L;
  const GURL scope("http://example.com/*");
  const GURL url("http://example.com/worker.js");

  ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  // Begin starting the worker.
  std::vector<int> available_process;
  available_process.push_back(kRenderProcessId);
  worker->Start(
      service_worker_version_id,
      scope,
      url,
      available_process,
      base::Bind(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  // But destroy it before it gets a chance to complete.
  worker.reset();
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_ERROR_ABORT, status);

  // Verify that we didn't send the message to start the worker.
  ASSERT_FALSE(
      ipc_sink()->GetUniqueMessageMatching(EmbeddedWorkerMsg_StartWorker::ID));
}

TEST_F(EmbeddedWorkerInstanceTest, SortProcesses) {
  scoped_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker->status());

  // Simulate adding processes to the worker.
  // Process 1 has 1 ref, 2 has 2 refs and 3 has 3 refs.
  const int embedded_worker_id = worker->embedded_worker_id();
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 1);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 2);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 2);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 3);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 3);
  helper_->SimulateAddProcessToWorker(embedded_worker_id, 3);

  // Process 3 has the biggest # of references and it should be chosen.
  EXPECT_THAT(worker->SortProcesses(std::vector<int>()),
              testing::ElementsAre(3, 2, 1));
  EXPECT_EQ(-1, worker->process_id());

  // Argument processes are added to the existing set, but only for a single
  // call.
  std::vector<int> registering_processes;
  registering_processes.push_back(1);
  registering_processes.push_back(1);
  registering_processes.push_back(1);
  registering_processes.push_back(4);
  EXPECT_THAT(worker->SortProcesses(registering_processes),
              testing::ElementsAre(1, 3, 2, 4));

  EXPECT_THAT(worker->SortProcesses(std::vector<int>()),
              testing::ElementsAre(3, 2, 1));
}

}  // namespace content
