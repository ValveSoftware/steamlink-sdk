// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_dispatcher_host.h"

#include <stdint.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_handle.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static void SaveStatusCallback(bool* called,
                               ServiceWorkerStatusCode* out,
                               ServiceWorkerStatusCode status) {
  *called = true;
  *out = status;
}

void SetUpDummyMessagePort(std::vector<int>* ports) {
  int port_id = -1;
  MessagePortService::GetInstance()->Create(MSG_ROUTING_NONE, nullptr,
                                            &port_id);
  ports->push_back(port_id);
}

}  // namespace

static const int kRenderFrameId = 1;

class TestingServiceWorkerDispatcherHost : public ServiceWorkerDispatcherHost {
 public:
  TestingServiceWorkerDispatcherHost(
      int process_id,
      ServiceWorkerContextWrapper* context_wrapper,
      ResourceContext* resource_context,
      EmbeddedWorkerTestHelper* helper)
      : ServiceWorkerDispatcherHost(process_id, NULL, resource_context),
        bad_messages_received_count_(0),
        helper_(helper) {
    Init(context_wrapper);
  }

  bool Send(IPC::Message* message) override { return helper_->Send(message); }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  void ShutdownForBadMessage() override { ++bad_messages_received_count_; }

  int bad_messages_received_count_;

 protected:
  EmbeddedWorkerTestHelper* helper_;
  ~TestingServiceWorkerDispatcherHost() override {}
};

class FailToStartWorkerTestHelper : public EmbeddedWorkerTestHelper {
 public:
  FailToStartWorkerTestHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}

  void OnStartWorker(int embedded_worker_id,
                     int64_t service_worker_version_id,
                     const GURL& scope,
                     const GURL& script_url,
                     bool pause_after_download) override {
    EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
  }
};

class ServiceWorkerDispatcherHostTest : public testing::Test {
 protected:
  ServiceWorkerDispatcherHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    Initialize(
        base::WrapUnique(new EmbeddedWorkerTestHelper(base::FilePath())));
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() {
    return helper_->context_wrapper();
  }

  void Initialize(std::unique_ptr<EmbeddedWorkerTestHelper> helper) {
    helper_.reset(helper.release());
    dispatcher_host_ = new TestingServiceWorkerDispatcherHost(
        helper_->mock_render_process_id(), context_wrapper(),
        &resource_context_, helper_.get());
  }

  void SetUpRegistration(const GURL& scope, const GURL& script_url) {
    registration_ = new ServiceWorkerRegistration(
        scope, 1L, helper_->context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), script_url, 1L,
                                        helper_->context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version_->script_url(), 100));
    version_->script_cache_map()->SetResources(records);

    // Make the registration findable via storage functions.
    helper_->context()->storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    bool called = false;
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
    helper_->context()->storage()->StoreRegistration(
        registration_.get(), version_.get(),
        base::Bind(&SaveStatusCallback, &called, &status));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  void SendSetHostedVersionId(int provider_id,
                              int64_t version_id,
                              int embedded_worker_id) {
    dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_SetVersionId(
        provider_id, version_id, embedded_worker_id));
  }

  void SendProviderCreated(ServiceWorkerProviderType type,
                           const GURL& pattern) {
    const int64_t kProviderId = 99;
    dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
        kProviderId, MSG_ROUTING_NONE, type,
        true /* is_parent_frame_secure */));
    helper_->SimulateAddProcessToPattern(pattern,
                                         helper_->mock_render_process_id());
    provider_host_ = context()->GetProviderHost(
        helper_->mock_render_process_id(), kProviderId);
  }

  void SendRegister(int64_t provider_id, GURL pattern, GURL worker_url) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_RegisterServiceWorker(
            -1, -1, provider_id, pattern, worker_url));
    base::RunLoop().RunUntilIdle();
  }

  void Register(int64_t provider_id,
                GURL pattern,
                GURL worker_url,
                uint32_t expected_message) {
    SendRegister(provider_id, pattern, worker_url);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendUnregister(int64_t provider_id, int64_t registration_id) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_UnregisterServiceWorker(-1, -1, provider_id,
                                                     registration_id));
    base::RunLoop().RunUntilIdle();
  }

  void Unregister(int64_t provider_id,
                  int64_t registration_id,
                  uint32_t expected_message) {
    SendUnregister(provider_id, registration_id);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendGetRegistration(int64_t provider_id, GURL document_url) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_GetRegistration(
            -1, -1, provider_id, document_url));
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistration(int64_t provider_id,
                       GURL document_url,
                       uint32_t expected_message) {
    SendGetRegistration(provider_id, document_url);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void SendGetRegistrations(int64_t provider_id) {
    dispatcher_host_->OnMessageReceived(
        ServiceWorkerHostMsg_GetRegistrations(-1, -1, provider_id));
    base::RunLoop().RunUntilIdle();
  }

  void GetRegistrations(int64_t provider_id, uint32_t expected_message) {
    SendGetRegistrations(provider_id);
    EXPECT_TRUE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
        expected_message));
    dispatcher_host_->ipc_sink()->ClearMessages();
  }

  void DispatchExtendableMessageEvent(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<int>& sent_message_ports,
      ServiceWorkerProviderHost* sender_provider_host,
      const ServiceWorkerDispatcherHost::StatusCallback& callback) {
    dispatcher_host_->DispatchExtendableMessageEvent(
        std::move(worker), message, source_origin, sent_message_ports,
        sender_provider_host, callback);
  }

  ServiceWorkerProviderHost* CreateServiceWorkerProviderHost(int provider_id) {
    return new ServiceWorkerProviderHost(
        helper_->mock_render_process_id(), kRenderFrameId, provider_id,
        SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
        context()->AsWeakPtr(), dispatcher_host_.get());
  }

  TestBrowserThreadBundle browser_thread_bundle_;
  content::MockResourceContext resource_context_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<TestingServiceWorkerDispatcherHost> dispatcher_host_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  ServiceWorkerProviderHost* provider_host_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() {}
  bool AllowServiceWorker(const GURL& scope,
                          const GURL& first_party,
                          content::ResourceContext* context,
                          int render_process_id,
                          int render_frame_id) override {
    return false;
  }
};

TEST_F(ServiceWorkerDispatcherHostTest,
       Register_ContentSettingsDisallowsServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  Register(kProviderId,
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
  GetRegistration(kProviderId,
                  GURL("https://www.example.com/"),
                  ServiceWorkerMsg_ServiceWorkerGetRegistrationError::ID);
  GetRegistrations(kProviderId,
                   ServiceWorkerMsg_ServiceWorkerGetRegistrationsError::ID);

  // Add a registration into a live registration map so that Unregister() can
  // find it.
  const int64_t kRegistrationId = 999;  // Dummy value
  scoped_refptr<ServiceWorkerRegistration> registration(
      new ServiceWorkerRegistration(GURL("https://www.example.com/"),
                                    kRegistrationId, context()->AsWeakPtr()));
  Unregister(kProviderId, kRegistrationId,
             ServiceWorkerMsg_ServiceWorkerUnregistrationError::ID);

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_HTTPS) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  Register(kProviderId,
           GURL("https://www.example.com/"),
           GURL("https://www.example.com/bar"),
           ServiceWorkerMsg_ServiceWorkerRegistered::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_NonSecureTransportLocalhost) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://127.0.0.3:81/foo"));
  context()->AddProviderHost(std::move(host));

  Register(kProviderId,
           GURL("http://127.0.0.3:81/bar"),
           GURL("http://127.0.0.3:81/baz"),
           ServiceWorkerMsg_ServiceWorkerRegistered::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_InvalidScopeShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendRegister(kProviderId, GURL(""),
               GURL("https://www.example.com/bar/hoge.js"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_InvalidScriptShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendRegister(kProviderId, GURL("https://www.example.com/bar/"), GURL(""));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_NonSecureOriginShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendRegister(kProviderId,
               GURL("http://www.example.com/"),
               GURL("http://www.example.com/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_CrossOriginShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  // Script has a different host
  SendRegister(kProviderId,
               GURL("https://www.example.com/"),
               GURL("https://foo.example.com/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  // Scope has a different host
  SendRegister(kProviderId,
               GURL("https://foo.example.com/"),
               GURL("https://www.example.com/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Script has a different port
  SendRegister(kProviderId,
               GURL("https://www.example.com/"),
               GURL("https://www.example.com:8080/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);

  // Scope has a different transport
  SendRegister(kProviderId,
               GURL("wss://www.example.com/"),
               GURL("https://www.example.com/bar"));
  EXPECT_EQ(4, dispatcher_host_->bad_messages_received_count_);

  // Script and scope have a different host but match each other
  SendRegister(kProviderId,
               GURL("https://foo.example.com/"),
               GURL("https://foo.example.com/bar"));
  EXPECT_EQ(5, dispatcher_host_->bad_messages_received_count_);

  // Script and scope URLs are invalid
  SendRegister(kProviderId,
               GURL(),
               GURL("h@ttps://@"));
  EXPECT_EQ(6, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, Register_BadCharactersShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/"));
  context()->AddProviderHost(std::move(host));

  SendRegister(kProviderId, GURL("https://www.example.com/%2f"),
               GURL("https://www.example.com/"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId, GURL("https://www.example.com/%2F"),
               GURL("https://www.example.com/"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId, GURL("https://www.example.com/"),
               GURL("https://www.example.com/%2f"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId, GURL("https://www.example.com/%5c"),
               GURL("https://www.example.com/"));
  EXPECT_EQ(4, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId, GURL("https://www.example.com/"),
               GURL("https://www.example.com/%5c"));
  EXPECT_EQ(5, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId, GURL("https://www.example.com/"),
               GURL("https://www.example.com/%5C"));
  EXPECT_EQ(6, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       Register_FileSystemDocumentShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("filesystem:https://www.example.com/temporary/a"));
  context()->AddProviderHost(std::move(host));

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("https://www.example.com/temporary/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       Register_FileSystemScriptOrScopeShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/temporary/"));
  context()->AddProviderHost(std::move(host));

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("https://www.example.com/temporary/bar"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  SendRegister(kProviderId,
               GURL("filesystem:https://www.example.com/temporary/"),
               GURL("filesystem:https://www.example.com/temporary/bar"));
  EXPECT_EQ(3, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  Register(-1,
           GURL(),
           GURL(),
           ServiceWorkerMsg_ServiceWorkerRegistrationError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, ProviderCreatedAndDestroyed) {
  const int kProviderId = 1001;
  int process_id = helper_->mock_render_process_id();

  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      true /* is_parent_frame_secure */));
  EXPECT_TRUE(context()->GetProviderHost(process_id, kProviderId));

  // Two with the same ID should be seen as a bad message.
  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      true /* is_parent_frame_secure */));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);

  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_FALSE(context()->GetProviderHost(process_id, kProviderId));

  // Destroying an ID that does not exist warrants a bad message.
  dispatcher_host_->OnMessageReceived(
      ServiceWorkerHostMsg_ProviderDestroyed(kProviderId));
  EXPECT_EQ(2, dispatcher_host_->bad_messages_received_count_);

  // Deletion of the dispatcher_host should cause providers for that
  // process to get deleted as well.
  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      kProviderId, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      true /* is_parent_frame_secure */));
  EXPECT_TRUE(context()->GetProviderHost(process_id, kProviderId));
  EXPECT_TRUE(dispatcher_host_->HasOneRef());
  dispatcher_host_ = NULL;
  EXPECT_FALSE(context()->GetProviderHost(process_id, kProviderId));
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistration_SameOrigin) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  GetRegistration(kProviderId,
                  GURL("https://www.example.com/"),
                  ServiceWorkerMsg_DidGetRegistration::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistration_CrossOriginShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendGetRegistration(kProviderId, GURL("https://foo.example.com/"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       GetRegistration_InvalidScopeShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendGetRegistration(kProviderId, GURL(""));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       GetRegistration_NonSecureOriginShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendGetRegistration(kProviderId, GURL("http://www.example.com/"));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistration_EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  GetRegistration(-1,
                  GURL(),
                  ServiceWorkerMsg_ServiceWorkerGetRegistrationError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistrations_SecureOrigin) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("https://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  GetRegistrations(kProviderId, ServiceWorkerMsg_DidGetRegistrations::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest,
       GetRegistrations_NonSecureOriginShouldFail) {
  const int64_t kProviderId = 99;  // Dummy value
  std::unique_ptr<ServiceWorkerProviderHost> host(
      CreateServiceWorkerProviderHost(kProviderId));
  host->SetDocumentUrl(GURL("http://www.example.com/foo"));
  context()->AddProviderHost(std::move(host));

  SendGetRegistrations(kProviderId);
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, GetRegistrations_EarlyContextDeletion) {
  helper_->ShutdownContext();

  // Let the shutdown reach the simulated IO thread.
  base::RunLoop().RunUntilIdle();

  GetRegistrations(-1, ServiceWorkerMsg_ServiceWorkerGetRegistrationsError::ID);
}

TEST_F(ServiceWorkerDispatcherHostTest, CleanupOnRendererCrash) {
  GURL pattern = GURL("http://www.example.com/");
  GURL script_url = GURL("http://www.example.com/service_worker.js");
  int process_id = helper_->mock_render_process_id();

  SendProviderCreated(SERVICE_WORKER_PROVIDER_FOR_WINDOW, pattern);
  SetUpRegistration(pattern, script_url);
  int64_t provider_id = provider_host_->provider_id();

  // Start up the worker.
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_ABORT;
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::Bind(&SaveStatusCallback, &called, &status));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  EXPECT_TRUE(context()->GetProviderHost(process_id, provider_id));
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, version_->running_status());

  // Simulate the render process crashing.
  dispatcher_host_->OnFilterRemoved();

  // The dispatcher host should clean up the state from the process.
  EXPECT_FALSE(context()->GetProviderHost(process_id, provider_id));
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, version_->running_status());

  // We should be able to hook up a new dispatcher host although the old object
  // is not yet destroyed. This is what the browser does when reusing a crashed
  // render process.
  scoped_refptr<TestingServiceWorkerDispatcherHost> new_dispatcher_host(
      new TestingServiceWorkerDispatcherHost(
          process_id, context_wrapper(), &resource_context_, helper_.get()));

  // To show the new dispatcher can operate, simulate provider creation. Since
  // the old dispatcher cleaned up the old provider host, the new one won't
  // complain.
  new_dispatcher_host->OnMessageReceived(ServiceWorkerHostMsg_ProviderCreated(
      provider_id, MSG_ROUTING_NONE, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
      true /* is_parent_frame_secure */));
  EXPECT_EQ(0, new_dispatcher_host->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, DispatchExtendableMessageEvent) {
  GURL pattern = GURL("http://www.example.com/");
  GURL script_url = GURL("http://www.example.com/service_worker.js");

  SendProviderCreated(SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, pattern);
  SetUpRegistration(pattern, script_url);

  // Set the running hosted version so that we can retrieve a valid service
  // worker object information for the source attribute of the message event.
  provider_host_->running_hosted_version_ = version_;

  // Set aside the initial refcount of the worker handle.
  provider_host_->GetOrCreateServiceWorkerHandle(version_.get());
  ServiceWorkerHandle* sender_worker_handle =
      dispatcher_host_->FindServiceWorkerHandle(provider_host_->provider_id(),
                                                version_->version_id());
  const int ref_count = sender_worker_handle->ref_count();

  // Dispatch ExtendableMessageEvent.
  std::vector<int> ports;
  SetUpDummyMessagePort(&ports);
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  DispatchExtendableMessageEvent(
      version_, base::string16(), url::Origin(version_->scope().GetOrigin()),
      ports, provider_host_, base::Bind(&SaveStatusCallback, &called, &status));
  for (int port : ports)
    EXPECT_TRUE(MessagePortService::GetInstance()->AreMessagesHeld(port));
  EXPECT_EQ(ref_count + 1, sender_worker_handle->ref_count());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // Messages should be held until ports are created at the destination.
  for (int port : ports)
    EXPECT_TRUE(MessagePortService::GetInstance()->AreMessagesHeld(port));

  EXPECT_EQ(ref_count + 1, sender_worker_handle->ref_count());
}

TEST_F(ServiceWorkerDispatcherHostTest, DispatchExtendableMessageEvent_Fail) {
  GURL pattern = GURL("http://www.example.com/");
  GURL script_url = GURL("http://www.example.com/service_worker.js");

  Initialize(base::WrapUnique(new FailToStartWorkerTestHelper));
  SendProviderCreated(SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, pattern);
  SetUpRegistration(pattern, script_url);

  // Set the running hosted version so that we can retrieve a valid service
  // worker object information for the source attribute of the message event.
  provider_host_->running_hosted_version_ = version_;

  // Set aside the initial refcount of the worker handle.
  provider_host_->GetOrCreateServiceWorkerHandle(version_.get());
  ServiceWorkerHandle* sender_worker_handle =
      dispatcher_host_->FindServiceWorkerHandle(provider_host_->provider_id(),
                                                version_->version_id());
  const int ref_count = sender_worker_handle->ref_count();

  // Try to dispatch ExtendableMessageEvent. This should fail to start the
  // worker and to dispatch the event.
  std::vector<int> ports;
  SetUpDummyMessagePort(&ports);
  bool called = false;
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  DispatchExtendableMessageEvent(
      version_, base::string16(), url::Origin(version_->scope().GetOrigin()),
      ports, provider_host_, base::Bind(&SaveStatusCallback, &called, &status));
  for (int port : ports)
    EXPECT_TRUE(MessagePortService::GetInstance()->AreMessagesHeld(port));
  EXPECT_EQ(ref_count + 1, sender_worker_handle->ref_count());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_EQ(SERVICE_WORKER_ERROR_START_WORKER_FAILED, status);

  // The error callback should clean up the ports and handle.
  for (int port : ports)
    EXPECT_FALSE(MessagePortService::GetInstance()->AreMessagesHeld(port));
  EXPECT_EQ(ref_count, sender_worker_handle->ref_count());
}

TEST_F(ServiceWorkerDispatcherHostTest, OnSetHostedVersionId) {
  GURL pattern = GURL("http://www.example.com/");
  GURL script_url = GURL("http://www.example.com/service_worker.js");

  Initialize(base::WrapUnique(new FailToStartWorkerTestHelper));
  SendProviderCreated(SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, pattern);
  SetUpRegistration(pattern, script_url);

  const int64_t kProviderId = 99;  // Dummy value
  bool called;
  ServiceWorkerStatusCode status;
  // StartWorker puts the worker in STARTING state but it will have no
  // process id yet.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::Bind(&SaveStatusCallback, &called, &status));
  EXPECT_NE(version_->embedded_worker()->process_id(),
            provider_host_->process_id());
  // SendSetHostedVersionId should reject because the provider host process id
  // is different. It should call BadMessageReceived because it's not an
  // expected error state.
  SendSetHostedVersionId(kProviderId, version_->version_id(),
                         version_->embedded_worker()->embedded_worker_id());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_AssociateRegistration::ID));
  EXPECT_EQ(1, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, OnSetHostedVersionId_DetachedWorker) {
  GURL pattern = GURL("http://www.example.com/");
  GURL script_url = GURL("http://www.example.com/service_worker.js");

  Initialize(base::WrapUnique(new FailToStartWorkerTestHelper));
  SendProviderCreated(SERVICE_WORKER_PROVIDER_FOR_CONTROLLER, pattern);
  SetUpRegistration(pattern, script_url);

  const int64_t kProviderId = 99;  // Dummy value
  bool called;
  ServiceWorkerStatusCode status;
  // StartWorker puts the worker in STARTING state.
  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::Bind(&SaveStatusCallback, &called, &status));

  // SendSetHostedVersionId should bail because the embedded worker is
  // different. It shouldn't call BadMessageReceived because receiving a message
  // for a detached worker is a legitimite possibility.
  int bad_embedded_worker_id =
      version_->embedded_worker()->embedded_worker_id() + 1;
  SendSetHostedVersionId(kProviderId, version_->version_id(),
                         bad_embedded_worker_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(dispatcher_host_->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_AssociateRegistration::ID));
  EXPECT_EQ(0, dispatcher_host_->bad_messages_received_count_);
}

TEST_F(ServiceWorkerDispatcherHostTest, ReceivedTimedOutRequestResponse) {
  GURL pattern = GURL("https://www.example.com/");
  GURL script_url = GURL("https://www.example.com/service_worker.js");

  SendProviderCreated(SERVICE_WORKER_PROVIDER_FOR_WINDOW, pattern);
  SetUpRegistration(pattern, script_url);

  version_->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Set the worker status to STOPPING.
  version_->embedded_worker()->Stop();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, version_->running_status());

  // Receive a response for a timed out request. The bad message count should
  // not increase.
  const int kRequestId = 91;  // Dummy value
  dispatcher_host_->OnMessageReceived(ServiceWorkerHostMsg_FetchEventResponse(
      version_->embedded_worker()->embedded_worker_id(), kRequestId,
      SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK, ServiceWorkerResponse()));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, dispatcher_host_->bad_messages_received_count_);
}

}  // namespace content
