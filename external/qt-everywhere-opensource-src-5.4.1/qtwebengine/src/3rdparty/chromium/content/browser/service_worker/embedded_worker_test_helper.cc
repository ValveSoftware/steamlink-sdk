// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include "base/bind.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(int mock_render_process_id)
    : wrapper_(new ServiceWorkerContextWrapper(NULL)),
      next_thread_id_(0),
      weak_factory_(this) {
  wrapper_->InitInternal(base::FilePath(),
                         base::MessageLoopProxy::current(),
                         base::MessageLoopProxy::current(),
                         NULL);
  wrapper_->process_manager()->SetProcessIdForTest(mock_render_process_id);
  registry()->AddChildProcessSender(mock_render_process_id, this);
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
  if (wrapper_)
    wrapper_->Shutdown();
}

void EmbeddedWorkerTestHelper::SimulateAddProcessToWorker(
    int embedded_worker_id,
    int process_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  registry()->AddChildProcessSender(process_id, this);
  worker->AddProcessReference(process_id);
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

void EmbeddedWorkerTestHelper::OnStartWorker(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& scope,
    const GURL& script_url) {
  // By default just notify the sender that the worker is started.
  SimulateWorkerStarted(next_thread_id_++, embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnStopWorker(int embedded_worker_id) {
  // By default just notify the sender that the worker is stopped.
  SimulateWorkerStopped(embedded_worker_id);
}

bool EmbeddedWorkerTestHelper::OnMessageToWorker(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  bool handled = true;
  current_embedded_worker_id_ = embedded_worker_id;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ActivateEvent, OnActivateEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_InstallEvent, OnInstallEventStub)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_FetchEvent, OnFetchEventStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // Record all messages directed to inner script context.
  inner_sink_.OnMessageReceived(message);
  return handled;
}

void EmbeddedWorkerTestHelper::OnActivateEvent(int embedded_worker_id,
                                               int request_id) {
  SimulateSend(
      new ServiceWorkerHostMsg_ActivateEventFinished(
          embedded_worker_id, request_id,
          blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnInstallEvent(int embedded_worker_id,
                                              int request_id,
                                              int active_version_id) {
  SimulateSend(
      new ServiceWorkerHostMsg_InstallEventFinished(
          embedded_worker_id, request_id,
          blink::WebServiceWorkerEventResultCompleted));
}

void EmbeddedWorkerTestHelper::OnFetchEvent(
    int embedded_worker_id,
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  SimulateSend(
      new ServiceWorkerHostMsg_FetchEventFinished(
          embedded_worker_id,
          request_id,
          SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE,
          ServiceWorkerResponse(200, "OK",
                                std::map<std::string, std::string>(),
                                std::string())));
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(
    int thread_id, int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerStarted(
      worker->process_id(),
      thread_id,
      embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  if (worker != NULL)
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateSend(
    IPC::Message* message) {
  registry()->OnMessageReceived(*message);
  delete message;
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    const EmbeddedWorkerMsg_StartWorker_Params& params) {
  EmbeddedWorkerInstance* worker =
      registry()->GetWorker(params.embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker,
                 weak_factory_.GetWeakPtr(),
                 params.embedded_worker_id,
                 params.service_worker_version_id,
                 params.scope,
                 params.script_url));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStopWorker,
                 weak_factory_.GetWeakPtr(),
                 embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnMessageToWorkerStub(
    int thread_id,
    int embedded_worker_id,
    const IPC::Message& message) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(worker->thread_id(), thread_id);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&EmbeddedWorkerTestHelper::OnMessageToWorker),
          weak_factory_.GetWeakPtr(),
          thread_id,
          embedded_worker_id,
          message));
}

void EmbeddedWorkerTestHelper::OnActivateEventStub(int request_id) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnActivateEvent,
                 weak_factory_.GetWeakPtr(),
                 current_embedded_worker_id_,
                 request_id));
}

void EmbeddedWorkerTestHelper::OnInstallEventStub(int request_id,
                                                  int active_version_id) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnInstallEvent,
                 weak_factory_.GetWeakPtr(),
                 current_embedded_worker_id_,
                 request_id,
                 active_version_id));
}

void EmbeddedWorkerTestHelper::OnFetchEventStub(
    int request_id,
    const ServiceWorkerFetchRequest& request) {
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnFetchEvent,
                 weak_factory_.GetWeakPtr(),
                 current_embedded_worker_id_,
                 request_id,
                 request));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context());
  return context()->embedded_worker_registry();
}

}  // namespace content
