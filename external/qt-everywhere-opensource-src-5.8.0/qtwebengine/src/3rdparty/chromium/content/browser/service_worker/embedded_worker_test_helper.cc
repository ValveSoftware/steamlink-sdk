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
#include "content/browser/message_port_message_filter.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/public/common/push_event_payload.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "services/shell/public/cpp/interface_registry.h"
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
  static void Create(
      const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
      mojo::InterfaceRequest<mojom::EmbeddedWorkerSetup> request) {
    new MockEmbeddedWorkerSetup(helper, std::move(request));
  }

  void ExchangeInterfaceProviders(
      int32_t thread_id,
      shell::mojom::InterfaceProviderRequest request,
      shell::mojom::InterfaceProviderPtr remote_interfaces) override {
    if (!helper_)
      return;
    helper_->OnSetupMojoStub(thread_id, std::move(request),
                             std::move(remote_interfaces));
  }

 private:
  MockEmbeddedWorkerSetup(
      const base::WeakPtr<EmbeddedWorkerTestHelper>& helper,
      mojo::InterfaceRequest<mojom::EmbeddedWorkerSetup> request)
      : helper_(helper), binding_(this, std::move(request)) {}

  base::WeakPtr<EmbeddedWorkerTestHelper> helper_;
  mojo::StrongBinding<mojom::EmbeddedWorkerSetup> binding_;
};

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    const base::FilePath& user_data_directory)
    : browser_context_(new TestBrowserContext),
      render_process_host_(new MockRenderProcessHost(browser_context_.get())),
      wrapper_(new ServiceWorkerContextWrapper(browser_context_.get())),
      next_thread_id_(0),
      mock_render_process_id_(render_process_host_->GetID()),
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
  render_process_interface_registry_.reset(
      new shell::InterfaceRegistry(nullptr));
  render_process_interface_registry_->AddInterface(
      base::Bind(&MockEmbeddedWorkerSetup::Create, weak_factory_.GetWeakPtr()));
  shell::mojom::InterfaceProviderPtr interfaces;
  render_process_interface_registry_->Bind(mojo::GetProxy(&interfaces));

  std::unique_ptr<shell::InterfaceProvider> host_remote_interfaces(
      new shell::InterfaceProvider);
  host_remote_interfaces->Bind(std::move(interfaces));
  std::unique_ptr<shell::InterfaceRegistry> host_registry(
      new shell::InterfaceRegistry(nullptr));
  render_process_host_->SetInterfaceRegistry(std::move(host_registry));
  render_process_host_->SetRemoteInterfaces(std::move(host_remote_interfaces));
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
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_PushEvent, OnPushEventStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // Record all messages directed to inner script context.
  inner_sink_.OnMessageReceived(message);
  return handled;
}

void EmbeddedWorkerTestHelper::OnSetupMojo(
    shell::InterfaceRegistry* interface_registry) {}

void EmbeddedWorkerTestHelper::OnActivateEvent(int embedded_worker_id,
                                               int request_id) {
  SimulateSend(new ServiceWorkerHostMsg_ActivateEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEvent(int embedded_worker_id,
                                                        int request_id) {
  SimulateSend(new ServiceWorkerHostMsg_ExtendableMessageEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnInstallEvent(int embedded_worker_id,
                                              int request_id) {
  // The installing worker may have been doomed and terminated.
  if (!registry()->GetWorker(embedded_worker_id))
    return;
  SimulateSend(new ServiceWorkerHostMsg_InstallEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted, true));
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int embedded_worker_id,
    int response_id,
    int event_finish_id,
    const ServiceWorkerFetchRequest& request) {
  SimulateSend(new ServiceWorkerHostMsg_FetchEventResponse(
      embedded_worker_id, response_id,
      SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
      ServiceWorkerResponse(
          GURL(), 200, "OK", blink::WebServiceWorkerResponseTypeDefault,
          ServiceWorkerHeaderMap(), std::string(), 0, GURL(),
          blink::WebServiceWorkerResponseErrorUnknown, base::Time(),
          false /* is_in_cache_storage */,
          std::string() /* cache_storage_cache_name */,
          ServiceWorkerHeaderList() /* cors_exposed_header_names */)));
  SimulateSend(new ServiceWorkerHostMsg_FetchEventFinished(
      embedded_worker_id, event_finish_id,
      blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnPushEvent(int embedded_worker_id,
                                           int request_id,
                                           const PushEventPayload& payload) {
  SimulateSend(new ServiceWorkerHostMsg_PushEventFinished(
      embedded_worker_id, request_id,
      blink::WebServiceWorkerEventResultCompleted));
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
    const EmbeddedWorkerMsg_StartWorker_Params& params) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker,
                 weak_factory_.GetWeakPtr(), params.embedded_worker_id,
                 params.service_worker_version_id, params.scope,
                 params.script_url, params.pause_after_download));
}

void EmbeddedWorkerTestHelper::OnResumeAfterDownloadStub(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnResumeAfterDownload,
                            weak_factory_.GetWeakPtr(), embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnStopWorker,
                            weak_factory_.GetWeakPtr(), embedded_worker_id));
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
          weak_factory_.GetWeakPtr(), thread_id, embedded_worker_id, message));
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnActivateEvent,
                            weak_factory_.GetWeakPtr(),
                            current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnExtendableMessageEventStub(
    int request_id,
    const ServiceWorkerMsg_ExtendableMessageEvent_Params& params) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnExtendableMessageEvent,
                            weak_factory_.GetWeakPtr(),
                            current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(int request_id) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnInstallEvent,
                            weak_factory_.GetWeakPtr(),
                            current_embedded_worker_id_, request_id));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int response_id,
    int event_finish_id,
    const ServiceWorkerFetchRequest& request) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnFetchEvent,
                 weak_factory_.GetWeakPtr(), current_embedded_worker_id_,
                 response_id, event_finish_id, request));
}

void EmbeddedWorkerTestHelper::OnPushEventStub(
    int request_id,
    const PushEventPayload& payload) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&EmbeddedWorkerTestHelper::OnPushEvent,
                            weak_factory_.GetWeakPtr(),
                            current_embedded_worker_id_, request_id, payload));
}

void EmbeddedWorkerTestHelper::OnSetupMojoStub(
    int thread_id,
    shell::mojom::InterfaceProviderRequest request,
    shell::mojom::InterfaceProviderPtr remote_interfaces) {
  std::unique_ptr<shell::InterfaceRegistry> local(
      new shell::InterfaceRegistry(nullptr));
  local->Bind(std::move(request));

  std::unique_ptr<shell::InterfaceProvider> remote(
      new shell::InterfaceProvider);
  remote->Bind(std::move(remote_interfaces));

  OnSetupMojo(local.get());
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

}  // namespace content
