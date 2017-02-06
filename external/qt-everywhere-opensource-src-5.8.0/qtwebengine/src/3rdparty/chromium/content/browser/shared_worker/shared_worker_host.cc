// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include "base/metrics/histogram.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/browser/shared_worker/worker_document_set.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"

namespace content {
namespace {

void NotifyWorkerReadyForInspection(int worker_process_id,
                                    int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(NotifyWorkerReadyForInspection,
                                       worker_process_id,
                                       worker_route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyed(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyed, worker_process_id, worker_route_id));
    return;
  }
  SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

}  // namespace

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance,
                                   SharedWorkerMessageFilter* filter,
                                   int worker_route_id)
    : instance_(instance),
      worker_document_set_(new WorkerDocumentSet()),
      container_render_filter_(filter),
      worker_process_id_(filter->render_process_id()),
      worker_route_id_(worker_route_id),
      termination_message_sent_(false),
      closed_(false),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  UMA_HISTOGRAM_LONG_TIMES("SharedWorker.TimeToDeleted",
                           base::TimeTicks::Now() - creation_time_);
  if (!closed_ && !termination_message_sent_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
  SharedWorkerServiceImpl::GetInstance()->NotifyWorkerDestroyed(
      worker_process_id_, worker_route_id_);
}

bool SharedWorkerHost::Send(IPC::Message* message) {
  if (!container_render_filter_) {
    delete message;
    return false;
  }
  return container_render_filter_->Send(message);
}

void SharedWorkerHost::Start(bool pause_on_start) {
  WorkerProcessMsg_CreateWorker_Params params;
  params.url = instance_->url();
  params.name = instance_->name();
  params.content_security_policy = instance_->content_security_policy();
  params.security_policy_type = instance_->security_policy_type();
  params.creation_address_space = instance_->creation_address_space();
  params.pause_on_start = pause_on_start;
  params.route_id = worker_route_id_;
  Send(new WorkerProcessMsg_CreateWorker(params));

  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    i->filter()->Send(new ViewMsg_WorkerCreated(i->route_id()));
  }
}

bool SharedWorkerHost::FilterMessage(const IPC::Message& message,
                                     SharedWorkerMessageFilter* filter) {
  if (!IsAvailable() || !HasFilter(filter, message.routing_id()))
    return false;

  RelayMessage(message, filter);
  return true;
}

void SharedWorkerHost::FilterShutdown(SharedWorkerMessageFilter* filter) {
  if (!instance_)
    return;
  RemoveFilters(filter);
  worker_document_set_->RemoveAll(filter);
  if (worker_document_set_->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    TerminateWorker();
  }
}

void SharedWorkerHost::DocumentDetached(SharedWorkerMessageFilter* filter,
                                        unsigned long long document_id) {
  if (!instance_)
    return;
  // Walk all instances and remove the document from their document set.
  worker_document_set_->Remove(filter, document_id);
  if (worker_document_set_->IsEmpty()) {
    // This worker has no more associated documents - shut it down.
    TerminateWorker();
  }
}

void SharedWorkerHost::WorkerContextClosed() {
  if (!instance_)
    return;
  // Set the closed flag - this will stop any further messages from
  // being sent to the worker (messages can still be sent from the worker,
  // for exception reporting, etc).
  closed_ = true;
  if (!termination_message_sent_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::WorkerContextDestroyed() {
  if (!instance_)
    return;
  instance_.reset();
  worker_document_set_ = NULL;
}

void SharedWorkerHost::WorkerReadyForInspection() {
  NotifyWorkerReadyForInspection(worker_process_id_, worker_route_id_);
}

void SharedWorkerHost::WorkerScriptLoaded() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoaded",
                      base::TimeTicks::Now() - creation_time_);
}

void SharedWorkerHost::WorkerScriptLoadFailed() {
  UMA_HISTOGRAM_TIMES("SharedWorker.TimeToScriptLoadFailed",
                      base::TimeTicks::Now() - creation_time_);
  if (!instance_)
    return;
  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    i->filter()->Send(new ViewMsg_WorkerScriptLoadFailed(i->route_id()));
  }
}

void SharedWorkerHost::WorkerConnected(int message_port_id) {
  if (!instance_)
    return;
  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    if (i->message_port_id() != message_port_id)
      continue;
    i->filter()->Send(new ViewMsg_WorkerConnected(i->route_id()));
    return;
  }
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    std::unique_ptr<IPC::Message> reply_msg) {
  if (!instance_)
    return;
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url,
      instance_->resource_context(),
      GetRenderFrameIDsForWorker(),
      base::Bind(&SharedWorkerHost::AllowFileSystemResponse,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&reply_msg)));
}

void SharedWorkerHost::AllowFileSystemResponse(
    std::unique_ptr<IPC::Message> reply_msg,
    bool allowed) {
  WorkerProcessHostMsg_RequestFileSystemAccessSync::WriteReplyParams(
      reply_msg.get(),
      allowed);
  Send(reply_msg.release());
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      const base::string16& name,
                                      bool* result) {
  if (!instance_)
    return;
  *result = GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, name, instance_->resource_context(), GetRenderFrameIDsForWorker());
}

void SharedWorkerHost::RelayMessage(
    const IPC::Message& message,
    SharedWorkerMessageFilter* incoming_filter) {
  if (!instance_)
    return;
  if (message.type() == WorkerMsg_Connect::ID) {
    // Crack the SharedWorker Connect message to setup routing for the port.
    WorkerMsg_Connect::Param param;
    if (!WorkerMsg_Connect::Read(&message, &param))
      return;
    int sent_message_port_id = std::get<0>(param);
    int new_routing_id = std::get<1>(param);

    DCHECK(container_render_filter_);
    new_routing_id = container_render_filter_->GetNextRoutingID();
    MessagePortService::GetInstance()->UpdateMessagePort(
        sent_message_port_id,
        container_render_filter_->message_port_message_filter(),
        new_routing_id);
    SetMessagePortID(
        incoming_filter, message.routing_id(), sent_message_port_id);
    // Resend the message with the new routing id.
    Send(new WorkerMsg_Connect(
        worker_route_id_, sent_message_port_id, new_routing_id));

    // Send any queued messages for the sent port.
    MessagePortService::GetInstance()->SendQueuedMessagesIfPossible(
        sent_message_port_id);
  } else {
    IPC::Message* new_message = new IPC::Message(message);
    new_message->set_routing_id(worker_route_id_);
    Send(new_message);
    return;
  }
}

void SharedWorkerHost::TerminateWorker() {
  termination_message_sent_ = true;
  if (!closed_)
    NotifyWorkerDestroyed(worker_process_id_, worker_route_id_);
  Send(new WorkerMsg_TerminateWorkerContext(worker_route_id_));
}

std::vector<std::pair<int, int> >
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<std::pair<int, int> > result;
  if (!instance_)
    return result;
  const WorkerDocumentSet::DocumentInfoSet& documents =
      worker_document_set_->documents();
  for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
           documents.begin();
       doc != documents.end();
       ++doc) {
    result.push_back(
        std::make_pair(doc->render_process_id(), doc->render_frame_id()));
  }
  return result;
}

bool SharedWorkerHost::IsAvailable() const {
  return instance_ && !termination_message_sent_ && !closed_;
}

void SharedWorkerHost::AddFilter(SharedWorkerMessageFilter* filter,
                                 int route_id) {
  CHECK(filter);
  if (!HasFilter(filter, route_id)) {
    FilterInfo info(filter, route_id);
    filters_.push_back(info);
  }
}

void SharedWorkerHost::RemoveFilters(SharedWorkerMessageFilter* filter) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end();) {
    if (i->filter() == filter)
      i = filters_.erase(i);
    else
      ++i;
  }
}

bool SharedWorkerHost::HasFilter(SharedWorkerMessageFilter* filter,
                                 int route_id) const {
  for (FilterList::const_iterator i = filters_.begin(); i != filters_.end();
       ++i) {
    if (i->filter() == filter && i->route_id() == route_id)
      return true;
  }
  return false;
}

void SharedWorkerHost::SetMessagePortID(SharedWorkerMessageFilter* filter,
                                        int route_id,
                                        int message_port_id) {
  for (FilterList::iterator i = filters_.begin(); i != filters_.end(); ++i) {
    if (i->filter() == filter && i->route_id() == route_id) {
      i->set_message_port_id(message_port_id);
      return;
    }
  }
}

}  // namespace content
