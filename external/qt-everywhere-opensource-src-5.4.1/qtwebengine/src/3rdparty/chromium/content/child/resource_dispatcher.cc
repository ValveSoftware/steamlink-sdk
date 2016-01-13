// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/child/resource_dispatcher.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_path.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "content/child/request_extra_data.h"
#include "content/child/request_info.h"
#include "content/child/site_isolation_policy.h"
#include "content/child/sync_load_response.h"
#include "content/child/threaded_data_provider.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/resource_messages.h"
#include "content/public/child/request_peer.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/resource_response.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "webkit/child/resource_loader_bridge.h"
#include "webkit/common/resource_type.h"

using webkit_glue::ResourceLoaderBridge;

namespace content {

namespace {

// Converts |time| from a remote to local TimeTicks, overwriting the original
// value.
void RemoteToLocalTimeTicks(
    const InterProcessTimeTicksConverter& converter,
    base::TimeTicks* time) {
  RemoteTimeTicks remote_time = RemoteTimeTicks::FromTimeTicks(*time);
  *time = converter.ToLocalTimeTicks(remote_time).ToTimeTicks();
}

void CrashOnMapFailure() {
#if defined(OS_WIN)
  DWORD last_err = GetLastError();
  base::debug::Alias(&last_err);
#endif
  CHECK(false);
}

// Each resource request is assigned an ID scoped to this process.
int MakeRequestID() {
  // NOTE: The resource_dispatcher_host also needs probably unique
  // request_ids, so they count down from -2 (-1 is a special we're
  // screwed value), while the renderer process counts up.
  static int next_request_id = 0;
  return next_request_id++;
}

}  // namespace

// ResourceLoaderBridge implementation ----------------------------------------

class IPCResourceLoaderBridge : public ResourceLoaderBridge {
 public:
  IPCResourceLoaderBridge(ResourceDispatcher* dispatcher,
                          const RequestInfo& request_info);
  virtual ~IPCResourceLoaderBridge();

  // ResourceLoaderBridge
  virtual void SetRequestBody(ResourceRequestBody* request_body) OVERRIDE;
  virtual bool Start(RequestPeer* peer) OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void SetDefersLoading(bool value) OVERRIDE;
  virtual void DidChangePriority(net::RequestPriority new_priority,
                                 int intra_priority_value) OVERRIDE;
  virtual bool AttachThreadedDataReceiver(
      blink::WebThreadedDataReceiver* threaded_data_receiver) OVERRIDE;
  virtual void SyncLoad(SyncLoadResponse* response) OVERRIDE;

 private:
  // The resource dispatcher for this loader.  The bridge doesn't own it, but
  // it's guaranteed to outlive the bridge.
  ResourceDispatcher* dispatcher_;

  // The request to send, created on initialization for modification and
  // appending data.
  ResourceHostMsg_Request request_;

  // ID for the request, valid once Start()ed, -1 if not valid yet.
  int request_id_;

  // The routing id used when sending IPC messages.
  int routing_id_;

  // The security origin of the frame that initiates this request.
  GURL frame_origin_;

  bool is_synchronous_request_;
};

IPCResourceLoaderBridge::IPCResourceLoaderBridge(
    ResourceDispatcher* dispatcher,
    const RequestInfo& request_info)
    : dispatcher_(dispatcher),
      request_id_(-1),
      routing_id_(request_info.routing_id),
      is_synchronous_request_(false) {
  DCHECK(dispatcher_) << "no resource dispatcher";
  request_.method = request_info.method;
  request_.url = request_info.url;
  request_.first_party_for_cookies = request_info.first_party_for_cookies;
  request_.referrer = request_info.referrer;
  request_.referrer_policy = request_info.referrer_policy;
  request_.headers = request_info.headers;
  request_.load_flags = request_info.load_flags;
  request_.origin_pid = request_info.requestor_pid;
  request_.resource_type = request_info.request_type;
  request_.priority = request_info.priority;
  request_.request_context = request_info.request_context;
  request_.appcache_host_id = request_info.appcache_host_id;
  request_.download_to_file = request_info.download_to_file;
  request_.has_user_gesture = request_info.has_user_gesture;

  const RequestExtraData kEmptyData;
  const RequestExtraData* extra_data;
  if (request_info.extra_data)
    extra_data = static_cast<RequestExtraData*>(request_info.extra_data);
  else
    extra_data = &kEmptyData;
  request_.visiblity_state = extra_data->visibility_state();
  request_.render_frame_id = extra_data->render_frame_id();
  request_.is_main_frame = extra_data->is_main_frame();
  request_.parent_is_main_frame = extra_data->parent_is_main_frame();
  request_.parent_render_frame_id = extra_data->parent_render_frame_id();
  request_.allow_download = extra_data->allow_download();
  request_.transition_type = extra_data->transition_type();
  request_.should_replace_current_entry =
      extra_data->should_replace_current_entry();
  request_.transferred_request_child_id =
      extra_data->transferred_request_child_id();
  request_.transferred_request_request_id =
      extra_data->transferred_request_request_id();
  request_.service_worker_provider_id =
      extra_data->service_worker_provider_id();
  frame_origin_ = extra_data->frame_origin();
}

IPCResourceLoaderBridge::~IPCResourceLoaderBridge() {
  // we remove our hook for the resource dispatcher only when going away, since
  // it doesn't keep track of whether we've force terminated the request
  if (request_id_ >= 0) {
    // this operation may fail, as the dispatcher will have preemptively
    // removed us when the renderer sends the ReceivedAllData message.
    dispatcher_->RemovePendingRequest(request_id_);
  }
}

void IPCResourceLoaderBridge::SetRequestBody(
    ResourceRequestBody* request_body) {
  DCHECK(request_id_ == -1) << "request already started";
  request_.request_body = request_body;
}

// Writes a footer on the message and sends it
bool IPCResourceLoaderBridge::Start(RequestPeer* peer) {
  if (request_id_ != -1) {
    NOTREACHED() << "Starting a request twice";
    return false;
  }

  // generate the request ID, and append it to the message
  request_id_ = dispatcher_->AddPendingRequest(peer,
                                               request_.resource_type,
                                               request_.origin_pid,
                                               frame_origin_,
                                               request_.url,
                                               request_.download_to_file);

  return dispatcher_->message_sender()->Send(
      new ResourceHostMsg_RequestResource(routing_id_, request_id_, request_));
}

void IPCResourceLoaderBridge::Cancel() {
  if (request_id_ < 0) {
    NOTREACHED() << "Trying to cancel an unstarted request";
    return;
  }

  if (!is_synchronous_request_) {
    // This also removes the the request from the dispatcher.
    dispatcher_->CancelPendingRequest(request_id_);
  }
}

void IPCResourceLoaderBridge::SetDefersLoading(bool value) {
  if (request_id_ < 0) {
    NOTREACHED() << "Trying to (un)defer an unstarted request";
    return;
  }

  dispatcher_->SetDefersLoading(request_id_, value);
}

void IPCResourceLoaderBridge::DidChangePriority(
    net::RequestPriority new_priority,
    int intra_priority_value) {
  if (request_id_ < 0) {
    NOTREACHED() << "Trying to change priority of an unstarted request";
    return;
  }

  dispatcher_->DidChangePriority(
      request_id_, new_priority, intra_priority_value);
}

bool IPCResourceLoaderBridge::AttachThreadedDataReceiver(
    blink::WebThreadedDataReceiver* threaded_data_receiver) {
  if (request_id_ < 0) {
    NOTREACHED() << "Trying to attach threaded receiver on unstarted request";
    return false;
  }

  return dispatcher_->AttachThreadedDataReceiver(request_id_,
                                                 threaded_data_receiver);
}

void IPCResourceLoaderBridge::SyncLoad(SyncLoadResponse* response) {
  if (request_id_ != -1) {
    NOTREACHED() << "Starting a request twice";
    response->error_code = net::ERR_FAILED;
    return;
  }

  request_id_ = MakeRequestID();
  is_synchronous_request_ = true;

  SyncLoadResult result;
  IPC::SyncMessage* msg = new ResourceHostMsg_SyncLoad(routing_id_, request_id_,
                                                       request_, &result);
  // NOTE: This may pump events (see RenderThread::Send).
  if (!dispatcher_->message_sender()->Send(msg)) {
    response->error_code = net::ERR_FAILED;
    return;
  }

  response->error_code = result.error_code;
  response->url = result.final_url;
  response->headers = result.headers;
  response->mime_type = result.mime_type;
  response->charset = result.charset;
  response->request_time = result.request_time;
  response->response_time = result.response_time;
  response->encoded_data_length = result.encoded_data_length;
  response->load_timing = result.load_timing;
  response->devtools_info = result.devtools_info;
  response->data.swap(result.data);
  response->download_file_path = result.download_file_path;
}

// ResourceDispatcher ---------------------------------------------------------

ResourceDispatcher::ResourceDispatcher(IPC::Sender* sender)
    : message_sender_(sender),
      weak_factory_(this),
      delegate_(NULL),
      io_timestamp_(base::TimeTicks()) {
}

ResourceDispatcher::~ResourceDispatcher() {
}

// ResourceDispatcher implementation ------------------------------------------

bool ResourceDispatcher::OnMessageReceived(const IPC::Message& message) {
  if (!IsResourceDispatcherMessage(message)) {
    return false;
  }

  int request_id;

  PickleIterator iter(message);
  if (!message.ReadInt(&iter, &request_id)) {
    NOTREACHED() << "malformed resource message";
    return true;
  }

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info) {
    // Release resources in the message if it is a data message.
    ReleaseResourcesInDataMessage(message);
    return true;
  }

  if (request_info->is_deferred) {
    request_info->deferred_message_queue.push_back(new IPC::Message(message));
    return true;
  }
  // Make sure any deferred messages are dispatched before we dispatch more.
  if (!request_info->deferred_message_queue.empty()) {
    FlushDeferredMessages(request_id);
    // The request could have been deferred now. If yes then the current
    // message has to be queued up. The request_info instance should remain
    // valid here as there are pending messages for it.
    DCHECK(pending_requests_.find(request_id) != pending_requests_.end());
    if (request_info->is_deferred) {
      request_info->deferred_message_queue.push_back(new IPC::Message(message));
      return true;
    }
  }

  DispatchMessage(message);
  return true;
}

ResourceDispatcher::PendingRequestInfo*
ResourceDispatcher::GetPendingRequestInfo(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    // This might happen for kill()ed requests on the webkit end.
    return NULL;
  }
  return &(it->second);
}

void ResourceDispatcher::OnUploadProgress(int request_id, int64 position,
                                          int64 size) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnUploadProgress(position, size);

  // Acknowledge receipt
  message_sender_->Send(new ResourceHostMsg_UploadProgress_ACK(request_id));
}

void ResourceDispatcher::OnReceivedResponse(
    int request_id, const ResourceResponseHead& response_head) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedResponse");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = ConsumeIOTimestamp();

  if (delegate_) {
    RequestPeer* new_peer =
        delegate_->OnReceivedResponse(
            request_info->peer, response_head.mime_type, request_info->url);
    if (new_peer)
      request_info->peer = new_peer;
  }

  ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  request_info->site_isolation_metadata =
      SiteIsolationPolicy::OnReceivedResponse(request_info->frame_origin,
                                              request_info->response_url,
                                              request_info->resource_type,
                                              request_info->origin_pid,
                                              renderer_response_info);
  request_info->peer->OnReceivedResponse(renderer_response_info);
}

void ResourceDispatcher::OnReceivedCachedMetadata(
      int request_id, const std::vector<char>& data) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  if (data.size())
    request_info->peer->OnReceivedCachedMetadata(&data.front(), data.size());
}

void ResourceDispatcher::OnSetDataBuffer(int request_id,
                                         base::SharedMemoryHandle shm_handle,
                                         int shm_size,
                                         base::ProcessId renderer_pid) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnSetDataBuffer");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  bool shm_valid = base::SharedMemory::IsHandleValid(shm_handle);
  CHECK((shm_valid && shm_size > 0) || (!shm_valid && !shm_size));

  request_info->buffer.reset(
      new base::SharedMemory(shm_handle, true));  // read only

  bool ok = request_info->buffer->Map(shm_size);
  if (!ok) {
    // Added to help debug crbug/160401.
    base::ProcessId renderer_pid_copy = renderer_pid;
    base::debug::Alias(&renderer_pid_copy);

    base::SharedMemoryHandle shm_handle_copy = shm_handle;
    base::debug::Alias(&shm_handle_copy);

    CrashOnMapFailure();
    return;
  }

  request_info->buffer_size = shm_size;
}

void ResourceDispatcher::OnReceivedData(int request_id,
                                        int data_offset,
                                        int data_length,
                                        int encoded_data_length) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedData");
  DCHECK_GT(data_length, 0);
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  bool send_ack = true;
  if (request_info && data_length > 0) {
    CHECK(base::SharedMemory::IsHandleValid(request_info->buffer->handle()));
    CHECK_GE(request_info->buffer_size, data_offset + data_length);

    // Ensure that the SHM buffer remains valid for the duration of this scope.
    // It is possible for CancelPendingRequest() to be called before we exit
    // this scope.
    linked_ptr<base::SharedMemory> retain_buffer(request_info->buffer);

    base::TimeTicks time_start = base::TimeTicks::Now();

    const char* data_start = static_cast<char*>(request_info->buffer->memory());
    CHECK(data_start);
    CHECK(data_start + data_offset);
    const char* data_ptr = data_start + data_offset;

    // Check whether this response data is compliant with our cross-site
    // document blocking policy. We only do this for the first packet.
    std::string alternative_data;
    if (request_info->site_isolation_metadata.get()) {
      request_info->blocked_response =
          SiteIsolationPolicy::ShouldBlockResponse(
              request_info->site_isolation_metadata, data_ptr, data_length,
              &alternative_data);
      request_info->site_isolation_metadata.reset();

      // When the response is blocked we may have any alternative data to
      // send to the renderer. When |alternative_data| is zero-sized, we do not
      // call peer's callback.
      if (request_info->blocked_response && !alternative_data.empty()) {
        data_ptr = alternative_data.data();
        data_length = alternative_data.size();
        encoded_data_length = alternative_data.size();
      }
    }

    if (!request_info->blocked_response || !alternative_data.empty()) {
      if (request_info->threaded_data_provider) {
        request_info->threaded_data_provider->OnReceivedDataOnForegroundThread(
            data_ptr, data_length, encoded_data_length);
        // A threaded data provider will take care of its own ACKing, as the
        // data may be processed later on another thread.
        send_ack = false;
      } else {
        request_info->peer->OnReceivedData(
            data_ptr, data_length, encoded_data_length);
      }
    }

    UMA_HISTOGRAM_TIMES("ResourceDispatcher.OnReceivedDataTime",
                        base::TimeTicks::Now() - time_start);
  }

  // Acknowledge the reception of this data.
  if (send_ack)
    message_sender_->Send(new ResourceHostMsg_DataReceived_ACK(request_id));
}

void ResourceDispatcher::OnDownloadedData(int request_id,
                                          int data_len,
                                          int encoded_data_length) {
  // Acknowledge the reception of this message.
  message_sender_->Send(new ResourceHostMsg_DataDownloaded_ACK(request_id));

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnDownloadedData(data_len, encoded_data_length);
}

void ResourceDispatcher::OnReceivedRedirect(
    int request_id,
    const GURL& new_url,
    const GURL& new_first_party_for_cookies,
    const ResourceResponseHead& response_head) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedRedirect");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = ConsumeIOTimestamp();

  ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  if (request_info->peer->OnReceivedRedirect(
          new_url, new_first_party_for_cookies, renderer_response_info)) {
    // Double-check if the request is still around. The call above could
    // potentially remove it.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info)
      return;
    // We update the response_url here so that we can send it to
    // SiteIsolationPolicy later when OnReceivedResponse is called.
    request_info->response_url = new_url;
    request_info->pending_redirect_message.reset(
        new ResourceHostMsg_FollowRedirect(request_id));
    if (!request_info->is_deferred) {
      FollowPendingRedirect(request_id, *request_info);
    }
  } else {
    CancelPendingRequest(request_id);
  }
}

void ResourceDispatcher::FollowPendingRedirect(
    int request_id,
    PendingRequestInfo& request_info) {
  IPC::Message* msg = request_info.pending_redirect_message.release();
  if (msg)
    message_sender_->Send(msg);
}

void ResourceDispatcher::OnRequestComplete(
    int request_id,
    const ResourceMsg_RequestCompleteData& request_complete_data) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnRequestComplete");

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->completion_time = ConsumeIOTimestamp();
  request_info->buffer.reset();
  request_info->buffer_size = 0;

  RequestPeer* peer = request_info->peer;

  if (delegate_) {
    RequestPeer* new_peer =
        delegate_->OnRequestComplete(
            request_info->peer, request_info->resource_type,
            request_complete_data.error_code);
    if (new_peer)
      request_info->peer = new_peer;
  }

  base::TimeTicks renderer_completion_time = ToRendererCompletionTime(
      *request_info, request_complete_data.completion_time);
  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  peer->OnCompletedRequest(request_complete_data.error_code,
                           request_complete_data.was_ignored_by_handler,
                           request_complete_data.exists_in_cache,
                           request_complete_data.security_info,
                           renderer_completion_time,
                           request_complete_data.encoded_data_length);
}

int ResourceDispatcher::AddPendingRequest(RequestPeer* callback,
                                          ResourceType::Type resource_type,
                                          int origin_pid,
                                          const GURL& frame_origin,
                                          const GURL& request_url,
                                          bool download_to_file) {
  // Compute a unique request_id for this renderer process.
  int id = MakeRequestID();
  pending_requests_[id] = PendingRequestInfo(callback,
                                             resource_type,
                                             origin_pid,
                                             frame_origin,
                                             request_url,
                                             download_to_file);
  return id;
}

bool ResourceDispatcher::RemovePendingRequest(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PendingRequestInfo& request_info = it->second;

  bool release_downloaded_file = request_info.download_to_file;

  ReleaseResourcesInMessageQueue(&request_info.deferred_message_queue);
  pending_requests_.erase(it);

  if (release_downloaded_file) {
    message_sender_->Send(
        new ResourceHostMsg_ReleaseDownloadedFile(request_id));
  }

  return true;
}

void ResourceDispatcher::CancelPendingRequest(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DVLOG(1) << "unknown request";
    return;
  }

  // Cancel the request, and clean it up so the bridge will receive no more
  // messages.
  message_sender_->Send(new ResourceHostMsg_CancelRequest(request_id));
  RemovePendingRequest(request_id);
}

void ResourceDispatcher::SetDefersLoading(int request_id, bool value) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  PendingRequestInfo& request_info = it->second;
  if (value) {
    request_info.is_deferred = value;
  } else if (request_info.is_deferred) {
    request_info.is_deferred = false;

    FollowPendingRedirect(request_id, request_info);

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ResourceDispatcher::FlushDeferredMessages,
                   weak_factory_.GetWeakPtr(),
                   request_id));
  }
}

void ResourceDispatcher::DidChangePriority(int request_id,
                                           net::RequestPriority new_priority,
                                           int intra_priority_value) {
  DCHECK(ContainsKey(pending_requests_, request_id));
  message_sender_->Send(new ResourceHostMsg_DidChangePriority(
      request_id, new_priority, intra_priority_value));
}

bool ResourceDispatcher::AttachThreadedDataReceiver(
    int request_id, blink::WebThreadedDataReceiver* threaded_data_receiver) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  DCHECK(request_info);

  if (request_info->buffer != NULL) {
    DCHECK(!request_info->threaded_data_provider);
    request_info->threaded_data_provider = new ThreadedDataProvider(
        request_id, threaded_data_receiver, request_info->buffer,
        request_info->buffer_size);
    return true;
  }

  return false;
}

ResourceDispatcher::PendingRequestInfo::PendingRequestInfo()
    : peer(NULL),
      threaded_data_provider(NULL),
      resource_type(ResourceType::SUB_RESOURCE),
      is_deferred(false),
      download_to_file(false),
      blocked_response(false),
      buffer_size(0) {
}

ResourceDispatcher::PendingRequestInfo::PendingRequestInfo(
    RequestPeer* peer,
    ResourceType::Type resource_type,
    int origin_pid,
    const GURL& frame_origin,
    const GURL& request_url,
    bool download_to_file)
    : peer(peer),
      threaded_data_provider(NULL),
      resource_type(resource_type),
      origin_pid(origin_pid),
      is_deferred(false),
      url(request_url),
      frame_origin(frame_origin),
      response_url(request_url),
      download_to_file(download_to_file),
      request_start(base::TimeTicks::Now()),
      blocked_response(false) {}

ResourceDispatcher::PendingRequestInfo::~PendingRequestInfo() {
  if (threaded_data_provider)
    threaded_data_provider->Stop();
}

void ResourceDispatcher::DispatchMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ResourceDispatcher, message)
    IPC_MESSAGE_HANDLER(ResourceMsg_UploadProgress, OnUploadProgress)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedResponse, OnReceivedResponse)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedCachedMetadata,
                        OnReceivedCachedMetadata)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedRedirect, OnReceivedRedirect)
    IPC_MESSAGE_HANDLER(ResourceMsg_SetDataBuffer, OnSetDataBuffer)
    IPC_MESSAGE_HANDLER(ResourceMsg_DataReceived, OnReceivedData)
    IPC_MESSAGE_HANDLER(ResourceMsg_DataDownloaded, OnDownloadedData)
    IPC_MESSAGE_HANDLER(ResourceMsg_RequestComplete, OnRequestComplete)
  IPC_END_MESSAGE_MAP()
}

void ResourceDispatcher::FlushDeferredMessages(int request_id) {
  PendingRequestList::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())  // The request could have become invalid.
    return;
  PendingRequestInfo& request_info = it->second;
  if (request_info.is_deferred)
    return;
  // Because message handlers could result in request_info being destroyed,
  // we need to work with a stack reference to the deferred queue.
  MessageQueue q;
  q.swap(request_info.deferred_message_queue);
  while (!q.empty()) {
    IPC::Message* m = q.front();
    q.pop_front();
    DispatchMessage(*m);
    delete m;
    // If this request is deferred in the context of the above message, then
    // we should honor the same and stop dispatching further messages.
    // We need to find the request again in the list as it may have completed
    // by now and the request_info instance above may be invalid.
    PendingRequestList::iterator index = pending_requests_.find(request_id);
    if (index != pending_requests_.end()) {
      PendingRequestInfo& pending_request = index->second;
      if (pending_request.is_deferred) {
        pending_request.deferred_message_queue.swap(q);
        return;
      }
    }
  }
}

ResourceLoaderBridge* ResourceDispatcher::CreateBridge(
    const RequestInfo& request_info) {
  return new IPCResourceLoaderBridge(this, request_info);
}

void ResourceDispatcher::ToResourceResponseInfo(
    const PendingRequestInfo& request_info,
    const ResourceResponseHead& browser_info,
    ResourceResponseInfo* renderer_info) const {
  *renderer_info = browser_info;
  if (request_info.request_start.is_null() ||
      request_info.response_start.is_null() ||
      browser_info.request_start.is_null() ||
      browser_info.response_start.is_null() ||
      browser_info.load_timing.request_start.is_null()) {
    return;
  }
  InterProcessTimeTicksConverter converter(
      LocalTimeTicks::FromTimeTicks(request_info.request_start),
      LocalTimeTicks::FromTimeTicks(request_info.response_start),
      RemoteTimeTicks::FromTimeTicks(browser_info.request_start),
      RemoteTimeTicks::FromTimeTicks(browser_info.response_start));

  net::LoadTimingInfo* load_timing = &renderer_info->load_timing;
  RemoteToLocalTimeTicks(converter, &load_timing->request_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_start);
  RemoteToLocalTimeTicks(converter, &load_timing->proxy_resolve_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.dns_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.connect_end);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_start);
  RemoteToLocalTimeTicks(converter, &load_timing->connect_timing.ssl_end);
  RemoteToLocalTimeTicks(converter, &load_timing->send_start);
  RemoteToLocalTimeTicks(converter, &load_timing->send_end);
  RemoteToLocalTimeTicks(converter, &load_timing->receive_headers_end);
}

base::TimeTicks ResourceDispatcher::ToRendererCompletionTime(
    const PendingRequestInfo& request_info,
    const base::TimeTicks& browser_completion_time) const {
  if (request_info.completion_time.is_null()) {
    return browser_completion_time;
  }

  // TODO(simonjam): The optimal lower bound should be the most recent value of
  // TimeTicks::Now() returned to WebKit. Is it worth trying to cache that?
  // Until then, |response_start| is used as it is the most recent value
  // returned for this request.
  int64 result = std::max(browser_completion_time.ToInternalValue(),
                          request_info.response_start.ToInternalValue());
  result = std::min(result, request_info.completion_time.ToInternalValue());
  return base::TimeTicks::FromInternalValue(result);
}

base::TimeTicks ResourceDispatcher::ConsumeIOTimestamp() {
  if (io_timestamp_ == base::TimeTicks())
    return base::TimeTicks::Now();
  base::TimeTicks result = io_timestamp_;
  io_timestamp_ = base::TimeTicks();
  return result;
}

// static
bool ResourceDispatcher::IsResourceDispatcherMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ResourceMsg_UploadProgress::ID:
    case ResourceMsg_ReceivedResponse::ID:
    case ResourceMsg_ReceivedCachedMetadata::ID:
    case ResourceMsg_ReceivedRedirect::ID:
    case ResourceMsg_SetDataBuffer::ID:
    case ResourceMsg_DataReceived::ID:
    case ResourceMsg_DataDownloaded::ID:
    case ResourceMsg_RequestComplete::ID:
      return true;

    default:
      break;
  }

  return false;
}

// static
void ResourceDispatcher::ReleaseResourcesInDataMessage(
    const IPC::Message& message) {
  PickleIterator iter(message);
  int request_id;
  if (!message.ReadInt(&iter, &request_id)) {
    NOTREACHED() << "malformed resource message";
    return;
  }

  // If the message contains a shared memory handle, we should close the handle
  // or there will be a memory leak.
  if (message.type() == ResourceMsg_SetDataBuffer::ID) {
    base::SharedMemoryHandle shm_handle;
    if (IPC::ParamTraits<base::SharedMemoryHandle>::Read(&message,
                                                         &iter,
                                                         &shm_handle)) {
      if (base::SharedMemory::IsHandleValid(shm_handle))
        base::SharedMemory::CloseHandle(shm_handle);
    }
  }
}

// static
void ResourceDispatcher::ReleaseResourcesInMessageQueue(MessageQueue* queue) {
  while (!queue->empty()) {
    IPC::Message* message = queue->front();
    ReleaseResourcesInDataMessage(*message);
    queue->pop_front();
    delete message;
  }
}

}  // namespace content
