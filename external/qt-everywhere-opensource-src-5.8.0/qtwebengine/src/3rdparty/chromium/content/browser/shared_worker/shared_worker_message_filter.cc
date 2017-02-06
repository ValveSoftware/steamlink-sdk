// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_message_filter.h"

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationErrors.h"

namespace content {
namespace {
const uint32_t kFilteredMessageClasses[] = {
    ViewMsgStart, WorkerMsgStart,
};

// TODO(estark): For now, only URLMismatch errors actually stop the
// worker from being created. Other errors are recorded in UMA in
// Blink but do not stop the worker from being created
// yet. https://crbug.com/573206
bool CreateWorkerErrorIsFatal(blink::WebWorkerCreationError error) {
  return (error == blink::WebWorkerCreationErrorURLMismatch);
}

}  // namespace

SharedWorkerMessageFilter::SharedWorkerMessageFilter(
    int render_process_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    MessagePortMessageFilter* message_port_message_filter)
    : BrowserMessageFilter(kFilteredMessageClasses,
                           arraysize(kFilteredMessageClasses)),
      render_process_id_(render_process_id),
      resource_context_(resource_context),
      partition_(partition),
      message_port_message_filter_(message_port_message_filter) {
}

SharedWorkerMessageFilter::~SharedWorkerMessageFilter() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void SharedWorkerMessageFilter::OnChannelClosing() {
  SharedWorkerServiceImpl::GetInstance()->OnSharedWorkerMessageFilterClosing(
      this);
}

bool SharedWorkerMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SharedWorkerMessageFilter, message)
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker, OnForwardToWorker)
    // Only sent from renderer.
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentDetached, OnDocumentDetached)
    // Only sent from SharedWorker in renderer.
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerContextClosed,
                        OnWorkerContextClosed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerContextDestroyed,
                        OnWorkerContextDestroyed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerReadyForInspection,
                        OnWorkerReadyForInspection)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerScriptLoaded,
                        OnWorkerScriptLoaded)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerConnected,
                        OnWorkerConnected)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        WorkerProcessHostMsg_RequestFileSystemAccessSync,
        OnRequestFileSystemAccess)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowIndexedDB, OnAllowIndexedDB)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

int SharedWorkerMessageFilter::GetNextRoutingID() {
  return message_port_message_filter_->GetNextRoutingID();
}

void SharedWorkerMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    ViewHostMsg_CreateWorker_Reply* reply) {
  reply->route_id = GetNextRoutingID();
  SharedWorkerServiceImpl::GetInstance()->CreateWorker(
      params, reply->route_id, this, resource_context_,
      WorkerStoragePartitionId(partition_), &reply->error);
  if (CreateWorkerErrorIsFatal(reply->error))
    reply->route_id = MSG_ROUTING_NONE;
}

void SharedWorkerMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  SharedWorkerServiceImpl::GetInstance()->ForwardToWorker(message, this);
}

void SharedWorkerMessageFilter::OnDocumentDetached(
    unsigned long long document_id) {
  SharedWorkerServiceImpl::GetInstance()->DocumentDetached(document_id, this);
}

void SharedWorkerMessageFilter::OnWorkerContextClosed(int worker_route_id) {
  SharedWorkerServiceImpl::GetInstance()->WorkerContextClosed(worker_route_id,
                                                              this);
}

void SharedWorkerMessageFilter::OnWorkerContextDestroyed(int worker_route_id) {
  SharedWorkerServiceImpl::GetInstance()->WorkerContextDestroyed(
      worker_route_id,
      this);
}

void SharedWorkerMessageFilter::OnWorkerReadyForInspection(
    int worker_route_id) {
  SharedWorkerServiceImpl::GetInstance()->WorkerReadyForInspection(
      worker_route_id, this);
}

void SharedWorkerMessageFilter::OnWorkerScriptLoaded(int worker_route_id) {
  SharedWorkerServiceImpl::GetInstance()->WorkerScriptLoaded(worker_route_id,
                                                             this);
}

void SharedWorkerMessageFilter::OnWorkerScriptLoadFailed(int worker_route_id) {
  SharedWorkerServiceImpl::GetInstance()->WorkerScriptLoadFailed(
      worker_route_id,
      this);
}

void SharedWorkerMessageFilter::OnWorkerConnected(int message_port_id,
                                                  int worker_route_id) {
  SharedWorkerServiceImpl::GetInstance()->WorkerConnected(
      message_port_id,
      worker_route_id,
      this);
}

void SharedWorkerMessageFilter::OnRequestFileSystemAccess(
    int worker_route_id,
    const GURL& url,
    IPC::Message* reply_msg) {
  SharedWorkerServiceImpl::GetInstance()->AllowFileSystem(
      worker_route_id, url, reply_msg, this);
}

void SharedWorkerMessageFilter::OnAllowIndexedDB(int worker_route_id,
                                                 const GURL& url,
                                                 const base::string16& name,
                                                 bool* result) {
  SharedWorkerServiceImpl::GetInstance()->AllowIndexedDB(worker_route_id,
                                                         url,
                                                         name,
                                                         result,
                                                         this);
}

}  // namespace content
