// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kRenderProcessId = 1;

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(
      int process_id,
      ServiceWorkerContextWrapper* context_wrapper,
      EmbeddedWorkerTestHelper* helper)
      : ServiceWorkerDispatcherHost(process_id, NULL),
        bad_messages_received_count_(0),
        helper_(helper) {
    Init(context_wrapper);
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    return helper_->Send(message);
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  virtual void BadMessageReceived() OVERRIDE {
    ++bad_messages_received_count_;
  }

  int bad_messages_received_count_;

 protected:
  EmbeddedWorkerTestHelper* helper_;
  virtual ~TestingServiceWorkerDispatcherHost() {}
};

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() {
    helper_.reset(new EmbeddedWorkerTestHelper(kRenderProcessId));
    dispatcher_host_ = new TestingServiceWorkerDispatcherHost(
        kRenderProcessId, context_wrapper(), helper_.get());
  }

  virtual void TearDown() {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void Register(int64 provider_id,
                GURL pattern,
                GURL worker_url,
                uint32 expected_message) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_RegisterServiceWorker(
            -1, -1, provider_id, pattern, worker_url));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void Unregister(int64 provider_id, GURL pattern, uint32 expected_message) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_UnregisterServiceWorker(
            -1, -1, provider_id, pattern));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host_;
};

TEST_F(ServiceWorkerDispatcherHostTest, DisabledCausesError) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, -1, GURL(), GURL()));

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host_->ipc_sink()->message_count());
  EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ServiceWorkerRegistrationError::ID));
}

// TODO(falken): Enable this test when we remove the
// --enable-service-worker-flag (see crbug.com/352581)
TEST_F(ServiceWorkerDispatcherHostTest, DISABLED_RegisterSameOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      kRenderProcessId, kProviderId, context()->AsWeakPtr(), NULL));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
  context()->AddProviderHost(host.Pass());

  Register(kProviderId,
           GURL("http://www.example.com/*"),
           GURL("http://foo.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
  Register(kProviderId,
           GURL("http://foo.example.com/*"),
           GURL("http://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
  Register(kProviderId,
           GURL("http://foo.example.com/*"),
           GURL("http://foo.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
  Register(kProviderId,
           GURL("http://www.example.com/*"),
           GURL("http://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistered::ID);
}

// TODO(falken): Enable this test when we remove the
// --enable-service-worker-flag (see crbug.com/352581)
TEST_F(ServiceWorkerDispatcherHostTest, DISABLED_UnregisterSameOrigin) {
  const int64 kProviderId = 99;  // Dummy value
  scoped_ptr<ServiceWorkerProviderHost> host(new ServiceWorkerProviderHost(
      kRenderProcessId, kProviderId, context()->AsWeakPtr(), NULL));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  base::WeakPtr<ServiceWorkerProviderHost> provider_host = host->AsWeakPtr();
  context()->AddProviderHost(host.Pass());

  Unregister(kProviderId,
             GURL("http://foo.example.com/*"),
             ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
  Unregister(kProviderId,
             GURL("http://www.example.com/*"),
             ServiceWorkerMsg_ServiceWorkerUnregistered::ID);
}

// Disable this since now we cache command-line switch in
// ServiceWorkerUtils::IsFeatureEnabled() and this could be flaky depending
// on testing order. (crbug.com/352581)
// TODO(kinuko): Just remove DisabledCausesError test above and enable
// this test when we remove the --enable-service-worker flag.
TEST_F(ServiceWorkerDispatcherHostTest, DISABLED_Enabled) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, -1, GURL(), GURL()));
  base::RunLoop().RunUntilIdle();

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(2UL, dispatcher_host_->ipc_sink()->message_count());
  EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
      EmbeddedWorkerMsg_StartWorker::ID));
  EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ServiceWorkerRegistered::ID));
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableServiceWorker));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableServiceWorker);

  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_RegisterServiceWorker(-1, -1, -1, GURL(), GURL()));

  // TODO(alecflett): Pump the message loop when this becomes async.
  ASSERT_EQ(1UL, dispatcher_host_->ipc_sink()->message_count());
  EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_ServiceWorkerRegistrationError::ID));
}

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  const int kProviderId = 1001;  // Test with a value != kRenderProcessId.

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId));
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Two with the same ID should be seen as a bad message.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));

  // Destroying an ID that does not exist warrants a bad message.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Deletion of the dispatcher_host should cause providers for that
  // process to get deleted as well.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderCreated(kProviderId));
  EXPECT_TRUE(context()->GetProviderHost(kRenderProcessId, kProviderId));
  EXPECT_TRUE(dispatcher_host_->HasOneRef());
  dispatcher_host_ = NULL;
  EXPECT_FALSE(context()->GetProviderHost(kRenderProcessId, kProviderId));
}

}  // namespace content
