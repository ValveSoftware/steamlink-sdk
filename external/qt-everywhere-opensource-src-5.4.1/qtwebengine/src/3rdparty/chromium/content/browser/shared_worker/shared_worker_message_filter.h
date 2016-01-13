// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_

#include "content/browser/worker_host/worker_storage_partition.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;
struct ViewHostMsg_CreateWorker_Params;

namespace content {
class MessagePortMessageFilter;
class ResourceContext;

// If "enable-embedded-shared-worker" is set this class will be used instead of
// WorkerMessageFilter.
class CONTENT_EXPORT SharedWorkerMessageFilter : public BrowserMessageFilter {
 public:
  SharedWorkerMessageFilter(int render_process_id,
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

 protected:
  // This is protected, so we can define sub classes for testing.
  virtual ~SharedWorkerMessageFilter();

 private:
  // Message handlers.
  void OnCreateWorker(const ViewHostMsg_CreateWorker_Params& params,
                      int* route_id);
  void OnForwardToWorker(const IPC::Message& message);
  void OnDocumentDetached(unsigned long long document_id);
  void OnWorkerContextClosed(int worker_route_id);
  void OnWorkerContextDestroyed(int worker_route_id);
  void OnWorkerScriptLoaded(int worker_route_id);
  void OnWorkerScriptLoadFailed(int worker_route_id);
  void OnWorkerConnected(int message_port_id, int worker_route_id);
  void OnAllowDatabase(int worker_route_id,
                       const GURL& url,
                       const base::string16& name,
                       const base::string16& display_name,
                       unsigned long estimated_size,
                       bool* result);
  void OnRequestFileSystemAccessSync(int worker_route_id,
                                     const GURL& url,
                                     bool* result);
  void OnAllowIndexedDB(int worker_route_id,
                        const GURL& url,
                        const base::string16& name,
                        bool* result);

  const int render_process_id_;
  ResourceContext* const resource_context_;
  const WorkerStoragePartition partition_;
  MessagePortMessageFilter* const message_port_message_filter_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedWorkerMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_MESSAGE_FILTER_H_
