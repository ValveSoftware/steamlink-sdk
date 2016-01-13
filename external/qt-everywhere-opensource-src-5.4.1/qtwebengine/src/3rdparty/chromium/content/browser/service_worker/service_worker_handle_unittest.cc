// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerState.h"

namespace content {

namespace {

const int kRenderProcessId = 88;  // A dummy ID for testing.

void VerifyStateChangedMessage(int expected_handle_id,
                              blink::WebServiceWorkerState expected_state,
                              const IPC::Message* message) {
  ASSERT_TRUE(message != NULL);
  ServiceWorkerMsg_ServiceWorkerStateChanged::Param param;
  ASSERT_TRUE(ServiceWorkerMsg_ServiceWorkerStateChanged::Read(
      message, &param));
  EXPECT_EQ(expected_handle_id, param.b);
  EXPECT_EQ(expected_state, param.c);
}

}  // namespace

class ServiceWorkerHandleTest : public testing::Test {
 public:
  ServiceWorkerHandleTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(kRenderProcessId));

    registration_ = new ServiceWorkerRegistration(
        GURL("http://www.example.com/*"),
        GURL("http://www.example.com/service_worker.js"),
        1L,
        helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_, 1L, helper_->context()->AsWeakPtr());

    // Simulate adding one process to the worker.
    int embedded_worker_id = version_->embedded_worker()->embedded_worker_id();
    helper_->SimulateAddProcessToWorker(embedded_worker_id, kRenderProcessId);
  }

  virtual void TearDown() OVERRIDE {
    registration_ = NULL;
    version_ = NULL;
    helper_.reset();
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandleTest);
};

TEST_F(ServiceWorkerHandleTest, OnVersionStateChanged) {
  scoped_ptr<ServiceWorkerHandle> handle =
      ServiceWorkerHandle::Create(helper_->context()->AsWeakPtr(),
                                  helper_.get(),
                                  1 /* thread_id */,
                                  version_);

  // Start the worker, and then...
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version_->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // ...dispatch install event.
  status = SERVICE_WORKER_ERROR_FAILED;
  version_->DispatchInstallEvent(-1, CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  ASSERT_EQ(4UL, ipc_sink()->message_count());

  // We should be sending 1. StartWorker,
  EXPECT_EQ(EmbeddedWorkerMsg_StartWorker::ID,
            ipc_sink()->GetMessageAt(0)->type());
  // 2. StateChanged (state == Installing),
  VerifyStateChangedMessage(handle->handle_id(),
                            blink::WebServiceWorkerStateInstalling,
                            ipc_sink()->GetMessageAt(1));
  // 3. SendMessageToWorker (to send InstallEvent), and
  EXPECT_EQ(EmbeddedWorkerContextMsg_MessageToWorker::ID,
            ipc_sink()->GetMessageAt(2)->type());
  // 4. StateChanged (state == Installed).
  VerifyStateChangedMessage(handle->handle_id(),
                            blink::WebServiceWorkerStateInstalled,
                            ipc_sink()->GetMessageAt(3));
}

}  // namespace content
