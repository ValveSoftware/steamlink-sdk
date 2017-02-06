// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/worker_thread_dispatcher.h"

#include "base/threading/thread_local.h"
#include "base/values.h"
#include "content/public/child/worker_thread.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/service_worker_data.h"

namespace extensions {

namespace {

base::LazyInstance<WorkerThreadDispatcher> g_instance =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::ThreadLocalPointer<extensions::ServiceWorkerData>>
    g_data_tls = LAZY_INSTANCE_INITIALIZER;

void OnResponseOnWorkerThread(int request_id,
                              bool succeeded,
                              const std::unique_ptr<base::ListValue>& response,
                              const std::string& error) {
  WorkerThreadDispatcher::GetRequestSender()->HandleResponse(
      request_id, succeeded, *response, error);
}

}  // namespace

WorkerThreadDispatcher::WorkerThreadDispatcher() {}
WorkerThreadDispatcher::~WorkerThreadDispatcher() {}

WorkerThreadDispatcher* WorkerThreadDispatcher::Get() {
  return g_instance.Pointer();
}

void WorkerThreadDispatcher::Init(content::RenderThread* render_thread) {
  DCHECK(render_thread);
  DCHECK_EQ(content::RenderThread::Get(), render_thread);
  DCHECK(!message_filter_);
  message_filter_ = render_thread->GetSyncMessageFilter();
  render_thread->AddObserver(this);
}

V8SchemaRegistry* WorkerThreadDispatcher::GetV8SchemaRegistry() {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK(data);
  return data->v8_schema_registry();
}

// static
RequestSender* WorkerThreadDispatcher::GetRequestSender() {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  DCHECK(data);
  return data->request_sender();
}

bool WorkerThreadDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerThreadDispatcher, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ResponseWorker, OnResponseWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool WorkerThreadDispatcher::Send(IPC::Message* message) {
  return message_filter_->Send(message);
}

void WorkerThreadDispatcher::OnResponseWorker(int worker_thread_id,
                                              int request_id,
                                              bool succeeded,
                                              const base::ListValue& response,
                                              const std::string& error) {
  content::WorkerThread::PostTask(
      worker_thread_id,
      base::Bind(&OnResponseOnWorkerThread, request_id, succeeded,
                 // TODO(lazyboy): Can we avoid CreateDeepCopy()?
                 base::Passed(response.CreateDeepCopy()), error));
}

void WorkerThreadDispatcher::AddWorkerData(int embedded_worker_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (!data) {
    ServiceWorkerData* new_data =
        new ServiceWorkerData(this, embedded_worker_id);
    g_data_tls.Pointer()->Set(new_data);
  }
}

void WorkerThreadDispatcher::RemoveWorkerData(int embedded_worker_id) {
  ServiceWorkerData* data = g_data_tls.Pointer()->Get();
  if (data) {
    DCHECK_EQ(embedded_worker_id, data->embedded_worker_id());
    delete data;
    g_data_tls.Pointer()->Set(nullptr);
  }
}

}  // namespace extensions
