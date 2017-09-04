// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include <map>
#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/common/service_worker/fetch_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class MockMessagePortMessageFilter : public MessagePortMessageFilter {
 public:
  MockMessagePortMessageFilter()
      : MessagePortMessageFilter(
            base::Bind(&base::AtomicSequenceNumber::GetNext,
                       base::Unretained(&next_routing_id_))) {}

  bool Send(IPC::Message* message) override {
    message_queue_.push_back(message);
    return true;
  }

 private:
  ~MockMessagePortMessageFilter() override {}
  base::AtomicSequenceNumber next_routing_id_;
  ScopedVector<IPC::Message> message_queue_;
};

}  // namespace

class EmbeddedWorkerTestHelper::MockEmbeddedWorkerSetup
    : public mojom::EmbeddedWorkerSetup {
 public:
  explicit MockEmbeddedWorkerSetup(
      const base::WeakPtr<EmbeddedWorkerTestHelper>& helper)
      : helper_(helper) {}

  static void Create(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     mojom::EmbeddedWorkerSetupRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<MockEmbeddedWorkerSetup>(helper),
                            std::move(request));
  }

  void ExchangeInterfaceProviders(
      int32_t thread_id,
      service_manager::mojom::InterfaceProviderRequest request,
      service_manager::mojom::InterfaceProviderPtr remote_interfaces) override {
    if (!helper_)
      return;
    helper_->OnSetupMojoStub(thread_id, std::move(request),
                             std::move(remote_interfaces));
  }

 private:
  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
};

EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    MockEmbeddedWorkerInstanceClient(
        base::WeakPtr<EmbeddedWorkerTestHelper> helper)
    : helper_(helper), binding_(this) {}

EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::
    ~MockEmbeddedWorkerInstanceClient() {}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StartWorker(
    const EmbeddedWorkerStartParams& params,
    service_manager::mojom::InterfaceProviderPtr browser_interfaces,
    service_manager::mojom::InterfaceProviderRequest renderer_request) {
  if (!helper_)
    return;

  embedded_worker_id_ = params.embedded_worker_id;

  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker, helper_->AsWeakPtr(),
                 params.embedded_worker_id, params.service_worker_version_id,
                 params.scope, params.script_url, params.pause_after_download));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnSetupMojoStub,
                            helper_->AsWeakPtr(), worker->thread_id(),
                            base::Passed(&renderer_request),
                            base::Passed(&browser_interfaces)));
}

void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::StopWorker(
    const StopWorkerCallback& callback) {
  if (!helper_)
    return;

  ASSERT_TRUE(embedded_worker_id_);
  EmbeddedWorkerInstance* worker =
      helper_->registry()->GetWorker(embedded_worker_id_.value());
  // |worker| is possible to be null when corresponding EmbeddedWorkerInstance
  // is removed right after sending StopWorker.
  if (worker)
    EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  callback.Run();
}

// static
void EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient::Bind(
    const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
    mojom::EmbeddedWorkerInstanceClientRequest request) {
  std::vector<std::unique_ptr<MockEmbeddedWorkerInstanceClient>>* clients =
      helper->mock_instance_clients();
  size_t next_client_index = helper->mock_instance_clients_next_index_;

  ASSERT_GE(clients->size(), next_client_index);
  if (clients->size() == next_client_index) {
    clients->push_back(
        base::MakeUnique<MockEmbeddedWorkerInstanceClient>(helper));
  }

  std::unique_ptr<MockEmbeddedWorkerInstanceClient>& client =
      clients->at(next_client_index);
  helper->mock_instance_clients_next_index_ = next_client_index + 1;
  if (client)
    client->binding_.Bind(std::move(request));
}

class EmbeddedWorkerTestHelper::MockFetchEventDispatcher
    : public NON_EXPORTED_BASE(mojom::FetchEventDispatcher) {
 public:
  static void Create(const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
                     int thread_id,
                     mojom::FetchEventDispatcherRequest request) {
    mojo::MakeStrongBinding(
        base::MakeUnique<MockFetchEventDispatcher>(helper, thread_id),
        std::move(request));
  }

  MockFetchEventDispatcher(
      const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
      int thread_id)
      : helper_(helper), thread_id_(thread_id) {}

  ~MockFetchEventDispatcher() override {}

  void DispatchFetchEvent(int fetch_event_id,
                          const ServiceWorkerFetchRequest& request,
                          mojom::FetchEventPreloadHandlePtr preload_handle,
                          const DispatchFetchEventCallback& callback) override {
    if (!helper_)
      return;
    helper_->OnFetchEventStub(thread_id_, fetch_event_id, request,
                              std::move(preload_handle), callback);
  }

 private:
  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  const int thread_id_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(new TestBrowserContext),
      render_process_host_(new MockRenderProcessHost(browser_context_.get())),
      new_render_process_host_(
          new MockRenderProcessHost(browser_context_.get())),
      wrapper_(new ServiceWorkerContextWrapper(browser_context_.get())),
      mock_instance_clients_next_index_(0),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
      new_mock_render_process_id_(new_render_process_host_->GetID()),
      weak_factory_(this) {
  std::unique_ptr<MockServiceWorkerDatabaseTaskManager> database_task_manager(
      new MockServiceWorkerDatabaseTaskManager(
          base::ThreadTaskRunnerHandle::Get()));
  wrapper_->InitInternal(user_data_directory, std::move(database_task_manager),
                         base::ThreadTaskRunnerHandle::Get(), nullptr, nullptr);
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id());
  wrapper_->process_manager()->SetNewProcessIdForTest(new_render_process_id());
  registry()->AddChildProcessSender(mock_render_process_id_, this,
                                    NewMessagePortMessageFilter());

  // Setup process level interface registry.
  render_process_interface_registry_ =
      CreateInterfaceRegistry(render_process_host_.get());
  new_render_process_interface_registry_ =
      CreateInterfaceRegistry(new_render_process_host_.get());
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  if (wrapper_.get())
    wrapper_->Shutdown();
}

void EmbeddedWorkerTestHelper::SimulateAddProcessToPattern(const GURL& pattern,
                                                           int process_id) {
  registry()->AddChildProcessSender(process_id, this,
                                    NewMessagePortMessageFilter());
  wrapper_->process_manager()->AddProcessReferenceToPattern(pattern,
                                                            process_id);
}

bool EmbeddedWorkerTestHelper::Send(IPC::Message* message) {
  OnMessageReceived(*message);
  delete message;
  return true;
}

bool EmbeddedWorkerTestHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StartWorker, OnStartWorkerStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_ResumeAfterDownload,
                        OnResumeAfterDownloadStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StopWorker, OnStopWorkerStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_MessageToWorker,
                        OnMessageToWorkerStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // IPC::TestSink only records messages that are not handled by filters,
  // so we just forward all messages to the separate sink.
  sink_.OnMessageReceived(message);

  return handled;
}

void EmbeddedWorkerTestHelper::RegisterMockInstanceClient(
    std::unique_ptr<MockEmbeddedWorkerInstanceClient> client) {
  mock_instance_clients_.push_back(std::move(client));
}

ServiceWorkerContextCore* EmbeddedWorkerTestHelper::context() {
  return wrapper_->context();
}

void EmbeddedWorkerTestHelper::ShutdownContext() {
  wrapper_->Shutdown();
  wrapper_ = NULL;
}

void EmbeddedWorkerTestHelper::OnStartWorker(int embedded_worker_id,
                                             int64_t service_worker_version_id,
                                             const GURL& scope,
                                             const GURL& script_url,
                                             bool pause_after_download) {
  embedded_worker_id_service_worker_version_id_map_[embedded_worker_id] =
      service_worker_version_id;
  SimulateWorkerReadyForInspection(embedded_worker_id);
  SimulateWorkerScriptCached(embedded_worker_id);
  SimulateWorkerScriptLoaded(embedded_worker_id);
  if (!pause_after_download)
    OnResumeAfterDownload(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownload(int embedded_worker_id) {
  SimulateWorkerThreadStarted(GetNextThreadId(), embedded_worker_id);
  SimulateWorkerScriptEvaluated(embedded_worker_id, true /* success */);
  SimulateWorkerStarted(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnStopWorker(int embedded_worker_id) {
  // By default just notify the sender that the worker is stopped.
  SimulateWorkerStopped(embedded_worker_id);
}

bool EmbeddedWorkerTestHelper::OnMessageToWorker(int thread_id,
                                                 int embedded_worker_id,
                                                 const IPC::Message& message) {
  bool handled = true;
  current_embedded_worker_id_ = embedded_worker_id;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ExtendableMessageEvent,
                        OnExtendableMessageEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEventStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // Record all messages directed to inner script context.
  inner_sink_.OnMessageReceived(message);
  return handled;
}

void EmbeddedWorkerTestHelper::OnSetupMojo(
    int thread_id,
    service_manager::InterfaceRegistry* interface_registry) {
  interface_registry->AddInterface(base::Bind(&MockFetchEventDispatcher::Create,
                                              weak_factory_.GetWeakPtr(),
                                              thread_id));
}

void EmbeddedWorkerTestHelper::OnActivateEvent(int embedded_worker_id,
                                               int request_id) {
  SimulateSend(new ServiceWorkerHostMsg_ActivateEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted, base::Time::Now()));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEvent(int embedded_worker_id,
                                                        int request_id) {
  SimulateSend(new ServiceWorkerHostMsg_ExtendableMessageEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted, base::Time::Now()));
}

void EmbeddedWorkerTestHelper::OnInstallEvent(int embedded_worker_id,
                                              int request_id) {
  // The installing worker may have been doomed and terminated.
  if (!registry()->GetWorker(embedded_worker_id))
    return;
  SimulateSend(new ServiceWorkerHostMsg_InstallEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted, true, base::Time::Now()));
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int embedded_worker_id,
    int fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    const FetchCallback& callback) {
  SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
      embedded_worker_id, fetch_event_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
      ServiceWorkerResponse(
          GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
          ServiceWorkerHeaderMap(), std::string(), 0, GURL(),
          blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
          false /* is_in_cache_storage */,
          std::string() /* cache_storage_cache_name */,
          ServiceWorkerHeaderList() /* cors_exposed_header_names */),
      base::Time::Now()));
  callback.Run(SERVICE_WORKER_OK, base::Time::Now());
}

void EmbeddedWorkerTestHelper::OnPushEvent(int embedded_worker_id,
                                           int request_id,
                                           const PushEventPayload& payload) {
  SimulateSend(new ServiceWorkerHostMsg_PushEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted, base::Time::Now()));
}

void EmbeddedWorkerTestHelper::SimulateWorkerReadyForInspection(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerReadyForInspection(worker->process_id(),
                                         embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptCached(
    int embedded_worker_id) {
  int64_t version_id =
      embedded_worker_id_service_worker_version_id_map_[embedded_worker_id];
  ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
  if (!version || version->script_cache_map()->size())
    return;
  std::vector<ServiceWorkerDatabase::ResourceRecord> records;
  // Add a dummy ResourceRecord for the main script to the script cache map of
  // the ServiceWorkerVersion. We use embedded_worker_id for resource_id to
  // avoid ID collision.
  records.push_back(ServiceWorkerDatabase::ResourceRecord(
      embedded_worker_id, version->script_url(), 100));
  version->script_cache_map()->SetResources(records);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptLoaded(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerScriptLoaded(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerThreadStarted(
    int thread_id,
    int embedded_worker_id) {
  thread_id_embedded_worker_id_map_[thread_id] = embedded_worker_id;
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerThreadStarted(worker->process_id(), thread_id,
                                    embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerScriptEvaluated(
    int embedded_worker_id,
    bool success) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerScriptEvaluated(worker->process_id(), embedded_worker_id,
                                      success);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerStarted(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  if (worker != NULL)
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateSend(IPC::Message* message) {
  registry()->OnMessageReceived(*message, mock_render_process_id_);
  delete message;
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    const EmbeddedWorkerStartParams& params) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker, AsWeakPtr(),
                 params.embedded_worker_id, params.service_worker_version_id,
                 params.scope, params.script_url, params.pause_after_download));
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownloadStub(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnResumeAfterDownload,
                            AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnStopWorker,
                            AsWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnMessageToWorkerStub(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(worker->thread_id(), thread_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&EmbeddedWorkerTestHelper::OnMessageToWorker),
          AsWeakPtr(), thread_id, embedded_worker_id, message));
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnActivateEvent, AsWeakPtr(),
                 current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEventStub(
    int request_id,
    const ServiceWorkerMsg_ExtendableMessageEvent_Params& params) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnExtendableMessageEvent,
                 AsWeakPtr(), current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnInstallEvent, AsWeakPtr(),
                 current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int thread_id,
    int fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    const FetchCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnFetchEvent, AsWeakPtr(),
                 thread_id_embedded_worker_id_map_[thread_id], fetch_event_id,
                 request, base::Passed(&preload_handle), callback));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    int request_id,
    const PushEventPayload& payload) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnPushEvent, AsWeakPtr(),
                            current_embedded_worker_id_, request_id, payload));
}

void EmbeddedWorkerTestHelper::OnSetupMojoStub(
    int thread_id,
    service_manager::mojom::InterfaceProviderRequest request,
    service_manager::mojom::InterfaceProviderPtr remote_interfaces) {
  auto local =
      base::MakeUnique<service_manager::InterfaceRegistry>(std::string());
  local->Bind(std::move(request), service_manager::Identity(),
              service_manager::InterfaceProviderSpec(),
              service_manager::Identity(),
              service_manager::InterfaceProviderSpec());

  std::unique_ptr<service_manager::InterfaceProvider> remote(
      new service_manager::InterfaceProvider);
  remote->Bind(std::move(remote_interfaces));

  OnSetupMojo(thread_id, local.get());
  InterfaceRegistryAndProvider pair(std::move(local), std::move(remote));
  thread_id_service_registry_map_[thread_id] = std::move(pair);
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

MessagePortMessageFilter*
EmbeddedWorkerTestHelper::NewMessagePortMessageFilter() {
  scoped_refptr<MessagePortMessageFilter> filter(
      new MockMessagePortMessageFilter);
  message_port_message_filters_.push_back(filter);
  return filter.get();
}

std::unique_ptr<service_manager::InterfaceRegistry>
EmbeddedWorkerTestHelper::CreateInterfaceRegistry(MockRenderProcessHost* rph) {
  auto registry =
      base::MakeUnique<service_manager::InterfaceRegistry>(std::string());
  registry->AddInterface(
      base::Bind(&MockEmbeddedWorkerSetup::Create, AsWeakPtr()));
  registry->AddInterface(
      base::Bind(&MockEmbeddedWorkerInstanceClient::Bind, AsWeakPtr()));

  service_manager::mojom::InterfaceProviderPtr interfaces;
  registry->Bind(mojo::GetProxy(&interfaces), service_manager::Identity(),
                 service_manager::InterfaceProviderSpec(),
                 service_manager::Identity(),
                 service_manager::InterfaceProviderSpec());

  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces(
      new service_manager::InterfaceProvider);
  remote_interfaces->Bind(std::move(interfaces));
  rph->SetRemoteInterfaces(std::move(remote_interfaces));
  return registry;
}

}  // namespace content
