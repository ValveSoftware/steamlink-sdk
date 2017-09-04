// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See http://dev.chromium.org/developers/design-documents/multi-process-resource-loading

#include "content/child/resource_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/child/request_extra_data.h"
#include "content/child/resource_scheduling_filter.h"
#include "content/child/shared_memory_received_data_factory.h"
#include "content/child/site_isolation_stats_gatherer.h"
#include "content/child/sync_load_response.h"
#include "content/child/url_response_body_consumer.h"
#include "content/common/inter_process_time_ticks_converter.h"
#include "content/common/navigation_params.h"
#include "content/common/resource_messages.h"
#include "content/common/resource_request.h"
#include "content/common/resource_request_completion_status.h"
#include "content/public/child/fixed_received_data.h"
#include "content/public/child/request_peer.h"
#include "content/public/child/resource_dispatcher_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_response.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"

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

class URLLoaderClientImpl final : public mojom::URLLoaderClient {
 public:
  URLLoaderClientImpl(int request_id,
                      ResourceDispatcher* resource_dispatcher,
                      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : binding_(this),
        request_id_(request_id),
        resource_dispatcher_(resource_dispatcher),
        task_runner_(std::move(task_runner)) {}
  ~URLLoaderClientImpl() override {
    if (body_consumer_)
      body_consumer_->Cancel();
  }

  void OnReceiveResponse(const ResourceResponseHead& response_head) override {
    has_received_response_ = true;
    if (body_consumer_)
      body_consumer_->Start(task_runner_.get());
    resource_dispatcher_->OnMessageReceived(
        ResourceMsg_ReceivedResponse(request_id_, response_head));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const ResourceResponseHead& response_head) override {
    DCHECK(!has_received_response_);
    DCHECK(!body_consumer_);
    resource_dispatcher_->OnMessageReceived(ResourceMsg_ReceivedRedirect(
        request_id_, redirect_info, response_head));
  }

  void OnDataDownloaded(int64_t data_len, int64_t encoded_data_len) override {
    resource_dispatcher_->OnMessageReceived(
        ResourceMsg_DataDownloaded(request_id_, data_len, encoded_data_len));
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    DCHECK(!body_consumer_);
    body_consumer_ = new URLResponseBodyConsumer(
        request_id_, resource_dispatcher_, std::move(body), task_runner_);
    if (has_received_response_)
      body_consumer_->Start(task_runner_.get());
  }

  void OnComplete(const ResourceRequestCompletionStatus& status) override {
    if (!body_consumer_) {
      resource_dispatcher_->OnMessageReceived(
          ResourceMsg_RequestComplete(request_id_, status));
      return;
    }
    body_consumer_->OnComplete(status);
  }

  void Bind(mojom::URLLoaderClientAssociatedPtrInfo* client_ptr_info,
            mojo::AssociatedGroup* associated_group) {
    binding_.Bind(client_ptr_info, associated_group);
  }

 private:
  mojo::AssociatedBinding<mojom::URLLoaderClient> binding_;
  scoped_refptr<URLResponseBodyConsumer> body_consumer_;
  const int request_id_;
  bool has_received_response_ = false;
  ResourceDispatcher* const resource_dispatcher_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

void CheckSchemeForReferrerPolicy(const ResourceRequest& request) {
  if ((request.referrer_policy == blink::WebReferrerPolicyDefault ||
       request.referrer_policy ==
           blink::WebReferrerPolicyNoReferrerWhenDowngrade) &&
      request.referrer.SchemeIsCryptographic() &&
      !request.url.SchemeIsCryptographic()) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request.url << "\n"
               << "Referrer = " << request.referrer;
  }
}

}  // namespace

ResourceDispatcher::ResourceDispatcher(
    IPC::Sender* sender,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : message_sender_(sender),
      delegate_(NULL),
      io_timestamp_(base::TimeTicks()),
      main_thread_task_runner_(main_thread_task_runner),
      weak_factory_(this) {
}

ResourceDispatcher::~ResourceDispatcher() {
}

bool ResourceDispatcher::OnMessageReceived(const IPC::Message& message) {
  if (!IsResourceDispatcherMessage(message)) {
    return false;
  }

  int request_id;

  base::PickleIterator iter(message);
  if (!iter.ReadInt(&request_id)) {
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
    request_info->deferred_message_queue.push_back(new IPC::Message(message));
    FlushDeferredMessages(request_id);
    return true;
  }

  DispatchMessage(message);
  return true;
}

ResourceDispatcher::PendingRequestInfo*
ResourceDispatcher::GetPendingRequestInfo(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    // This might happen for kill()ed requests on the webkit end.
    return NULL;
  }
  return it->second.get();
}

void ResourceDispatcher::OnUploadProgress(int request_id,
                                          int64_t position,
                                          int64_t size) {
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
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnReceivedResponse(
        std::move(request_info->peer), response_head.mime_type,
        request_info->url);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  request_info->site_isolation_metadata =
      SiteIsolationStatsGatherer::OnReceivedResponse(
          request_info->frame_origin, request_info->response_url,
          request_info->resource_type, request_info->origin_pid,
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
  request_info->received_data_factory =
      make_scoped_refptr(new SharedMemoryReceivedDataFactory(
          message_sender_, request_id, request_info->buffer));

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

  // TODO(erikchen): Temporary debugging. http://crbug.com/527588.
  CHECK_GE(shm_size, 0);
  CHECK_LE(shm_size, 512 * 1024);
  request_info->buffer_size = shm_size;
}

void ResourceDispatcher::OnReceivedInlinedDataChunk(
    int request_id,
    const std::vector<char>& data,
    int encoded_data_length,
    int encoded_body_length) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedInlinedDataChunk");
  DCHECK(!data.empty());
  DCHECK(base::FeatureList::IsEnabled(
      features::kOptimizeLoadingIPCForSmallResources));

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info || data.empty())
    return;

  // Check whether this response data is compliant with our cross-site
  // document blocking policy. We only do this for the first chunk of data.
  if (request_info->site_isolation_metadata.get()) {
    SiteIsolationStatsGatherer::OnReceivedFirstChunk(
        request_info->site_isolation_metadata, data.data(), data.size());
    request_info->site_isolation_metadata.reset();
  }

  DCHECK(!request_info->buffer.get());

  std::unique_ptr<RequestPeer::ReceivedData> received_data(
      new content::FixedReceivedData(data, encoded_data_length,
                                     encoded_body_length));
  request_info->peer->OnReceivedData(std::move(received_data));
}

void ResourceDispatcher::OnReceivedData(int request_id,
                                        int data_offset,
                                        int data_length,
                                        int encoded_data_length,
                                        int encoded_body_length) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedData");
  DCHECK_GT(data_length, 0);
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  bool send_ack = true;
  if (request_info && data_length > 0) {
    CHECK(base::SharedMemory::IsHandleValid(request_info->buffer->handle()));
    CHECK_GE(request_info->buffer_size, data_offset + data_length);

    const char* data_start = static_cast<char*>(request_info->buffer->memory());
    CHECK(data_start);
    CHECK(data_start + data_offset);
    const char* data_ptr = data_start + data_offset;

    // Check whether this response data is compliant with our cross-site
    // document blocking policy. We only do this for the first chunk of data.
    if (request_info->site_isolation_metadata.get()) {
      SiteIsolationStatsGatherer::OnReceivedFirstChunk(
          request_info->site_isolation_metadata, data_ptr, data_length);
      request_info->site_isolation_metadata.reset();
    }

    std::unique_ptr<RequestPeer::ReceivedData> data =
        request_info->received_data_factory->Create(
            data_offset, data_length, encoded_data_length, encoded_body_length);
    // |data| takes care of ACKing.
    send_ack = false;
    request_info->peer->OnReceivedData(std::move(data));
  }

  // Acknowledge the reception of this data.
  if (send_ack)
    message_sender_->Send(new ResourceHostMsg_DataReceived_ACK(request_id));
}

void ResourceDispatcher::OnDownloadedData(int request_id,
                                          int data_len,
                                          int encoded_data_length) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;

  request_info->peer->OnDownloadedData(data_len, encoded_data_length);
}

void ResourceDispatcher::OnReceivedRedirect(
    int request_id,
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnReceivedRedirect");
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->response_start = ConsumeIOTimestamp();

  ResourceResponseInfo renderer_response_info;
  ToResourceResponseInfo(*request_info, response_head, &renderer_response_info);
  if (request_info->peer->OnReceivedRedirect(redirect_info,
                                             renderer_response_info)) {
    // Double-check if the request is still around. The call above could
    // potentially remove it.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info)
      return;
    // We update the response_url here so that we can send it to
    // SiteIsolationStatsGatherer later when OnReceivedResponse is called.
    request_info->response_url = redirect_info.new_url;
    request_info->pending_redirect_message.reset(
        new ResourceHostMsg_FollowRedirect(request_id));
    if (!request_info->is_deferred) {
      FollowPendingRedirect(request_id, request_info);
    }
  } else {
    Cancel(request_id);
  }
}

void ResourceDispatcher::FollowPendingRedirect(
    int request_id,
    PendingRequestInfo* request_info) {
  IPC::Message* msg = request_info->pending_redirect_message.release();
  if (msg) {
    if (request_info->url_loader) {
      request_info->url_loader->FollowRedirect();
      delete msg;
    } else {
      message_sender_->Send(msg);
    }
  }
}

void ResourceDispatcher::OnRequestComplete(
    int request_id,
    const ResourceRequestCompletionStatus& request_complete_data) {
  TRACE_EVENT0("loader", "ResourceDispatcher::OnRequestComplete");

  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info)
    return;
  request_info->completion_time = ConsumeIOTimestamp();
  request_info->buffer.reset();
  if (request_info->received_data_factory)
    request_info->received_data_factory->Stop();
  request_info->received_data_factory = nullptr;
  request_info->buffer_size = 0;

  RequestPeer* peer = request_info->peer.get();

  if (delegate_) {
    std::unique_ptr<RequestPeer> new_peer = delegate_->OnRequestComplete(
        std::move(request_info->peer), request_info->resource_type,
        request_complete_data.error_code);
    DCHECK(new_peer);
    request_info->peer = std::move(new_peer);
  }

  base::TimeTicks renderer_completion_time = ToRendererCompletionTime(
      *request_info, request_complete_data.completion_time);

  // The request ID will be removed from our pending list in the destructor.
  // Normally, dispatching this message causes the reference-counted request to
  // die immediately.
  // TODO(kinuko): Revisit here. This probably needs to call request_info->peer
  // but the past attempt to change it seems to have caused crashes.
  // (crbug.com/547047)
  peer->OnCompletedRequest(request_complete_data.error_code,
                           request_complete_data.was_ignored_by_handler,
                           request_complete_data.exists_in_cache,
                           renderer_completion_time,
                           request_complete_data.encoded_data_length);
}

bool ResourceDispatcher::RemovePendingRequest(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())
    return false;

  PendingRequestInfo* request_info = it->second.get();

  bool release_downloaded_file = request_info->download_to_file;

  ReleaseResourcesInMessageQueue(&request_info->deferred_message_queue);

  // Clear URLLoaderClient to stop receiving further Mojo IPC from the browser
  // process.
  it->second->url_loader_client = nullptr;

  // Always delete the pending_request asyncly so that cancelling the request
  // doesn't delete the request context info while its response is still being
  // handled.
  main_thread_task_runner_->DeleteSoon(FROM_HERE, it->second.release());
  pending_requests_.erase(it);

  if (release_downloaded_file) {
    message_sender_->Send(
        new ResourceHostMsg_ReleaseDownloadedFile(request_id));
  }

  if (resource_scheduling_filter_.get())
    resource_scheduling_filter_->ClearRequestIdTaskRunner(request_id);

  return true;
}

void ResourceDispatcher::Cancel(int request_id) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DVLOG(1) << "unknown request";
    return;
  }

  // |completion_time.is_null()| is a proxy for OnRequestComplete never being
  // called.
  // TODO(csharrison): Remove this code when crbug.com/557430 is resolved.
  // Sample this enough that this won't dump much more than a hundred times a
  // day even without the static guard. The guard ensures this dumps much less
  // frequently, because these aborts frequently come in quick succession.
  const PendingRequestInfo& info = *it->second;
  int64_t request_time =
      (base::TimeTicks::Now() - info.request_start).InMilliseconds();
  if (info.resource_type == ResourceType::RESOURCE_TYPE_MAIN_FRAME &&
      info.completion_time.is_null() && request_time < 100 &&
      base::RandDouble() < .000001) {
    static bool should_dump = true;
    if (should_dump) {
      char url_copy[256] = {0};
      strncpy(url_copy, info.response_url.spec().c_str(),
              sizeof(url_copy));
      base::debug::Alias(&url_copy);
      base::debug::Alias(&request_time);
      base::debug::DumpWithoutCrashing();
      should_dump = false;
    }
  }
  // Cancel the request if it didn't complete, and clean it up so the bridge
  // will receive no more messages.
  if (info.completion_time.is_null())
    message_sender_->Send(new ResourceHostMsg_CancelRequest(request_id));
  RemovePendingRequest(request_id);
}

void ResourceDispatcher::SetDefersLoading(int request_id, bool value) {
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end()) {
    DLOG(ERROR) << "unknown request";
    return;
  }
  PendingRequestInfo* request_info = it->second.get();
  if (value) {
    request_info->is_deferred = value;
  } else if (request_info->is_deferred) {
    request_info->is_deferred = false;

    FollowPendingRedirect(request_id, request_info);

    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ResourceDispatcher::FlushDeferredMessages,
                              weak_factory_.GetWeakPtr(), request_id));
  }
}

void ResourceDispatcher::DidChangePriority(int request_id,
                                           net::RequestPriority new_priority,
                                           int intra_priority_value) {
  DCHECK(base::ContainsKey(pending_requests_, request_id));
  message_sender_->Send(new ResourceHostMsg_DidChangePriority(
      request_id, new_priority, intra_priority_value));
}

ResourceDispatcher::PendingRequestInfo::PendingRequestInfo(
    std::unique_ptr<RequestPeer> peer,
    ResourceType resource_type,
    int origin_pid,
    const url::Origin& frame_origin,
    const GURL& request_url,
    bool download_to_file)
    : peer(std::move(peer)),
      resource_type(resource_type),
      origin_pid(origin_pid),
      url(request_url),
      frame_origin(frame_origin),
      response_url(request_url),
      download_to_file(download_to_file),
      request_start(base::TimeTicks::Now()) {}

ResourceDispatcher::PendingRequestInfo::~PendingRequestInfo() {
}

void ResourceDispatcher::DispatchMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(ResourceDispatcher, message)
    IPC_MESSAGE_HANDLER(ResourceMsg_UploadProgress, OnUploadProgress)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedResponse, OnReceivedResponse)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedCachedMetadata,
                        OnReceivedCachedMetadata)
    IPC_MESSAGE_HANDLER(ResourceMsg_ReceivedRedirect, OnReceivedRedirect)
    IPC_MESSAGE_HANDLER(ResourceMsg_SetDataBuffer, OnSetDataBuffer)
    IPC_MESSAGE_HANDLER(ResourceMsg_InlinedDataChunkReceived,
                        OnReceivedInlinedDataChunk)
    IPC_MESSAGE_HANDLER(ResourceMsg_DataReceived, OnReceivedData)
    IPC_MESSAGE_HANDLER(ResourceMsg_DataDownloaded, OnDownloadedData)
    IPC_MESSAGE_HANDLER(ResourceMsg_RequestComplete, OnRequestComplete)
  IPC_END_MESSAGE_MAP()
}

void ResourceDispatcher::FlushDeferredMessages(int request_id) {
  PendingRequestInfo* request_info = GetPendingRequestInfo(request_id);
  if (!request_info || request_info->is_deferred)
    return;
  // Because message handlers could result in request_info being destroyed,
  // we need to work with a stack reference to the deferred queue.
  MessageQueue q;
  q.swap(request_info->deferred_message_queue);
  while (!q.empty()) {
    IPC::Message* m = q.front();
    q.pop_front();
    DispatchMessage(*m);
    delete m;
    // We need to find the request again in the list as it may have completed
    // by now and the request_info instance above may be invalid.
    request_info = GetPendingRequestInfo(request_id);
    if (!request_info) {
      // The recipient is gone, the messages won't be handled and
      // resources they might hold won't be released. Explicitly release
      // them from here so that they won't leak.
      ReleaseResourcesInMessageQueue(&q);
      return;
    }
    // If this request is deferred in the context of the above message, then
    // we should honor the same and stop dispatching further messages.
    if (request_info->is_deferred) {
      request_info->deferred_message_queue.swap(q);
      return;
    }
  }
}

void ResourceDispatcher::StartSync(
    std::unique_ptr<ResourceRequest> request,
    int routing_id,
    SyncLoadResponse* response,
    blink::WebURLRequest::LoadingIPCType ipc_type,
    mojom::URLLoaderFactory* url_loader_factory) {
  CheckSchemeForReferrerPolicy(*request);

  SyncLoadResult result;

  if (ipc_type == blink::WebURLRequest::LoadingIPCType::Mojo) {
    if (!url_loader_factory->SyncLoad(
            routing_id, MakeRequestID(), *request, &result)) {
      response->error_code = net::ERR_FAILED;
      return;
    }
  } else {
    IPC::SyncMessage* msg = new ResourceHostMsg_SyncLoad(
        routing_id, MakeRequestID(), *request, &result);

    // NOTE: This may pump events (see RenderThread::Send).
    if (!message_sender_->Send(msg)) {
      response->error_code = net::ERR_FAILED;
      return;
    }
  }

  response->error_code = result.error_code;
  response->url = result.final_url;
  response->headers = result.headers;
  response->mime_type = result.mime_type;
  response->charset = result.charset;
  response->request_time = result.request_time;
  response->response_time = result.response_time;
  response->load_timing = result.load_timing;
  response->devtools_info = result.devtools_info;
  response->data.swap(result.data);
  response->download_file_path = result.download_file_path;
  response->socket_address = result.socket_address;
  response->encoded_data_length = result.encoded_data_length;
  response->encoded_body_length = result.encoded_body_length;
}

int ResourceDispatcher::StartAsync(
    std::unique_ptr<ResourceRequest> request,
    int routing_id,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
    const url::Origin& frame_origin,
    std::unique_ptr<RequestPeer> peer,
    blink::WebURLRequest::LoadingIPCType ipc_type,
    mojom::URLLoaderFactory* url_loader_factory,
    mojo::AssociatedGroup* associated_group) {
  CheckSchemeForReferrerPolicy(*request);

  // Compute a unique request_id for this renderer process.
  int request_id = MakeRequestID();
  pending_requests_[request_id] = base::MakeUnique<PendingRequestInfo>(
      std::move(peer), request->resource_type, request->origin_pid,
      frame_origin, request->url, request->download_to_file);

  if (resource_scheduling_filter_.get() && loading_task_runner) {
    resource_scheduling_filter_->SetRequestIdTaskRunner(request_id,
                                                        loading_task_runner);
  }

  if (ipc_type == blink::WebURLRequest::LoadingIPCType::Mojo) {
    std::unique_ptr<URLLoaderClientImpl> client(
        new URLLoaderClientImpl(request_id, this, main_thread_task_runner_));
    mojom::URLLoaderAssociatedPtr url_loader;
    mojom::URLLoaderClientAssociatedPtrInfo client_ptr_info;
    client->Bind(&client_ptr_info, associated_group);
    url_loader_factory->CreateLoaderAndStart(
        GetProxy(&url_loader, associated_group), routing_id, request_id,
        *request, std::move(client_ptr_info));
    pending_requests_[request_id]->url_loader = std::move(url_loader);
    pending_requests_[request_id]->url_loader_client = std::move(client);
  } else {
    message_sender_->Send(
        new ResourceHostMsg_RequestResource(routing_id, request_id, *request));
  }

  return request_id;
}

void ResourceDispatcher::ToResourceResponseInfo(
    const PendingRequestInfo& request_info,
    const ResourceResponseHead& browser_info,
    ResourceResponseInfo* renderer_info) const {
  *renderer_info = browser_info;
  if (base::TimeTicks::IsConsistentAcrossProcesses() ||
      request_info.request_start.is_null() ||
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
  RemoteToLocalTimeTicks(converter, &load_timing->push_start);
  RemoteToLocalTimeTicks(converter, &load_timing->push_end);
  RemoteToLocalTimeTicks(converter, &renderer_info->service_worker_start_time);
  RemoteToLocalTimeTicks(converter, &renderer_info->service_worker_ready_time);

  // Collect UMA on the inter-process skew.
  bool is_skew_additive = false;
  if (converter.IsSkewAdditiveForMetrics()) {
    is_skew_additive = true;
    base::TimeDelta skew = converter.GetSkewForMetrics();
    if (skew >= base::TimeDelta()) {
      UMA_HISTOGRAM_TIMES(
          "InterProcessTimeTicks.BrowserAhead_BrowserToRenderer", skew);
    } else {
      UMA_HISTOGRAM_TIMES(
          "InterProcessTimeTicks.BrowserBehind_BrowserToRenderer", -skew);
    }
  }
  UMA_HISTOGRAM_BOOLEAN(
      "InterProcessTimeTicks.IsSkewAdditive_BrowserToRenderer",
      is_skew_additive);
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
  int64_t result = std::max(browser_completion_time.ToInternalValue(),
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
    case ResourceMsg_InlinedDataChunkReceived::ID:
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
  base::PickleIterator iter(message);
  int request_id;
  if (!iter.ReadInt(&request_id)) {
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

void ResourceDispatcher::SetResourceSchedulingFilter(
    scoped_refptr<ResourceSchedulingFilter> resource_scheduling_filter) {
  resource_scheduling_filter_ = resource_scheduling_filter;
}

}  // namespace content
