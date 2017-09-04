// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/fetch_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_test_sink.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/interfaces/interface_provider.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class GURL;
struct ServiceWorkerMsg_ExtendableMessageEvent_Params;

namespace service_manager {
class InterfaceProvider;
class InterfaceRegistry;
}

namespace content {

class EmbeddedWorkerRegistry;
class EmbeddedWorkerTestHelper;
class MessagePortMessageFilter;
class MockRenderProcessHost;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class TestBrowserContext;
struct EmbeddedWorkerStartParams;
struct PushEventPayload;
struct ServiceWorkerFetchRequest;

// In-Process EmbeddedWorker test helper.
//
// Usage: create an instance of this class to test browser-side embedded worker
// code without creating a child process.  This class will create a
// ServiceWorkerContextWrapper and ServiceWorkerContextCore for you.
//
// By default this class just notifies back WorkerStarted and WorkerStopped
// for StartWorker and StopWorker requests. The default implementation
// also returns success for event messages (e.g. InstallEvent, FetchEvent).
//
// Alternatively consumers can subclass this helper and override On*()
// methods to add their own logic/verification code.
//
// See embedded_worker_instance_unittest.cc for example usages.
//
class EmbeddedWorkerTestHelper : public IPC::Sender,
                                 public IPC::Listener {
 public:
  using FetchCallback =
      base::Callback<void(ServiceWorkerStatusCode,
                          base::Time /* dispatch_event_time */)>;

  class MockEmbeddedWorkerInstanceClient
      : public mojom::EmbeddedWorkerInstanceClient {
   public:
    explicit MockEmbeddedWorkerInstanceClient(
        base::WeakPtr<EmbeddedWorkerTestHelper> helper);
    ~MockEmbeddedWorkerInstanceClient() override;

    static void Bind(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     mojom::EmbeddedWorkerInstanceClientRequest request);

   protected:
    // Implementation of mojo interfaces.
    void StartWorker(
        const EmbeddedWorkerStartParams& params,
        service_manager::mojom::InterfaceProviderPtr browser_interfaces,
        service_manager::mojom::InterfaceProviderRequest renderer_request)
        override;
    void StopWorker(const StopWorkerCallback& callback) override;

    base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
    mojo::Binding<mojom::EmbeddedWorkerInstanceClient> binding_;

    base::Optional<int> embedded_worker_id_;

   private:
    DISALLOW_COPY_AND_ASSIGN(MockEmbeddedWorkerInstanceClient);
  };

  // If |user_data_directory| is empty, the context makes storage stuff in
  // memory.
  explicit EmbeddedWorkerTestHelper(const base::FilePath& user_data_directory);
  ~EmbeddedWorkerTestHelper() override;

  // Call this to simulate add/associate a process to a pattern.
  // This also registers this sender for the process.
  void SimulateAddProcessToPattern(const GURL& pattern, int process_id);

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Register a mojo endpoint object derived from
  // MockEmbeddedWorkerInstanceClient.
  void RegisterMockInstanceClient(
      std::unique_ptr<MockEmbeddedWorkerInstanceClient> client);

  template <typename MockType, typename... Args>
  MockType* CreateAndRegisterMockInstanceClient(Args&&... args);

  // IPC sink for EmbeddedWorker messages.
  IPC::TestSink* ipc_sink() { return &sink_; }
  // Inner IPC sink for script context messages sent via EmbeddedWorker.
  IPC::TestSink* inner_ipc_sink() { return &inner_sink_; }

  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>*
  mock_instance_clients() {
    return &mock_instance_clients_;
  }

  ServiceWorkerContextCore* context();
  ServiceWorkerContextWrapper* context_wrapper() { return wrapper_.get(); }
  void ShutdownContext();

  int GetNextThreadId() { return next_thread_id_++; }

  int mock_render_process_id() const { return mock_render_process_id_; }
  MockRenderProcessHost* mock_render_process_host() {
    return render_process_host_.get();
  }

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map() {
    return embedded_worker_id_service_worker_version_id_map_;
  }

  // Only used for tests that force creating a new render process.
  int new_render_process_id() const { return new_mock_render_process_id_; }

  TestBrowserContext* browser_context() { return browser_context_.get(); }

  base::WeakPtr<EmbeddedWorkerTestHelper> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  // Called when StartWorker, StopWorker and SendMessageToWorker message
  // is sent to the embedded worker. Override if necessary. By default
  // they verify given parameters and:
  // - OnStartWorker calls SimulateWorkerStarted
  // - OnStopWorker calls SimulateWorkerStoped
  // - OnSendMessageToWorker calls the message's respective On*Event handler
  virtual void OnStartWorker(int embedded_worker_id,
                             int64_t service_worker_version_id,
                             const GURL& scope,
                             const GURL& script_url,
                             bool pause_after_download);
  virtual void OnResumeAfterDownload(int embedded_worker_id);
  virtual void OnStopWorker(int embedded_worker_id);
  virtual bool OnMessageToWorker(int thread_id,
                                 int embedded_worker_id,
                                 const IPC::Message& message);

  // Called to setup mojo for a new embedded worker. Override to register
  // interfaces the worker should expose to the browser.
  virtual void OnSetupMojo(
      int thread_id,
      service_manager::InterfaceRegistry* interface_registry);

  // On*Event handlers. Called by the default implementation of
  // OnMessageToWorker when events are sent to the embedded
  // worker. By default they just return success via
  // SimulateSendReplyToBrowser.
  virtual void OnActivateEvent(int embedded_worker_id, int request_id);
  virtual void OnExtendableMessageEvent(int embedded_worker_id, int request_id);
  virtual void OnInstallEvent(int embedded_worker_id, int request_id);
  virtual void OnFetchEvent(int embedded_worker_id,
                            int fetch_event_id,
                            const ServiceWorkerFetchRequest& request,
                            mojom::FetchEventPreloadHandlePtr preload_handle,
                            const FetchCallback& callback);
  virtual void OnPushEvent(int embedded_worker_id,
                           int request_id,
                           const PushEventPayload& payload);

  // These functions simulate sending an EmbeddedHostMsg message to the
  // browser.
  void SimulateWorkerReadyForInspection(int embedded_worker_id);
  void SimulateWorkerScriptCached(int embedded_worker_id);
  void SimulateWorkerScriptLoaded(int embedded_worker_id);
  void SimulateWorkerThreadStarted(int thread_id, int embedded_worker_id);
  void SimulateWorkerScriptEvaluated(int embedded_worker_id, bool success);
  void SimulateWorkerStarted(int embedded_worker_id);
  void SimulateWorkerStopped(int embedded_worker_id);
  void SimulateSend(IPC::Message* message);

  EmbeddedWorkerRegistry* registry();

 private:
  using InterfaceRegistryAndProvider =
      std::pair<std::unique_ptr<service_manager::InterfaceRegistry>,
                std::unique_ptr<service_manager::InterfaceProvider>>;

  class MockEmbeddedWorkerSetup;
  class MockFetchEventDispatcher;

  void OnStartWorkerStub(const EmbeddedWorkerStartParams& params);
  void OnResumeAfterDownloadStub(int embedded_worker_id);
  void OnStopWorkerStub(int embedded_worker_id);
  void OnMessageToWorkerStub(int thread_id,
                             int embedded_worker_id,
                             const IPC::Message& message);
  void OnActivateEventStub(int request_id);
  void OnExtendableMessageEventStub(
      int request_id,
      const ServiceWorkerMsg_ExtendableMessageEvent_Params& params);
  void OnInstallEventStub(int request_id);
  void OnFetchEventStub(int thread_id,
                        int fetch_event_id,
                        const ServiceWorkerFetchRequest& request,
                        mojom::FetchEventPreloadHandlePtr preload_handle,
                        const FetchCallback& callback);
  void OnPushEventStub(int request_id, const PushEventPayload& payload);
  void OnSetupMojoStub(
      int thread_id,
      service_manager::mojom::InterfaceProviderRequest services,
      service_manager::mojom::InterfaceProviderPtr exposed_services);

  MessagePortMessageFilter* NewMessagePortMessageFilter();

  std::unique_ptr<service_manager::InterfaceRegistry> CreateInterfaceRegistry(
      MockRenderProcessHost* rph);

  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> render_process_host_;
  std::unique_ptr<MockRenderProcessHost> new_render_process_host_;

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;

  IPC::TestSink sink_;
  IPC::TestSink inner_sink_;

  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>
      mock_instance_clients_;
  size_t mock_instance_clients_next_index_;

  int next_thread_id_;
  int mock_render_process_id_;
  int new_mock_render_process_id_;

  std::unique_ptr<service_manager::InterfaceRegistry>
      render_process_interface_registry_;
  std::unique_ptr<service_manager::InterfaceRegistry>
      new_render_process_interface_registry_;

  std::map<int, int64_t> embedded_worker_id_service_worker_version_id_map_;
  std::map<int /* thread_id */, int /* embedded_worker_id */>
      thread_id_embedded_worker_id_map_;

  // Stores the InterfaceRegistry/InterfaceProviders that are associated with
  // each individual service worker.
  base::hash_map<int, InterfaceRegistryAndProvider>
      thread_id_service_registry_map_;

  // Updated each time MessageToWorker message is received.
  int current_embedded_worker_id_;

  std::vector<scoped_refptr<MessagePortMessageFilter>>
      message_port_message_filters_;

  base::WeakPtrFactory<EmbeddedWorkerTestHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerTestHelper);
};

template <typename MockType, typename... Args>
MockType* EmbeddedWorkerTestHelper::CreateAndRegisterMockInstanceClient(
    Args&&... args) {
  std::unique_ptr<MockType> mock =
      base::MakeUnique<MockType>(std::forward<Args>(args)...);
  MockType* mock_rawptr = mock.get();
  RegisterMockInstanceClient(std::move(mock));
  return mock_rawptr;
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_TEST_HELPER_H_
