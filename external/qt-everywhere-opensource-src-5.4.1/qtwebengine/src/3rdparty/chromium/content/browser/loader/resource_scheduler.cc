// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "content/browser/loader/resource_scheduler.h"

#include "base/stl_util.h"
#include "content/common/resource_messages.h"
#include "content/browser/loader/resource_message_delegate.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/http/http_server_properties.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"

namespace content {

static const size_t kMaxNumDelayableRequestsPerClient = 10;
static const size_t kMaxNumDelayableRequestsPerHost = 6;


struct ResourceScheduler::RequestPriorityParams {
  RequestPriorityParams()
    : priority(net::DEFAULT_PRIORITY),
      intra_priority(0) {
  }

  RequestPriorityParams(net::RequestPriority priority, int intra_priority)
    : priority(priority),
      intra_priority(intra_priority) {
  }

  bool operator==(const RequestPriorityParams& other) const {
    return (priority == other.priority) &&
        (intra_priority == other.intra_priority);
  }

  bool operator!=(const RequestPriorityParams& other) const {
    return !(*this == other);
  }

  bool GreaterThan(const RequestPriorityParams& other) const {
    if (priority != other.priority)
      return priority > other.priority;
    return intra_priority > other.intra_priority;
  }

  net::RequestPriority priority;
  int intra_priority;
};

class ResourceScheduler::RequestQueue {
 public:
  typedef std::multiset<ScheduledResourceRequest*, ScheduledResourceSorter>
      NetQueue;

  RequestQueue() : fifo_ordering_ids_(0) {}
  ~RequestQueue() {}

  // Adds |request| to the queue with given |priority|.
  void Insert(ScheduledResourceRequest* request);

  // Removes |request| from the queue.
  void Erase(ScheduledResourceRequest* request) {
    PointerMap::iterator it = pointers_.find(request);
    DCHECK(it != pointers_.end());
    if (it == pointers_.end())
      return;
    queue_.erase(it->second);
    pointers_.erase(it);
  }

  NetQueue::iterator GetNextHighestIterator() {
    return queue_.begin();
  }

  NetQueue::iterator End() {
    return queue_.end();
  }

  // Returns true if |request| is queued.
  bool IsQueued(ScheduledResourceRequest* request) const {
    return ContainsKey(pointers_, request);
  }

  // Returns true if no requests are queued.
  bool IsEmpty() const { return queue_.size() == 0; }

 private:
  typedef std::map<ScheduledResourceRequest*, NetQueue::iterator> PointerMap;

  uint32 MakeFifoOrderingId() {
    fifo_ordering_ids_ += 1;
    return fifo_ordering_ids_;
  }

  // Used to create an ordering ID for scheduled resources so that resources
  // with same priority/intra_priority stay in fifo order.
  uint32 fifo_ordering_ids_;

  NetQueue queue_;
  PointerMap pointers_;
};

// This is the handle we return to the ResourceDispatcherHostImpl so it can
// interact with the request.
class ResourceScheduler::ScheduledResourceRequest
    : public ResourceMessageDelegate,
      public ResourceThrottle {
 public:
  ScheduledResourceRequest(const ClientId& client_id,
                           net::URLRequest* request,
                           ResourceScheduler* scheduler,
                           const RequestPriorityParams& priority)
      : ResourceMessageDelegate(request),
        client_id_(client_id),
        request_(request),
        ready_(false),
        deferred_(false),
        scheduler_(scheduler),
        priority_(priority),
        fifo_ordering_(0),
        accounted_as_delayable_request_(false) {
    TRACE_EVENT_ASYNC_BEGIN1("net", "URLRequest", request_,
                             "url", request->url().spec());
  }

  virtual ~ScheduledResourceRequest() {
    scheduler_->RemoveRequest(this);
  }

  void Start() {
    TRACE_EVENT_ASYNC_STEP_PAST0("net", "URLRequest", request_, "Queued");
    ready_ = true;
    if (deferred_ && request_->status().is_success()) {
      deferred_ = false;
      controller()->Resume();
    }
  }

  void set_request_priority_params(const RequestPriorityParams& priority) {
    priority_ = priority;
  }
  const RequestPriorityParams& get_request_priority_params() const {
    return priority_;
  }
  const ClientId& client_id() const { return client_id_; }
  net::URLRequest* url_request() { return request_; }
  const net::URLRequest* url_request() const { return request_; }
  uint32 fifo_ordering() const { return fifo_ordering_; }
  void set_fifo_ordering(uint32 fifo_ordering) {
    fifo_ordering_ = fifo_ordering;
  }
  bool accounted_as_delayable_request() const {
    return accounted_as_delayable_request_;
  }
  void set_accounted_as_delayable_request(bool accounted) {
    accounted_as_delayable_request_ = accounted;
  }

 private:
  // ResourceMessageDelegate interface:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(ScheduledResourceRequest, message)
      IPC_MESSAGE_HANDLER(ResourceHostMsg_DidChangePriority, DidChangePriority)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  // ResourceThrottle interface:
  virtual void WillStartRequest(bool* defer) OVERRIDE {
    deferred_ = *defer = !ready_;
  }

  virtual const char* GetNameForLogging() const OVERRIDE {
    return "ResourceScheduler";
  }

  void DidChangePriority(int request_id, net::RequestPriority new_priority,
                         int intra_priority_value) {
    scheduler_->ReprioritizeRequest(this, new_priority, intra_priority_value);
  }

  ClientId client_id_;
  net::URLRequest* request_;
  bool ready_;
  bool deferred_;
  ResourceScheduler* scheduler_;
  RequestPriorityParams priority_;
  uint32 fifo_ordering_;
  // True if the request is delayable in |in_flight_requests_|.
  bool accounted_as_delayable_request_;

  DISALLOW_COPY_AND_ASSIGN(ScheduledResourceRequest);
};

bool ResourceScheduler::ScheduledResourceSorter::operator()(
    const ScheduledResourceRequest* a,
    const ScheduledResourceRequest* b) const {
  // Want the set to be ordered first by decreasing priority, then by
  // decreasing intra_priority.
  // ie. with (priority, intra_priority)
  // [(1, 0), (1, 0), (0, 100), (0, 0)]
  if (a->get_request_priority_params() != b->get_request_priority_params())
    return a->get_request_priority_params().GreaterThan(
        b->get_request_priority_params());

  // If priority/intra_priority is the same, fall back to fifo ordering.
  // std::multiset doesn't guarantee this until c++11.
  return a->fifo_ordering() < b->fifo_ordering();
}

void ResourceScheduler::RequestQueue::Insert(
    ScheduledResourceRequest* request) {
  DCHECK(!ContainsKey(pointers_, request));
  request->set_fifo_ordering(MakeFifoOrderingId());
  pointers_[request] = queue_.insert(request);
}

// Each client represents a tab.
class ResourceScheduler::Client {
 public:
  Client()
      : has_body_(false),
        using_spdy_proxy_(false),
        total_delayable_count_(0) {}
  ~Client() {}

  void ScheduleRequest(
      net::URLRequest* url_request,
      ScheduledResourceRequest* request) {
    if (ShouldStartRequest(request) == START_REQUEST) {
      StartRequest(request);
    } else {
      pending_requests_.Insert(request);
    }
  }

  void RemoveRequest(ScheduledResourceRequest* request) {
    if (pending_requests_.IsQueued(request)) {
      pending_requests_.Erase(request);
      DCHECK(!ContainsKey(in_flight_requests_, request));
    } else {
      EraseInFlightRequest(request);

      // Removing this request may have freed up another to load.
      LoadAnyStartablePendingRequests();
    }
  }

  RequestSet RemoveAllRequests() {
    RequestSet unowned_requests;
    for (RequestSet::iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      unowned_requests.insert(*it);
      (*it)->set_accounted_as_delayable_request(false);
    }
    ClearInFlightRequests();
    return unowned_requests;
  }

  void OnNavigate() {
    has_body_ = false;
  }

  void OnWillInsertBody() {
    has_body_ = true;
    LoadAnyStartablePendingRequests();
  }

  void OnReceivedSpdyProxiedHttpResponse() {
    if (!using_spdy_proxy_) {
      using_spdy_proxy_ = true;
      LoadAnyStartablePendingRequests();
    }
  }

  void ReprioritizeRequest(ScheduledResourceRequest* request,
                           RequestPriorityParams old_priority_params,
                           RequestPriorityParams new_priority_params) {
    request->url_request()->SetPriority(new_priority_params.priority);
    request->set_request_priority_params(new_priority_params);
    if (!pending_requests_.IsQueued(request)) {
      DCHECK(ContainsKey(in_flight_requests_, request));
      // The priority and SPDY support may have changed, so update the
      // delayable count.
      SetRequestDelayable(request, IsDelayableRequest(request));
      // Request has already started.
      return;
    }

    pending_requests_.Erase(request);
    pending_requests_.Insert(request);

    if (new_priority_params.priority > old_priority_params.priority) {
      // Check if this request is now able to load at its new priority.
      LoadAnyStartablePendingRequests();
    }
  }

 private:
  enum ShouldStartReqResult {
      DO_NOT_START_REQUEST_AND_STOP_SEARCHING = -2,
      DO_NOT_START_REQUEST_AND_KEEP_SEARCHING = -1,
      START_REQUEST = 1,
  };

  void InsertInFlightRequest(ScheduledResourceRequest* request) {
    in_flight_requests_.insert(request);
    if (IsDelayableRequest(request))
      SetRequestDelayable(request, true);
  }

  void EraseInFlightRequest(ScheduledResourceRequest* request) {
    size_t erased = in_flight_requests_.erase(request);
    DCHECK_EQ(1u, erased);
    SetRequestDelayable(request, false);
    DCHECK_LE(total_delayable_count_, in_flight_requests_.size());
  }

  void ClearInFlightRequests() {
    in_flight_requests_.clear();
    total_delayable_count_ = 0;
  }

  bool IsDelayableRequest(ScheduledResourceRequest* request) {
    if (request->url_request()->priority() < net::LOW) {
      net::HostPortPair host_port_pair =
          net::HostPortPair::FromURL(request->url_request()->url());
      net::HttpServerProperties& http_server_properties =
          *request->url_request()->context()->http_server_properties();
      if (!http_server_properties.SupportsSpdy(host_port_pair)) {
        return true;
      }
    }
    return false;
  }

  void SetRequestDelayable(ScheduledResourceRequest* request,
                           bool delayable) {
    if (request->accounted_as_delayable_request() == delayable)
      return;
    if (delayable)
      total_delayable_count_++;
    else
      total_delayable_count_--;
    request->set_accounted_as_delayable_request(delayable);
  }

  bool ShouldKeepSearching(
      const net::HostPortPair& active_request_host) const {
    size_t same_host_count = 0;
    for (RequestSet::const_iterator it = in_flight_requests_.begin();
         it != in_flight_requests_.end(); ++it) {
      net::HostPortPair host_port_pair =
          net::HostPortPair::FromURL((*it)->url_request()->url());
      if (active_request_host.Equals(host_port_pair)) {
        same_host_count++;
        if (same_host_count >= kMaxNumDelayableRequestsPerHost)
          return true;
      }
    }
    return false;
  }

  void StartRequest(ScheduledResourceRequest* request) {
    InsertInFlightRequest(request);
    request->Start();
  }

  // ShouldStartRequest is the main scheduling algorithm.
  //
  // Requests are categorized into two categories:
  //
  // 1. Immediately issued requests, which are:
  //
  //   * Higher priority requests (>= net::LOW).
  //   * Synchronous requests.
  //   * Requests to SPDY-capable origin servers.
  //   * Non-HTTP[S] requests.
  //
  // 2. The remainder are delayable requests, which follow these rules:
  //
  //   * If no high priority requests are in flight, start loading low priority
  //      requests.
  //   * Once the renderer has a <body>, start loading delayable requests.
  //   * Never exceed 10 delayable requests in flight per client.
  //   * Never exceed 6 delayable requests for a given host.
  //   * Prior to <body>, allow one delayable request to load at a time.
  ShouldStartReqResult ShouldStartRequest(
      ScheduledResourceRequest* request) const {
    const net::URLRequest& url_request = *request->url_request();
    // TODO(simonjam): This may end up causing disk contention. We should
    // experiment with throttling if that happens.
    if (!url_request.url().SchemeIsHTTPOrHTTPS()) {
      return START_REQUEST;
    }

    if (using_spdy_proxy_ && url_request.url().SchemeIs("http")) {
      return START_REQUEST;
    }

    net::HttpServerProperties& http_server_properties =
        *url_request.context()->http_server_properties();

    if (url_request.priority() >= net::LOW ||
        !ResourceRequestInfo::ForRequest(&url_request)->IsAsync()) {
      return START_REQUEST;
    }

    net::HostPortPair host_port_pair =
        net::HostPortPair::FromURL(url_request.url());

    // TODO(willchan): We should really improve this algorithm as described in
    // crbug.com/164101. Also, theoretically we should not count a SPDY request
    // against the delayable requests limit.
    if (http_server_properties.SupportsSpdy(host_port_pair)) {
      return START_REQUEST;
    }

    size_t num_delayable_requests_in_flight = total_delayable_count_;
    if (num_delayable_requests_in_flight >= kMaxNumDelayableRequestsPerClient) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    if (ShouldKeepSearching(host_port_pair)) {
      // There may be other requests for other hosts we'd allow,
      // so keep checking.
      return DO_NOT_START_REQUEST_AND_KEEP_SEARCHING;
    }

    bool have_immediate_requests_in_flight =
        in_flight_requests_.size() > num_delayable_requests_in_flight;
    if (have_immediate_requests_in_flight && !has_body_ &&
        num_delayable_requests_in_flight != 0) {
      return DO_NOT_START_REQUEST_AND_STOP_SEARCHING;
    }

    return START_REQUEST;
  }

  void LoadAnyStartablePendingRequests() {
    // We iterate through all the pending requests, starting with the highest
    // priority one. For each entry, one of three things can happen:
    // 1) We start the request, remove it from the list, and keep checking.
    // 2) We do NOT start the request, but ShouldStartRequest() signals us that
    //     there may be room for other requests, so we keep checking and leave
    //     the previous request still in the list.
    // 3) We do not start the request, same as above, but StartRequest() tells
    //     us there's no point in checking any further requests.
    RequestQueue::NetQueue::iterator request_iter =
        pending_requests_.GetNextHighestIterator();

    while (request_iter != pending_requests_.End()) {
      ScheduledResourceRequest* request = *request_iter;
      ShouldStartReqResult query_result = ShouldStartRequest(request);

      if (query_result == START_REQUEST) {
        pending_requests_.Erase(request);
        StartRequest(request);

        // StartRequest can modify the pending list, so we (re)start evaluation
        // from the currently highest priority request. Avoid copying a singular
        // iterator, which would trigger undefined behavior.
        if (pending_requests_.GetNextHighestIterator() ==
            pending_requests_.End())
          break;
        request_iter = pending_requests_.GetNextHighestIterator();
      } else if (query_result == DO_NOT_START_REQUEST_AND_KEEP_SEARCHING) {
        ++request_iter;
        continue;
      } else {
        DCHECK(query_result == DO_NOT_START_REQUEST_AND_STOP_SEARCHING);
        break;
      }
    }
  }

  bool has_body_;
  bool using_spdy_proxy_;
  RequestQueue pending_requests_;
  RequestSet in_flight_requests_;
  // The number of delayable in-flight requests.
  size_t total_delayable_count_;
};

ResourceScheduler::ResourceScheduler() {
}

ResourceScheduler::~ResourceScheduler() {
  DCHECK(unowned_requests_.empty());
  DCHECK(client_map_.empty());
}

scoped_ptr<ResourceThrottle> ResourceScheduler::ScheduleRequest(
    int child_id,
    int route_id,
    net::URLRequest* url_request) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  scoped_ptr<ScheduledResourceRequest> request(
      new ScheduledResourceRequest(client_id, url_request, this,
          RequestPriorityParams(url_request->priority(), 0)));

  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end()) {
    // There are several ways this could happen:
    // 1. <a ping> requests don't have a route_id.
    // 2. Most unittests don't send the IPCs needed to register Clients.
    // 3. The tab is closed while a RequestResource IPC is in flight.
    unowned_requests_.insert(request.get());
    request->Start();
    return request.PassAs<ResourceThrottle>();
  }

  Client* client = it->second;
  client->ScheduleRequest(url_request, request.get());
  return request.PassAs<ResourceThrottle>();
}

void ResourceScheduler::RemoveRequest(ScheduledResourceRequest* request) {
  DCHECK(CalledOnValidThread());
  if (ContainsKey(unowned_requests_, request)) {
    unowned_requests_.erase(request);
    return;
  }

  ClientMap::iterator client_it = client_map_.find(request->client_id());
  if (client_it == client_map_.end()) {
    return;
  }

  Client* client = client_it->second;
  client->RemoveRequest(request);
}

void ResourceScheduler::OnClientCreated(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  DCHECK(!ContainsKey(client_map_, client_id));

  client_map_[client_id] = new Client;
}

void ResourceScheduler::OnClientDeleted(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);
  DCHECK(ContainsKey(client_map_, client_id));
  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end())
    return;

  Client* client = it->second;

  // FYI, ResourceDispatcherHost cancels all of the requests after this function
  // is called. It should end up canceling all of the requests except for a
  // cross-renderer navigation.
  RequestSet client_unowned_requests = client->RemoveAllRequests();
  for (RequestSet::iterator it = client_unowned_requests.begin();
       it != client_unowned_requests.end(); ++it) {
    unowned_requests_.insert(*it);
  }

  delete client;
  client_map_.erase(it);
}

void ResourceScheduler::OnNavigate(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    return;
  }

  Client* client = it->second;
  client->OnNavigate();
}

void ResourceScheduler::OnWillInsertBody(int child_id, int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator it = client_map_.find(client_id);
  if (it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    return;
  }

  Client* client = it->second;
  client->OnWillInsertBody();
}

void ResourceScheduler::OnReceivedSpdyProxiedHttpResponse(
    int child_id,
    int route_id) {
  DCHECK(CalledOnValidThread());
  ClientId client_id = MakeClientId(child_id, route_id);

  ClientMap::iterator client_it = client_map_.find(client_id);
  if (client_it == client_map_.end()) {
    return;
  }

  Client* client = client_it->second;
  client->OnReceivedSpdyProxiedHttpResponse();
}

void ResourceScheduler::ReprioritizeRequest(ScheduledResourceRequest* request,
                                            net::RequestPriority new_priority,
                                            int new_intra_priority_value) {
  if (request->url_request()->load_flags() & net::LOAD_IGNORE_LIMITS) {
    // We should not be re-prioritizing requests with the
    // IGNORE_LIMITS flag.
    NOTREACHED();
    return;
  }
  RequestPriorityParams new_priority_params(new_priority,
      new_intra_priority_value);
  RequestPriorityParams old_priority_params =
      request->get_request_priority_params();

  DCHECK(old_priority_params != new_priority_params);

  ClientMap::iterator client_it = client_map_.find(request->client_id());
  if (client_it == client_map_.end()) {
    // The client was likely deleted shortly before we received this IPC.
    request->url_request()->SetPriority(new_priority_params.priority);
    request->set_request_priority_params(new_priority_params);
    return;
  }

  if (old_priority_params == new_priority_params)
    return;

  Client *client = client_it->second;
  client->ReprioritizeRequest(
      request, old_priority_params, new_priority_params);
}

ResourceScheduler::ClientId ResourceScheduler::MakeClientId(
    int child_id, int route_id) {
  return (static_cast<ResourceScheduler::ClientId>(child_id) << 32) | route_id;
}

}  // namespace content
