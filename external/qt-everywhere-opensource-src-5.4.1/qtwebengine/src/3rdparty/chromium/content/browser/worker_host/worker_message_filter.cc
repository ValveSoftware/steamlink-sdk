// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_message_filter.h"

#include "content/browser/message_port_message_filter.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/resource_context.h"

namespace content {

WorkerMessageFilter::WorkerMessageFilter(
    int render_process_id,
    ResourceContext* resource_context,
    const WorkerStoragePartition& partition,
    MessagePortMessageFilter* message_port_message_filter)
    : BrowserMessageFilter(ViewMsgStart),
      render_process_id_(render_process_id),
      resource_context_(resource_context),
      partition_(partition),
      message_port_message_filter_(message_port_message_filter) {
  // Note: This constructor is called on both IO or UI thread.
  DCHECK(resource_context);
}

WorkerMessageFilter::~WorkerMessageFilter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void WorkerMessageFilter::OnChannelClosing() {
  WorkerServiceImpl::GetInstance()->OnWorkerMessageFilterClosing(this);
}

bool WorkerMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerMessageFilter, message)
    // Worker messages.
    // Only sent from renderer for now, until we have nested workers.
    IPC_MESSAGE_HANDLER(ViewHostMsg_CreateWorker, OnCreateWorker)
    IPC_MESSAGE_HANDLER(ViewHostMsg_ForwardToWorker, OnForwardToWorker)
    // Only sent from renderer.
    IPC_MESSAGE_HANDLER(ViewHostMsg_DocumentDetached, OnDocumentDetached)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

int WorkerMessageFilter::GetNextRoutingID() {
  return message_port_message_filter_->GetNextRoutingID();
}

void WorkerMessageFilter::OnCreateWorker(
    const ViewHostMsg_CreateWorker_Params& params,
    int* route_id) {
  bool url_error = false;
  *route_id = GetNextRoutingID();
  WorkerServiceImpl::GetInstance()->CreateWorker(
      params, *route_id, this, resource_context_, partition_, &url_error);
  if (url_error)
    *route_id = MSG_ROUTING_NONE;
}

void WorkerMessageFilter::OnForwardToWorker(const IPC::Message& message) {
  WorkerServiceImpl::GetInstance()->ForwardToWorker(message, this);
}

void WorkerMessageFilter::OnDocumentDetached(unsigned long long document_id) {
  WorkerServiceImpl::GetInstance()->DocumentDetached(document_id, this);
}

}  // namespace content
