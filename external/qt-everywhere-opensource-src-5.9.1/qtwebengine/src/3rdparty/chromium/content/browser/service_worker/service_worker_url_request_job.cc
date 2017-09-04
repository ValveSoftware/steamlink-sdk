// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_url_request_job.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_blob_reader.h"
#include "content/browser/service_worker/service_worker_fetch_dispatcher.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_response_info.h"
#include "content/browser/service_worker/service_worker_stream_reader.h"
#include "content/common/resource_request_body_impl.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/common/referrer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

net::NetLogEventType RequestJobResultToNetEventType(
    ServiceWorkerMetrics::URLRequestJobResult result) {
  using n = net::NetLogEventType;
  using m = ServiceWorkerMetrics;
  switch (result) {
    case m::REQUEST_JOB_FALLBACK_RESPONSE:
      return n::SERVICE_WORKER_FALLBACK_RESPONSE;
    case m::REQUEST_JOB_FALLBACK_FOR_CORS:
      return n::SERVICE_WORKER_FALLBACK_FOR_CORS;
    case m::REQUEST_JOB_HEADERS_ONLY_RESPONSE:
      return n::SERVICE_WORKER_HEADERS_ONLY_RESPONSE;
    case m::REQUEST_JOB_STREAM_RESPONSE:
      return n::SERVICE_WORKER_STREAM_RESPONSE;
    case m::REQUEST_JOB_BLOB_RESPONSE:
      return n::SERVICE_WORKER_BLOB_RESPONSE;
    case m::REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO:
      return n::SERVICE_WORKER_ERROR_RESPONSE_STATUS_ZERO;
    case m::REQUEST_JOB_ERROR_BAD_BLOB:
      return n::SERVICE_WORKER_ERROR_BAD_BLOB;
    case m::REQUEST_JOB_ERROR_NO_PROVIDER_HOST:
      return n::SERVICE_WORKER_ERROR_NO_PROVIDER_HOST;
    case m::REQUEST_JOB_ERROR_NO_ACTIVE_VERSION:
      return n::SERVICE_WORKER_ERROR_NO_ACTIVE_VERSION;
    case m::REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH:
      return n::SERVICE_WORKER_ERROR_FETCH_EVENT_DISPATCH;
    case m::REQUEST_JOB_ERROR_BLOB_READ:
      return n::SERVICE_WORKER_ERROR_BLOB_READ;
    case m::REQUEST_JOB_ERROR_STREAM_ABORTED:
      return n::SERVICE_WORKER_ERROR_STREAM_ABORTED;
    case m::REQUEST_JOB_ERROR_KILLED:
      return n::SERVICE_WORKER_ERROR_KILLED;
    case m::REQUEST_JOB_ERROR_KILLED_WITH_BLOB:
      return n::SERVICE_WORKER_ERROR_KILLED_WITH_BLOB;
    case m::REQUEST_JOB_ERROR_KILLED_WITH_STREAM:
      return n::SERVICE_WORKER_ERROR_KILLED_WITH_STREAM;
    case m::REQUEST_JOB_ERROR_BAD_DELEGATE:
      return n::SERVICE_WORKER_ERROR_BAD_DELEGATE;
    case m::REQUEST_JOB_ERROR_REQUEST_BODY_BLOB_FAILED:
      return n::SERVICE_WORKER_ERROR_REQUEST_BODY_BLOB_FAILED;
    // We can't log if there's no request; fallthrough.
    case m::REQUEST_JOB_ERROR_NO_REQUEST:
    // Obsolete types; fallthrough.
    case m::REQUEST_JOB_ERROR_DESTROYED:
    case m::REQUEST_JOB_ERROR_DESTROYED_WITH_BLOB:
    case m::REQUEST_JOB_ERROR_DESTROYED_WITH_STREAM:
    // Invalid type.
    case m::NUM_REQUEST_JOB_RESULT_TYPES:
      NOTREACHED() << result;
  }
  NOTREACHED() << result;
  return n::FAILED;
}

std::vector<int64_t> GetFileSizesOnBlockingPool(
    std::vector<base::FilePath> file_paths) {
  std::vector<int64_t> sizes;
  sizes.reserve(file_paths.size());
  for (const base::FilePath& path : file_paths) {
    base::File::Info file_info;
    if (!base::GetFileInfo(path, &file_info) || file_info.is_directory)
      return std::vector<int64_t>();
    sizes.push_back(file_info.size);
  }
  return sizes;
}

}  // namespace

// Sets the size on each DataElement in the request body that is a file with
// unknown size. This ensures ServiceWorkerURLRequestJob::CreateRequestBodyBlob
// can successfuly create a blob from the data elements, as files with unknown
// sizes are not supported by the blob storage system.
class ServiceWorkerURLRequestJob::FileSizeResolver {
 public:
  explicit FileSizeResolver(ServiceWorkerURLRequestJob* owner)
      : owner_(owner), weak_factory_(this) {
    TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker", "FileSizeResolver", this, "URL",
                             owner_->request()->url().spec());
    owner_->request()->net_log().BeginEvent(
        net::NetLogEventType::SERVICE_WORKER_WAITING_FOR_REQUEST_BODY_FILES);
  }

  ~FileSizeResolver() {
    owner_->request()->net_log().EndEvent(
        net::NetLogEventType::SERVICE_WORKER_WAITING_FOR_REQUEST_BODY_FILES,
        net::NetLog::BoolCallback("success", phase_ == Phase::SUCCESS));
    TRACE_EVENT_ASYNC_END1("ServiceWorker", "FileSizeResolver", this, "Success",
                           phase_ == Phase::SUCCESS);
  }

  void Resolve(base::TaskRunner* file_runner,
               const base::Callback<void(bool)>& callback) {
    DCHECK_EQ(static_cast<int>(Phase::INITIAL), static_cast<int>(phase_));
    DCHECK(file_elements_.empty());
    phase_ = Phase::WAITING;
    body_ = owner_->body_;
    callback_ = callback;

    std::vector<base::FilePath> file_paths;
    for (ResourceRequestBodyImpl::Element& element :
         *body_->elements_mutable()) {
      if (element.type() == ResourceRequestBodyImpl::Element::TYPE_FILE &&
          element.length() == ResourceRequestBodyImpl::Element::kUnknownSize) {
        file_elements_.push_back(&element);
        file_paths.push_back(element.path());
      }
    }
    if (file_elements_.empty()) {
      Complete(true);
      return;
    }

    PostTaskAndReplyWithResult(
        file_runner, FROM_HERE,
        base::Bind(&GetFileSizesOnBlockingPool, base::Passed(&file_paths)),
        base::Bind(
            &ServiceWorkerURLRequestJob::FileSizeResolver::OnFileSizesResolved,
            weak_factory_.GetWeakPtr()));
  }

 private:
  enum class Phase { INITIAL, WAITING, SUCCESS, FAIL };

  void OnFileSizesResolved(std::vector<int64_t> sizes) {
    bool success = !sizes.empty();
    if (success) {
      DCHECK_EQ(sizes.size(), file_elements_.size());
      size_t num_elements = file_elements_.size();
      for (size_t i = 0; i < num_elements; i++) {
        ResourceRequestBodyImpl::Element* element = file_elements_[i];
        element->SetToFilePathRange(element->path(), element->offset(),
                                    base::checked_cast<uint64_t>(sizes[i]),
                                    element->expected_modification_time());
      }
      file_elements_.clear();
    }
    Complete(success);
  }

  void Complete(bool success) {
    DCHECK_EQ(static_cast<int>(Phase::WAITING), static_cast<int>(phase_));
    phase_ = success ? Phase::SUCCESS : Phase::FAIL;
    // Destroys |this|, so we use a copy.
    base::ResetAndReturn(&callback_).Run(success);
  }

  // Owns and must outlive |this|.
  ServiceWorkerURLRequestJob* owner_;

  scoped_refptr<ResourceRequestBodyImpl> body_;
  std::vector<ResourceRequestBodyImpl::Element*> file_elements_;
  base::Callback<void(bool)> callback_;
  Phase phase_ = Phase::INITIAL;
  base::WeakPtrFactory<FileSizeResolver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSizeResolver);
};

bool ServiceWorkerURLRequestJob::Delegate::RequestStillValid(
    ServiceWorkerMetrics::URLRequestJobResult* result) {
  return true;
}

ServiceWorkerURLRequestJob::ServiceWorkerURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& client_id,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
    const ResourceContext* resource_context,
    FetchRequestMode request_mode,
    FetchCredentialsMode credentials_mode,
    FetchRedirectMode redirect_mode,
    ResourceType resource_type,
    RequestContextType request_context_type,
    RequestContextFrameType frame_type,
    scoped_refptr<ResourceRequestBodyImpl> body,
    ServiceWorkerFetchType fetch_type,
    Delegate* delegate)
    : net::URLRequestJob(request, network_delegate),
      delegate_(delegate),
      response_type_(NOT_DETERMINED),
      is_started_(false),
      service_worker_response_type_(blink::WebServiceWorkerResponseTypeDefault),
      client_id_(client_id),
      blob_storage_context_(blob_storage_context),
      resource_context_(resource_context),
      request_mode_(request_mode),
      credentials_mode_(credentials_mode),
      redirect_mode_(redirect_mode),
      resource_type_(resource_type),
      request_context_type_(request_context_type),
      frame_type_(frame_type),
      fall_back_required_(false),
      body_(body),
      fetch_type_(fetch_type),
      weak_factory_(this) {
  DCHECK(delegate_) << "ServiceWorkerURLRequestJob requires a delegate";
}

ServiceWorkerURLRequestJob::~ServiceWorkerURLRequestJob() {
  stream_reader_.reset();
  file_size_resolver_.reset();

  if (!ShouldRecordResult())
    return;
  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED;
  if (response_body_type_ == STREAM)
    result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED_WITH_STREAM;
  else if (response_body_type_ == BLOB)
    result = ServiceWorkerMetrics::REQUEST_JOB_ERROR_KILLED_WITH_BLOB;
  RecordResult(result);
}

void ServiceWorkerURLRequestJob::FallbackToNetwork() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  DCHECK(!IsFallbackToRendererNeeded());
  response_type_ = FALLBACK_TO_NETWORK;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::FallbackToNetworkOrRenderer() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  DCHECK_NE(ServiceWorkerFetchType::FOREIGN_FETCH, fetch_type_);
  if (IsFallbackToRendererNeeded()) {
    response_type_ = FALLBACK_TO_RENDERER;
  } else {
    response_type_ = FALLBACK_TO_NETWORK;
  }
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::ForwardToServiceWorker() {
  DCHECK_EQ(NOT_DETERMINED, response_type_);
  response_type_ = FORWARD_TO_SERVICE_WORKER;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Start() {
  is_started_ = true;
  MaybeStartRequest();
}

void ServiceWorkerURLRequestJob::Kill() {
  net::URLRequestJob::Kill();
  stream_reader_.reset();
  fetch_dispatcher_.reset();
  blob_reader_.reset();
  weak_factory_.InvalidateWeakPtrs();
}

net::LoadState ServiceWorkerURLRequestJob::GetLoadState() const {
  // TODO(kinuko): refine this for better debug.
  return net::URLRequestJob::GetLoadState();
}

bool ServiceWorkerURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  const base::Time request_time = info->request_time;
  *info = *http_info();
  info->request_time = request_time;
  info->response_time = response_time_;
}

void ServiceWorkerURLRequestJob::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  *load_timing_info = load_timing_info_;
}

int ServiceWorkerURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

void ServiceWorkerURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header) ||
      !net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
    return;
  }

  // We don't support multiple range requests in one single URL request.
  if (ranges.size() == 1U)
    byte_range_ = ranges[0];
}

int ServiceWorkerURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(buf);
  DCHECK_GE(buf_size, 0);

  if (stream_reader_)
    return stream_reader_->ReadRawData(buf, buf_size);
  if (blob_reader_)
    return blob_reader_->ReadRawData(buf, buf_size);

  return 0;
}

void ServiceWorkerURLRequestJob::OnResponseStarted() {
  if (response_time_.is_null())
    response_time_ = base::Time::Now();
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::OnReadRawDataComplete(int bytes_read) {
  ReadRawDataComplete(bytes_read);
}

void ServiceWorkerURLRequestJob::RecordResult(
    ServiceWorkerMetrics::URLRequestJobResult result) {
  // It violates style guidelines to handle a NOTREACHED() failure but if there
  // is a bug don't let it corrupt UMA results by double-counting.
  if (!ShouldRecordResult()) {
    NOTREACHED();
    return;
  }
  did_record_result_ = true;
  ServiceWorkerMetrics::RecordURLRequestJobResult(IsMainResourceLoad(), result);
  request()->net_log().AddEvent(RequestJobResultToNetEventType(result));
}

base::WeakPtr<ServiceWorkerURLRequestJob>
ServiceWorkerURLRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

const net::HttpResponseInfo* ServiceWorkerURLRequestJob::http_info() const {
  if (!http_response_info_)
    return nullptr;
  if (range_response_info_)
    return range_response_info_.get();
  return http_response_info_.get();
}

void ServiceWorkerURLRequestJob::MaybeStartRequest() {
  if (is_started_ && response_type_ != NOT_DETERMINED) {
    // Start asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ServiceWorkerURLRequestJob::StartRequest,
                              weak_factory_.GetWeakPtr()));
  }
}

void ServiceWorkerURLRequestJob::StartRequest() {
  request()->net_log().AddEvent(
      net::NetLogEventType::SERVICE_WORKER_START_REQUEST);

  switch (response_type_) {
    case NOT_DETERMINED:
      NOTREACHED();
      return;

    case FALLBACK_TO_NETWORK:
      FinalizeFallbackToNetwork();
      return;

    case FALLBACK_TO_RENDERER:
      FinalizeFallbackToRenderer();
      return;

    case FORWARD_TO_SERVICE_WORKER:
      if (HasRequestBody()) {
        DCHECK(!file_size_resolver_);
        file_size_resolver_.reset(new FileSizeResolver(this));
        file_size_resolver_->Resolve(
            BrowserThread::GetBlockingPool(),
            base::Bind(
                &ServiceWorkerURLRequestJob::RequestBodyFileSizesResolved,
                GetWeakPtr()));
        return;
      }

      RequestBodyFileSizesResolved(true);
      return;
  }

  NOTREACHED();
}

std::unique_ptr<ServiceWorkerFetchRequest>
ServiceWorkerURLRequestJob::CreateFetchRequest() {
  std::string blob_uuid;
  uint64_t blob_size = 0;
  if (HasRequestBody())
    CreateRequestBodyBlob(&blob_uuid, &blob_size);
  std::unique_ptr<ServiceWorkerFetchRequest> request(
      new ServiceWorkerFetchRequest());
  request->mode = request_mode_;
  request->is_main_resource_load = IsMainResourceLoad();
  request->request_context_type = request_context_type_;
  request->frame_type = frame_type_;
  request->url = request_->url();
  request->method = request_->method();
  const net::HttpRequestHeaders& headers = request_->extra_request_headers();
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    if (ServiceWorkerContext::IsExcludedHeaderNameForFetchEvent(it.name()))
      continue;
    request->headers[it.name()] = it.value();
  }
  request->blob_uuid = blob_uuid;
  request->blob_size = blob_size;
  request->credentials_mode = credentials_mode_;
  request->redirect_mode = redirect_mode_;
  request->client_id = client_id_;
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request_);
  if (info) {
    request->is_reload = ui::PageTransitionCoreTypeIs(
        info->GetPageTransition(), ui::PAGE_TRANSITION_RELOAD);
    request->referrer =
        Referrer(GURL(request_->referrer()), info->GetReferrerPolicy());
  } else {
    CHECK(
        request_->referrer_policy() ==
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
    request->referrer =
        Referrer(GURL(request_->referrer()), blink::WebReferrerPolicyDefault);
  }
  request->fetch_type = fetch_type_;
  return request;
}

void ServiceWorkerURLRequestJob::CreateRequestBodyBlob(std::string* blob_uuid,
                                                       uint64_t* blob_size) {
  DCHECK(HasRequestBody());
  storage::BlobDataBuilder blob_builder(base::GenerateGUID());
  for (const ResourceRequestBodyImpl::Element& element : (*body_->elements())) {
    blob_builder.AppendIPCDataElement(element);
  }

  request_body_blob_data_handle_ =
      blob_storage_context_->AddFinishedBlob(&blob_builder);
  *blob_uuid = blob_builder.uuid();
  *blob_size = request_body_blob_data_handle_->size();
}

void ServiceWorkerURLRequestJob::DidPrepareFetchEvent(
    scoped_refptr<ServiceWorkerVersion> version) {
  worker_ready_time_ = base::TimeTicks::Now();
  load_timing_info_.send_start = worker_ready_time_;

  // Record the time taken for the browser to find and possibly start an active
  // worker to which to dispatch a FetchEvent for a main frame resource request.
  // For context, a FetchEvent can only be dispatched to an ACTIVATED worker
  // that is running (it has been successfully started). The measurements starts
  // when the browser process receives the request. The browser then finds the
  // worker appropriate for this request (if there is none, this metric is not
  // recorded). If that worker is already started, the browser process can send
  // the request to it, so the measurement ends quickly. Otherwise the browser
  // process has to start the worker and the measurement ends when the worker is
  // successfully started.
  // The metric is not recorded in the following situations:
  // 1) The worker was in state INSTALLED or ACTIVATING, and the browser had to
  //    wait for it to become ACTIVATED. This is to avoid including the time to
  //    execute the activate event handlers in the worker's script.
  // 2) The worker was started for the fetch AND DevTools was attached during
  //    startup. This is intended to avoid including the time for debugging.
  // 3) The request is for New Tab Page. This is because it tends to dominate
  //    the stats and makes the results largely skewed.
  if (resource_type_ != RESOURCE_TYPE_MAIN_FRAME)
    return;
  if (!worker_already_activated_)
    return;
  if (version->skip_recording_startup_time() &&
      initial_worker_status_ != EmbeddedWorkerStatus::RUNNING) {
    return;
  }
  if (version->should_exclude_from_uma())
    return;
  ServiceWorkerMetrics::RecordActivatedWorkerPreparationTimeForMainFrame(
      worker_ready_time_ - request()->creation_time(), initial_worker_status_,
      version->embedded_worker()->start_situation());
}

void ServiceWorkerURLRequestJob::DidDispatchFetchEvent(
    ServiceWorkerStatusCode status,
    ServiceWorkerFetchEventResult fetch_result,
    const ServiceWorkerResponse& response,
    const scoped_refptr<ServiceWorkerVersion>& version) {
  fetch_dispatcher_.reset();
  ServiceWorkerMetrics::RecordFetchEventStatus(IsMainResourceLoad(), status);

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  if (!delegate_->RequestStillValid(&result)) {
    RecordResult(result);
    DeliverErrorResponse();
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_FETCH_EVENT_DISPATCH);
    if (IsMainResourceLoad()) {
      // Using the service worker failed, so fallback to network.
      delegate_->MainResourceLoadFailed();
      FinalizeFallbackToNetwork();
    } else {
      DeliverErrorResponse();
    }
    return;
  }

  if (fetch_result == SERVICE_WORKER_FETCH_EVENT_RESULT_FALLBACK) {
    ServiceWorkerMetrics::RecordFallbackedRequestMode(request_mode_);
    if (IsFallbackToRendererNeeded()) {
      FinalizeFallbackToRenderer();
    } else {
      FinalizeFallbackToNetwork();
    }
    return;
  }

  // We should have a response now.
  DCHECK_EQ(SERVICE_WORKER_FETCH_EVENT_RESULT_RESPONSE, fetch_result);

  // A response with status code 0 is Blink telling us to respond with network
  // error.
  if (response.status_code == 0) {
    RecordStatusZeroResponseError(response.error);
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  load_timing_info_.send_end = base::TimeTicks::Now();

  // Creates a new HttpResponseInfo using the the ServiceWorker script's
  // HttpResponseInfo to show HTTPS padlock.
  // TODO(horo): When we support mixed-content (HTTP) no-cors requests from a
  // ServiceWorker, we have to check the security level of the responses.
  DCHECK(!http_response_info_);
  DCHECK(version);
  const net::HttpResponseInfo* main_script_http_info =
      version->GetMainScriptHttpResponseInfo();
  if (main_script_http_info) {
    // In normal case |main_script_http_info| must be set while starting the
    // ServiceWorker. But when the ServiceWorker registration database was not
    // written correctly, it may be null.
    // TODO(horo): Change this line to DCHECK when crbug.com/485900 is fixed.
    http_response_info_.reset(
        new net::HttpResponseInfo(*main_script_http_info));
  }

  // Set up a request for reading the stream.
  if (response.stream_url.is_valid()) {
    DCHECK(response.blob_uuid.empty());
    SetResponseBodyType(STREAM);
    SetResponse(response);
    stream_reader_.reset(new ServiceWorkerStreamReader(this, version));
    stream_reader_->Start(response.stream_url);
    return;
  }

  // Set up a request for reading the blob.
  if (!response.blob_uuid.empty() && blob_storage_context_) {
    SetResponseBodyType(BLOB);
    std::unique_ptr<storage::BlobDataHandle> blob_data_handle =
        blob_storage_context_->GetBlobDataFromUUID(response.blob_uuid);
    if (!blob_data_handle) {
      // The renderer gave us a bad blob UUID.
      RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_BLOB);
      DeliverErrorResponse();
      return;
    }
    blob_reader_.reset(new ServiceWorkerBlobReader(this));
    blob_reader_->Start(std::move(blob_data_handle), request()->context());
  }

  SetResponse(response);
  if (!blob_reader_) {
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_HEADERS_ONLY_RESPONSE);
    CommitResponseHeader();
  }
}

void ServiceWorkerURLRequestJob::SetResponse(
    const ServiceWorkerResponse& response) {
  response_url_ = response.url;
  service_worker_response_type_ = response.response_type;
  cors_exposed_header_names_ = response.cors_exposed_header_names;
  response_time_ = response.response_time;
  CreateResponseHeader(response.status_code, response.status_text,
                       response.headers);
  load_timing_info_.receive_headers_end = base::TimeTicks::Now();

  response_is_in_cache_storage_ = response.is_in_cache_storage;
  response_cache_storage_cache_name_ = response.cache_storage_cache_name;
}

void ServiceWorkerURLRequestJob::CreateResponseHeader(
    int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers) {
  // TODO(kinuko): If the response has an identifier to on-disk cache entry,
  // pull response header from the disk.
  std::string status_line(
      base::StringPrintf("HTTP/1.1 %d %s", status_code, status_text.c_str()));
  status_line.push_back('\0');
  http_response_headers_ = new net::HttpResponseHeaders(status_line);
  for (ServiceWorkerHeaderMap::const_iterator it = headers.begin();
       it != headers.end();
       ++it) {
    std::string header;
    header.reserve(it->first.size() + 2 + it->second.size());
    header.append(it->first);
    header.append(": ");
    header.append(it->second);
    http_response_headers_->AddHeader(header);
  }
}

void ServiceWorkerURLRequestJob::CommitResponseHeader() {
  if (!http_response_info_)
    http_response_info_.reset(new net::HttpResponseInfo());
  http_response_info_->headers.swap(http_response_headers_);
  http_response_info_->vary_data = net::HttpVaryData();
  http_response_info_->metadata =
      blob_reader_ ? blob_reader_->response_metadata() : nullptr;
  NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::DeliverErrorResponse() {
  // TODO(falken): Print an error to the console of the ServiceWorker and of
  // the requesting page.
  CreateResponseHeader(
      500, "Service Worker Response Error", ServiceWorkerHeaderMap());
  CommitResponseHeader();
}

void ServiceWorkerURLRequestJob::FinalizeFallbackToNetwork() {
  // Restart this request to create a new job. The default job (which will hit
  // network) will be created in the next time because our request handler will
  // return nullptr after restarting and this means our interceptor does not
  // intercept.
  if (ShouldRecordResult())
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_FALLBACK_RESPONSE);
  response_type_ = FALLBACK_TO_NETWORK;
  NotifyRestartRequired();
  return;
}

void ServiceWorkerURLRequestJob::FinalizeFallbackToRenderer() {
  // TODO(mek): http://crbug.com/604084 Figure out what to do about CORS
  // preflight and fallbacks for foreign fetch events.
  DCHECK_NE(fetch_type_, ServiceWorkerFetchType::FOREIGN_FETCH);
  fall_back_required_ = true;
  if (ShouldRecordResult())
    RecordResult(ServiceWorkerMetrics::REQUEST_JOB_FALLBACK_FOR_CORS);
  CreateResponseHeader(400, "Service Worker Fallback Required",
                       ServiceWorkerHeaderMap());
  response_type_ = FALLBACK_TO_RENDERER;
  CommitResponseHeader();
}

bool ServiceWorkerURLRequestJob::IsFallbackToRendererNeeded() const {
  // When the request_mode is |CORS| or |CORS-with-forced-preflight| and the
  // origin of the request URL is different from the security origin of the
  // document, we can't simply fallback to the network in the browser process.
  // It is because the CORS preflight logic is implemented in the renderer. So
  // we return a fall_back_required response to the renderer.
  // If fetch_type is |FOREIGN_FETCH| any required CORS checks will have already
  // been done in the renderer (and if a preflight was necesary the request
  // would never have reached foreign fetch), so such requests can always
  // fallback to the network directly.
  return !IsMainResourceLoad() &&
         fetch_type_ != ServiceWorkerFetchType::FOREIGN_FETCH &&
         (request_mode_ == FETCH_REQUEST_MODE_CORS ||
          request_mode_ == FETCH_REQUEST_MODE_CORS_WITH_FORCED_PREFLIGHT) &&
         (!request()->initiator().has_value() ||
          !request()->initiator()->IsSameOriginWith(
              url::Origin(request()->url())));
}

void ServiceWorkerURLRequestJob::SetResponseBodyType(ResponseBodyType type) {
  DCHECK_EQ(response_body_type_, UNKNOWN);
  DCHECK_NE(type, UNKNOWN);
  response_body_type_ = type;
}

bool ServiceWorkerURLRequestJob::ShouldRecordResult() {
  return !did_record_result_ && is_started_ &&
         response_type_ == FORWARD_TO_SERVICE_WORKER;
}

void ServiceWorkerURLRequestJob::RecordStatusZeroResponseError(
    blink::WebServiceWorkerResponseError error) {
  // It violates style guidelines to handle a NOTREACHED() failure but if there
  // is a bug don't let it corrupt UMA results by double-counting.
  if (!ShouldRecordResult()) {
    NOTREACHED();
    return;
  }
  RecordResult(ServiceWorkerMetrics::REQUEST_JOB_ERROR_RESPONSE_STATUS_ZERO);
  ServiceWorkerMetrics::RecordStatusZeroResponseError(IsMainResourceLoad(),
                                                      error);
}

void ServiceWorkerURLRequestJob::NotifyHeadersComplete() {
  OnStartCompleted();
  URLRequestJob::NotifyHeadersComplete();
}

void ServiceWorkerURLRequestJob::NotifyStartError(
    net::URLRequestStatus status) {
  OnStartCompleted();
  URLRequestJob::NotifyStartError(status);
}

void ServiceWorkerURLRequestJob::NotifyRestartRequired() {
  ServiceWorkerResponseInfo::ForRequest(request_, true)
      ->OnPrepareToRestart(worker_start_time_, worker_ready_time_);
  delegate_->OnPrepareToRestart();
  URLRequestJob::NotifyRestartRequired();
}

void ServiceWorkerURLRequestJob::OnStartCompleted() const {
  if (response_type_ != FORWARD_TO_SERVICE_WORKER &&
      response_type_ != FALLBACK_TO_RENDERER) {
    ServiceWorkerResponseInfo::ForRequest(request_, true)
        ->OnStartCompleted(
            false /* was_fetched_via_service_worker */,
            false /* was_fetched_via_foreign_fetch */,
            false /* was_fallback_required */,
            GURL() /* original_url_via_service_worker */,
            blink::WebServiceWorkerResponseTypeDefault,
            base::TimeTicks() /* service_worker_start_time */,
            base::TimeTicks() /* service_worker_ready_time */,
            false /* respons_is_in_cache_storage */,
            std::string() /* response_cache_storage_cache_name */,
            ServiceWorkerHeaderList() /* cors_exposed_header_names */);
    return;
  }
  ServiceWorkerResponseInfo::ForRequest(request_, true)
      ->OnStartCompleted(
          true /* was_fetched_via_service_worker */,
          fetch_type_ == ServiceWorkerFetchType::FOREIGN_FETCH,
          fall_back_required_, response_url_, service_worker_response_type_,
          worker_start_time_, worker_ready_time_, response_is_in_cache_storage_,
          response_cache_storage_cache_name_, cors_exposed_header_names_);
}

bool ServiceWorkerURLRequestJob::IsMainResourceLoad() const {
  return ServiceWorkerUtils::IsMainResourceType(resource_type_);
}

bool ServiceWorkerURLRequestJob::HasRequestBody() {
  // URLRequest::has_upload() must be checked since its upload data may have
  // been cleared while handling a redirect.
  return request_->has_upload() && body_.get() && blob_storage_context_;
}

void ServiceWorkerURLRequestJob::RequestBodyFileSizesResolved(bool success) {
  file_size_resolver_.reset();
  if (!success) {
    RecordResult(
        ServiceWorkerMetrics::REQUEST_JOB_ERROR_REQUEST_BODY_BLOB_FAILED);
    // TODO(falken): This and below should probably be NotifyStartError, not
    // DeliverErrorResponse. But changing it causes
    // ServiceWorkerURLRequestJobTest.DeletedProviderHostBeforeFetchEvent to
    // fail.
    DeliverErrorResponse();
    return;
  }

  ServiceWorkerMetrics::URLRequestJobResult result =
      ServiceWorkerMetrics::REQUEST_JOB_ERROR_BAD_DELEGATE;
  ServiceWorkerVersion* active_worker =
      delegate_->GetServiceWorkerVersion(&result);
  if (!active_worker) {
    RecordResult(result);
    DeliverErrorResponse();
    return;
  }

  worker_already_activated_ =
      active_worker->status() == ServiceWorkerVersion::ACTIVATED;
  initial_worker_status_ = active_worker->running_status();

  DCHECK(!fetch_dispatcher_);
  fetch_dispatcher_.reset(new ServiceWorkerFetchDispatcher(
      CreateFetchRequest(), active_worker, resource_type_, request()->net_log(),
      base::Bind(&ServiceWorkerURLRequestJob::DidPrepareFetchEvent,
                 weak_factory_.GetWeakPtr(), active_worker),
      base::Bind(&ServiceWorkerURLRequestJob::DidDispatchFetchEvent,
                 weak_factory_.GetWeakPtr())));
  worker_start_time_ = base::TimeTicks::Now();
  fetch_dispatcher_->MaybeStartNavigationPreload(request());
  fetch_dispatcher_->Run();
}

}  // namespace content
