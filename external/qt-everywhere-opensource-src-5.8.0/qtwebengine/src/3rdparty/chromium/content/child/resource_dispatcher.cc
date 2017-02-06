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
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/child/request_extra_data.h"
#include "content/child/request_info.h"
#include "content/child/resource_scheduling_filter.h"
#include "content/child/shared_memory_received_data_factory.h"
#include "content/child/site_isolation_stats_gatherer.h"
#include "content/child/sync_load_response.h"
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
    FlushDeferredMessages(request_id);
    request_info = GetPendingRequestInfo(request_id);
    DCHECK(request_info);
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
    int encoded_data_length) {
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
      new content::FixedReceivedData(data, encoded_data_length));
  request_info->peer->OnReceivedData(std::move(received_data));
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
        request_info->received_data_factory->Create(data_offset, data_length,
                                                    encoded_data_length);
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
  // Acknowledge the reception of this message.
  message_sender_->Send(new ResourceHostMsg_DataDownloaded_ACK(request_id));

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
  if (msg)
    message_sender_->Send(msg);
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
                           request_complete_data.security_info,
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
  // Cancel the request, and clean it up so the bridge will receive no more
  // messages.
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
  DCHECK(ContainsKey(pending_requests_, request_id));
  message_sender_->Send(new ResourceHostMsg_DidChangePriority(
      request_id, new_priority, intra_priority_value));
}

ResourceDispatcher::PendingRequestInfo::PendingRequestInfo(
    std::unique_ptr<RequestPeer> peer,
    ResourceType resource_type,
    int origin_pid,
    const GURL& frame_origin,
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
  PendingRequestMap::iterator it = pending_requests_.find(request_id);
  if (it == pending_requests_.end())  // The request could have become invalid.
    return;
  PendingRequestInfo* request_info = it->second.get();
  if (request_info->is_deferred)
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
    // If this request is deferred in the context of the above message, then
    // we should honor the same and stop dispatching further messages.
    // We need to find the request again in the list as it may have completed
    // by now and the request_info instance above may be invalid.
    PendingRequestMap::iterator index = pending_requests_.find(request_id);
    if (index != pending_requests_.end()) {
      PendingRequestInfo* pending_request = index->second.get();
      if (pending_request->is_deferred) {
        pending_request->deferred_message_queue.swap(q);
        return;
      }
    }
  }
}

void ResourceDispatcher::StartSync(const RequestInfo& request_info,
                                   ResourceRequestBodyImpl* request_body,
                                   SyncLoadResponse* response) {
  std::unique_ptr<ResourceRequest> request =
      CreateRequest(request_info, request_body, NULL);

  SyncLoadResult result;
  IPC::SyncMessage* msg = new ResourceHostMsg_SyncLoad(
      request_info.routing_id, MakeRequestID(), *request, &result);

  // NOTE: This may pump events (see RenderThread::Send).
  if (!message_sender_->Send(msg)) {
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
  response->socket_address = result.socket_address;
}

int ResourceDispatcher::StartAsync(const RequestInfo& request_info,
                                   ResourceRequestBodyImpl* request_body,
                                   std::unique_ptr<RequestPeer> peer) {
  GURL frame_origin;
  std::unique_ptr<ResourceRequest> request =
      CreateRequest(request_info, request_body, &frame_origin);

  // Compute a unique request_id for this renderer process.
  int request_id = MakeRequestID();
  pending_requests_[request_id] = base::WrapUnique(new PendingRequestInfo(
      std::move(peer), request->resource_type, request->origin_pid,
      frame_origin, request->url, request_info.download_to_file));

  if (resource_scheduling_filter_.get() &&
      request_info.loading_web_task_runner) {
    resource_scheduling_filter_->SetRequestIdTaskRunner(
        request_id,
        base::WrapUnique(request_info.loading_web_task_runner->clone()));
  }

  message_sender_->Send(new ResourceHostMsg_RequestResource(
      request_info.routing_id, request_id, *request));

  return request_id;
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

std::unique_ptr<ResourceRequest> ResourceDispatcher::CreateRequest(
    const RequestInfo& request_info,
    ResourceRequestBodyImpl* request_body,
    GURL* frame_origin) {
  std::unique_ptr<ResourceRequest> request(new ResourceRequest);
  request->method = request_info.method;
  request->url = request_info.url;
  request->first_party_for_cookies = request_info.first_party_for_cookies;
  request->request_initiator = request_info.request_initiator;
  request->referrer = request_info.referrer.url;
  request->referrer_policy = request_info.referrer.policy;
  request->headers = request_info.headers;
  request->load_flags = request_info.load_flags;
  request->origin_pid = request_info.requestor_pid;
  request->resource_type = request_info.request_type;
  request->priority = request_info.priority;
  request->request_context = request_info.request_context;
  request->appcache_host_id = request_info.appcache_host_id;
  request->download_to_file = request_info.download_to_file;
  request->has_user_gesture = request_info.has_user_gesture;
  request->skip_service_worker = request_info.skip_service_worker;
  request->should_reset_appcache = request_info.should_reset_appcache;
  request->fetch_request_mode = request_info.fetch_request_mode;
  request->fetch_credentials_mode = request_info.fetch_credentials_mode;
  request->fetch_redirect_mode = request_info.fetch_redirect_mode;
  request->fetch_request_context_type = request_info.fetch_request_context_type;
  request->fetch_frame_type = request_info.fetch_frame_type;
  request->enable_load_timing = request_info.enable_load_timing;
  request->enable_upload_progress = request_info.enable_upload_progress;
  request->do_not_prompt_for_login = request_info.do_not_prompt_for_login;
  request->report_raw_headers = request_info.report_raw_headers;
  request->lofi_state = request_info.lofi_state;

  if ((request_info.referrer.policy == blink::WebReferrerPolicyDefault ||
       request_info.referrer.policy ==
           blink::WebReferrerPolicyNoReferrerWhenDowngrade) &&
      request_info.referrer.url.SchemeIsCryptographic() &&
      !request_info.url.SchemeIsCryptographic()) {
    LOG(FATAL) << "Trying to send secure referrer for insecure request "
               << "without an appropriate referrer policy.\n"
               << "URL = " << request_info.url << "\n"
               << "Referrer = " << request_info.referrer.url;
  }

  const RequestExtraData kEmptyData;
  const RequestExtraData* extra_data;
  if (request_info.extra_data)
    extra_data = static_cast<RequestExtraData*>(request_info.extra_data);
  else
    extra_data = &kEmptyData;
  request->visiblity_state = extra_data->visibility_state();
  request->render_frame_id = extra_data->render_frame_id();
  request->is_main_frame = extra_data->is_main_frame();
  request->parent_is_main_frame = extra_data->parent_is_main_frame();
  request->parent_render_frame_id = extra_data->parent_render_frame_id();
  request->allow_download = extra_data->allow_download();
  request->transition_type = extra_data->transition_type();
  request->should_replace_current_entry =
      extra_data->should_replace_current_entry();
  request->transferred_request_child_id =
      extra_data->transferred_request_child_id();
  request->transferred_request_request_id =
      extra_data->transferred_request_request_id();
  request->service_worker_provider_id =
      extra_data->service_worker_provider_id();
  request->originated_from_service_worker =
      extra_data->originated_from_service_worker();
  request->request_body = request_body;
  request->resource_body_stream_url = request_info.resource_body_stream_url;
  request->initiated_in_secure_context =
      extra_data->initiated_in_secure_context();
  if (frame_origin)
    *frame_origin = extra_data->frame_origin();
  return request;
}

void ResourceDispatcher::SetResourceSchedulingFilter(
    scoped_refptr<ResourceSchedulingFilter> resource_scheduling_filter) {
  resource_scheduling_filter_ = resource_scheduling_filter;
}

}  // namespace content
