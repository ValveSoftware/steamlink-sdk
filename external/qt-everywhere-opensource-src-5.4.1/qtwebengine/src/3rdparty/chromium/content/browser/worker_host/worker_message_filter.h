// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "content/browser/worker_host/worker_storage_partition.h"
#include "content/public/browser/browser_message_filter.h"

class ResourceDispatcherHost;
struct ViewHostMsg_CreateWorker_Params;

namespace content {
class MessagePortMessageFilter;
class ResourceContext;

class WorkerMessageFilter : public BrowserMessageFilter {
 public:
  WorkerMessageFilter(int render_process_id,
                      ResourceContext* resource_context,
                      const WorkerStoragePartition& partition,
                      MessagePortMessageFilter* message_port_filter);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  int GetNextRoutingID();
  int render_process_id() const { return render_process_id_; }

  MessagePortMessageFilter* message_port_message_filter() const {
    return message_port_message_filter_;
  }

 private:
  virtual ~WorkerMessageFilter();

  // Message handlers.
  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      int* route_id);
  void OnForwardToWorker(const IPC::Message& message);
  void OnDocumentDetached(unsigned long long document_id);

  int render_process_id_;
  ResourceContext* const resource_context_;
  WorkerStoragePartition partition_;

  MessagePortMessageFilter* message_port_message_filter_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(WorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_MESSAGE_FILTER_H_
