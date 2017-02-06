// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/browser/shared_worker/worker_document_set.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {
class SharedWorkerMessageFilter;
class SharedWorkerInstance;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel.
class SharedWorkerHost {
 public:
  SharedWorkerHost(SharedWorkerInstance* instance,
                   SharedWorkerMessageFilter* filter,
                   int worker_route_id);
  ~SharedWorkerHost();

  // Sends |message| to the SharedWorker.
  bool Send(IPC::Message* message);

  // Starts the SharedWorker in the renderer process which is associated with
  // |filter_|.
  void Start(bool pause_on_start);

  // Returns true iff the given message from a renderer process was forwarded to
  // the worker.
  bool FilterMessage(const IPC::Message& message,
                     SharedWorkerMessageFilter* filter);

  // Handles the shutdown of the filter. If the worker has no other client,
  // sends TerminateWorkerContext message to shut it down.
  void FilterShutdown(SharedWorkerMessageFilter* filter);

  // Shuts down any shared workers that are no longer referenced by active
  // documents.
  void DocumentDetached(SharedWorkerMessageFilter* filter,
                        unsigned long long document_id);

  void WorkerContextClosed();
  void WorkerReadyForInspection();
  void WorkerScriptLoaded();
  void WorkerScriptLoadFailed();
  void WorkerConnected(int message_port_id);
  void WorkerContextDestroyed();
  void AllowFileSystem(const GURL& url,
                       std::unique_ptr<IPC::Message> reply_msg);
  void AllowIndexedDB(const GURL& url,
                      const base::string16& name,
                      bool* result);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  void AddFilter(SharedWorkerMessageFilter* filter, int route_id);

  SharedWorkerInstance* instance() { return instance_.get(); }
  WorkerDocumentSet* worker_document_set() const {
    return worker_document_set_.get();
  }
  SharedWorkerMessageFilter* container_render_filter() const {
    return container_render_filter_;
  }
  int process_id() const { return worker_process_id_; }
  int worker_route_id() const { return worker_route_id_; }
  bool IsAvailable() const;

 private:
  // Unique identifier for a worker client.
  class FilterInfo {
   public:
    FilterInfo(SharedWorkerMessageFilter* filter, int route_id)
        : filter_(filter), route_id_(route_id), message_port_id_(0) {}
    SharedWorkerMessageFilter* filter() const { return filter_; }
    int route_id() const { return route_id_; }
    int message_port_id() const { return message_port_id_; }
    void set_message_port_id(int id) { message_port_id_ = id; }

   private:
    SharedWorkerMessageFilter* filter_;
    int route_id_;
    int message_port_id_;
  };

  typedef std::list<FilterInfo> FilterList;

  // Relays |message| to the SharedWorker. Takes care of parsing the message if
  // it contains a message port and sending it a valid route id.
  void RelayMessage(const IPC::Message& message,
                    SharedWorkerMessageFilter* incoming_filter);

  // Return a vector of all the render process/render frame IDs.
  std::vector<std::pair<int, int> > GetRenderFrameIDsForWorker();

  void RemoveFilters(SharedWorkerMessageFilter* filter);
  bool HasFilter(SharedWorkerMessageFilter* filter, int route_id) const;
  void SetMessagePortID(SharedWorkerMessageFilter* filter,
                        int route_id,
                        int message_port_id);
  void AllowFileSystemResponse(std::unique_ptr<IPC::Message> reply_msg,
                               bool allowed);
  std::unique_ptr<SharedWorkerInstance> instance_;
  scoped_refptr<WorkerDocumentSet> worker_document_set_;
  FilterList filters_;
  SharedWorkerMessageFilter* container_render_filter_;
  int worker_process_id_;
  int worker_route_id_;
  bool termination_message_sent_;
  bool closed_;
  const base::TimeTicks creation_time_;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
