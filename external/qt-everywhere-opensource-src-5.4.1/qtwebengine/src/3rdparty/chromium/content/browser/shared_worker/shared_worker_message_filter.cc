// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_message_filter.h"

#include "content/browser/devtools/worker_devtools_manager.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"

namespace content {
namespace {
const uint32 kFilteredMessageClasses[] = {
  ViewMsgStart,
  WorkerMsgStart,
};
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerScriptLoaded,
                        OnWorkerScriptLoaded)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(WorkerHostMsg_WorkerConnected,
                        OnWorkerConnected)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_RequestFileSystemAccessSync,
                        OnRequestFileSystemAccessSync)
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
    int* route_id) {
  bool url_error = false;
  *route_id = GetNextRoutingID();
  SharedWorkerServiceImpl::GetInstance()->CreateWorker(
      params,
      *route_id,
      this,
      resource_context_,
      WorkerStoragePartitionId(partition_),
      &url_error);
  if (url_error)
    *route_id = MSG_ROUTING_NONE;
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

void SharedWorkerMessageFilter::OnAllowDatabase(
    int worker_route_id,
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    unsigned long estimated_size,
    bool* result) {
  SharedWorkerServiceImpl::GetInstance()->AllowDatabase(worker_route_id,
                                                        url,
                                                        name,
                                                        display_name,
                                                        estimated_size,
                                                        result,
                                                        this);
}

void SharedWorkerMessageFilter::OnRequestFileSystemAccessSync(
    int worker_route_id,
    const GURL& url,
    bool* result) {
  SharedWorkerServiceImpl::GetInstance()->AllowFileSystem(worker_route_id,
                                                          url,
                                                          result,
                                                          this);
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
