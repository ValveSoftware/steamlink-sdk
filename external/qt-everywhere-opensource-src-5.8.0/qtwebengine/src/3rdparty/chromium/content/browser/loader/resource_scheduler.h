// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"
#include "net/base/priority_queue.h"
#include "net/base/request_priority.h"

namespace net {
class HostPortPair;
class URLRequest;
}

namespace content {
class ResourceThrottle;

// There is one ResourceScheduler. All renderer-initiated HTTP requests are
// expected to pass through it.
//
// There are two types of input to the scheduler:
// 1. Requests to start, cancel, or finish fetching a resource.
// 2. Notifications for renderer events, such as new tabs, navigation and
//    painting.
//
// These input come from different threads, so they may not be in sync. The UI
// thread is considered the authority on renderer lifetime, which means some
// IPCs may be meaningless if they arrive after the UI thread signals a renderer
// has been deleted.
//
// The ResourceScheduler tracks many Clients, which should correlate with tabs.
// A client is uniquely identified by its child_id and route_id.
//
// Each Client may have many Requests in flight. Requests are uniquely
// identified within a Client by its ScheduledResourceRequest.
//
// Users should call ScheduleRequest() to notify this ResourceScheduler of a new
// request. The returned ResourceThrottle should be destroyed when the load
// finishes or is canceled, before the net::URLRequest.
//
// The scheduler may defer issuing the request via the ResourceThrottle
// interface or it may alter the request's priority by calling set_priority() on
// the URLRequest.
class CONTENT_EXPORT ResourceScheduler : public base::NonThreadSafe {
 public:
  ResourceScheduler();
  ~ResourceScheduler();

  // Requests that this ResourceScheduler schedule, and eventually loads, the
  // specified |url_request|. Caller should delete the returned ResourceThrottle
  // when the load completes or is canceled, before |url_request| is deleted.
  std::unique_ptr<ResourceThrottle> ScheduleRequest(
      int child_id,
      int route_id,
      bool is_async,
      net::URLRequest* url_request);

  // Signals from the UI thread, posted as tasks on the IO thread:

  // Called when a renderer is created.
  void OnClientCreated(int child_id, int route_id);

  // Called when a renderer is destroyed.
  void OnClientDeleted(int child_id, int route_id);

  // Called when a renderer stops or restarts loading.
  void OnLoadingStateChanged(int child_id, int route_id, bool is_loaded);

  // Signals from IPC messages directly from the renderers:

  // Called when a client navigates to a new main document.
  void OnNavigate(int child_id, int route_id);

  // Called when the client has parsed the <body> element. This is a signal that
  // resource loads won't interfere with first paint.
  void OnWillInsertBody(int child_id, int route_id);

  // Signals from the IO thread:

  // Called when we received a response to a http request that was served
  // from a proxy using SPDY.
  void OnReceivedSpdyProxiedHttpResponse(int child_id, int route_id);

  // Client functions:

  // Returns true if at least one client is currently loading.
  bool HasLoadingClients() const;

  // Update the priority for |request|. Modifies request->priority(), and may
  // start the request loading if it wasn't already started.
  void ReprioritizeRequest(net::URLRequest* request,
                           net::RequestPriority new_priority,
                           int intra_priority_value);

 private:
  // Returns true if limiting of outstanding requests is enabled.
  bool limit_outstanding_requests() const {
    return limit_outstanding_requests_;
  }

  // Returns the outstanding request limit.  Only valid if
  // |IsLimitingOutstandingRequests()|.
  size_t outstanding_request_limit() const {
    return outstanding_request_limit_;
  }

  // Returns the maximum number of delayable requests to all be in-flight at
  // any point in time (across all hosts).
  size_t max_num_delayable_requests() const {
    return max_num_delayable_requests_;
  }

  class RequestQueue;
  class ScheduledResourceRequest;
  struct RequestPriorityParams;
  struct ScheduledResourceSorter {
    bool operator()(const ScheduledResourceRequest* a,
                    const ScheduledResourceRequest* b) const;
  };
  class Client;

  typedef int64_t ClientId;
  typedef std::map<ClientId, Client*> ClientMap;
  typedef std::set<ScheduledResourceRequest*> RequestSet;

  // Called when a ScheduledResourceRequest is destroyed.
  void RemoveRequest(ScheduledResourceRequest* request);

  // Returns the client ID for the given |child_id| and |route_id| combo.
  ClientId MakeClientId(int child_id, int route_id);

  // Returns the client for the given |child_id| and |route_id| combo.
  Client* GetClient(int child_id, int route_id);

  ClientMap client_map_;
  bool limit_outstanding_requests_;
  size_t outstanding_request_limit_;
  size_t max_num_delayable_requests_;
  RequestSet unowned_requests_;

  DISALLOW_COPY_AND_ASSIGN(ResourceScheduler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_SCHEDULER_H_
