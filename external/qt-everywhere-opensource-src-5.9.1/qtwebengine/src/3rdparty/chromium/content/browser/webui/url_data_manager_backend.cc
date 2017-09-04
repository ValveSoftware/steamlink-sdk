// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/url_data_manager_backend.h"

#include <set>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/threading/worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/histogram_internals_request_job.h"
#include "content/browser/net/view_blob_internals_job_factory.h"
#include "content/browser/net/view_http_cache_job_factory.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/webui/shared_resources_data_source.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/url_constants.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "url/url_util.h"

namespace content {

namespace {

const char kChromeURLContentSecurityPolicyHeaderBase[] =
    "Content-Security-Policy: ";

const char kChromeURLXFrameOptionsHeader[] = "X-Frame-Options: DENY";
const char kNetworkErrorKey[] = "netError";

bool SchemeIsInSchemes(const std::string& scheme,
                       const std::vector<std::string>& schemes) {
  return std::find(schemes.begin(), schemes.end(), scheme) != schemes.end();
}

// Returns whether |url| passes some sanity checks and is a valid GURL.
bool CheckURLIsValid(const GURL& url) {
  std::vector<std::string> additional_schemes;
  DCHECK(url.SchemeIs(kChromeDevToolsScheme) || url.SchemeIs(kChromeUIScheme) ||
         (GetContentClient()->browser()->GetAdditionalWebUISchemes(
              &additional_schemes),
          SchemeIsInSchemes(url.scheme(), additional_schemes)));

  if (!url.is_valid()) {
    NOTREACHED();
    return false;
  }

  return true;
}

// Parse |url| to get the path which will be used to resolve the request. The
// path is the remaining portion after the scheme and hostname.
void URLToRequestPath(const GURL& url, std::string* path) {
  const std::string& spec = url.possibly_invalid_spec();
  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  // + 1 to skip the slash at the beginning of the path.
  int offset = parsed.CountCharactersBefore(url::Parsed::PATH, false) + 1;

  if (offset < static_cast<int>(spec.size()))
    path->assign(spec.substr(offset));
}

// Returns a value of 'Origin:' header for the |request| if the header is set.
// Otherwise returns an empty string.
std::string GetOriginHeaderValue(const net::URLRequest* request) {
  std::string result;
  if (request->extra_request_headers().GetHeader(
          net::HttpRequestHeaders::kOrigin, &result))
    return result;
  net::HttpRequestHeaders headers;
  if (request->GetFullRequestHeaders(&headers))
    headers.GetHeader(net::HttpRequestHeaders::kOrigin, &result);
  return result;
}

// Copy data from source buffer into IO buffer destination.
// TODO(groby): Very similar to URLRequestSimpleJob, unify at some point.
void CopyData(const scoped_refptr<net::IOBuffer>& buf,
              int buf_size,
              const scoped_refptr<base::RefCountedMemory>& data,
              int64_t data_offset) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/455423 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "455423 URLRequestChromeJob::CompleteRead memcpy"));
  memcpy(buf->data(), data->front() + data_offset, buf_size);
}

}  // namespace

// URLRequestChromeJob is a net::URLRequestJob that manages running
// chrome-internal resource requests asynchronously.
// It hands off URL requests to ChromeURLDataManager, which asynchronously
// calls back once the data is available.
class URLRequestChromeJob : public net::URLRequestJob {
 public:
  // |is_incognito| set when job is generated from an incognito profile.
  URLRequestChromeJob(net::URLRequest* request,
                      net::NetworkDelegate* network_delegate,
                      URLDataManagerBackend* backend,
                      bool is_incognito);

  // net::URLRequestJob implementation.
  void Start() override;
  void Kill() override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;
  bool GetMimeType(std::string* mime_type) const override;
  int GetResponseCode() const override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  std::unique_ptr<net::SourceStream> SetUpSourceStream() override;

  // Used to notify that the requested data's |mime_type| is ready.
  void MimeTypeAvailable(const std::string& mime_type);

  // Called by ChromeURLDataManager to notify us that the data blob is ready
  // for us.  |bytes| may be null, indicating an error.
  void DataAvailable(base::RefCountedMemory* bytes);

  // Returns a weak pointer to the job.
  base::WeakPtr<URLRequestChromeJob> AsWeakPtr();

  void set_mime_type(const std::string& mime_type) {
    mime_type_ = mime_type;
  }

  void set_allow_caching(bool allow_caching) {
    allow_caching_ = allow_caching;
  }

  void set_add_content_security_policy(bool add_content_security_policy) {
    add_content_security_policy_ = add_content_security_policy;
  }

  void set_content_security_policy_object_source(
      const std::string& data) {
    content_security_policy_object_source_ = data;
  }

  void set_content_security_policy_script_source(
      const std::string& data) {
    content_security_policy_script_source_ = data;
  }

  void set_content_security_policy_child_source(
      const std::string& data) {
    content_security_policy_child_source_ = data;
  }

  void set_content_security_policy_style_source(
      const std::string& data) {
    content_security_policy_style_source_ = data;
  }

  void set_content_security_policy_image_source(
      const std::string& data) {
    content_security_policy_image_source_ = data;
  }

  void set_deny_xframe_options(bool deny_xframe_options) {
    deny_xframe_options_ = deny_xframe_options;
  }

  void set_send_content_type_header(bool send_content_type_header) {
    send_content_type_header_ = send_content_type_header;
  }

  void set_access_control_allow_origin(const std::string& value) {
    access_control_allow_origin_ = value;
  }

  void set_is_gzipped(bool is_gzipped) {
    is_gzipped_ = is_gzipped;
  }

  // Returns true when job was generated from an incognito profile.
  bool is_incognito() const {
    return is_incognito_;
  }

 private:
  ~URLRequestChromeJob() override;

  // Helper for Start(), to let us start asynchronously.
  // (This pattern is shared by most net::URLRequestJob implementations.)
  void StartAsync();

  // Due to a race condition, DevTools relies on a legacy thread hop to the UI
  // thread before calling StartAsync.
  // TODO(caseq): Fix the race condition and remove this thread hop in
  // https://crbug.com/616641.
  static void DelayStartForDevTools(
      const base::WeakPtr<URLRequestChromeJob>& job);

  // Post a task to copy |data_| to |buf_| on a worker thread, to avoid browser
  // jank. (|data_| might be mem-mapped, so a memcpy can trigger file ops).
  int PostReadTask(scoped_refptr<net::IOBuffer> buf, int buf_size);

  // The actual data we're serving.  NULL until it's been fetched.
  scoped_refptr<base::RefCountedMemory> data_;

  // The current offset into the data that we're handing off to our
  // callers via the Read interfaces.
  int data_offset_;

  // When DataAvailable() is called with a null argument, indicating an error,
  // this is set accordingly to a code for ReadRawData() to return.
  net::Error data_available_status_;

  // For async reads, we keep around a pointer to the buffer that
  // we're reading into.
  scoped_refptr<net::IOBuffer> pending_buf_;
  int pending_buf_size_;
  std::string mime_type_;

  // If true, set a header in the response to prevent it from being cached.
  bool allow_caching_;

  // If true, set the Content Security Policy (CSP) header.
  bool add_content_security_policy_;

  // These are used with the CSP.
  std::string content_security_policy_script_source_;
  std::string content_security_policy_object_source_;
  std::string content_security_policy_child_source_;
  std::string content_security_policy_style_source_;
  std::string content_security_policy_image_source_;

  // If true, sets  the "X-Frame-Options: DENY" header.
  bool deny_xframe_options_;

  // If true, sets the "Content-Type: <mime-type>" header.
  bool send_content_type_header_;

  // If not empty, "Access-Control-Allow-Origin:" is set to the value of this
  // string.
  std::string access_control_allow_origin_;

  // True when job is generated from an incognito profile.
  const bool is_incognito_;

  // True when gzip encoding should be used. NOTE: this requires the original
  // resources in resources.pak use compress="gzip".
  bool is_gzipped_;

  // The backend is owned by net::URLRequestContext and always outlives us.
  URLDataManagerBackend* const backend_;

  base::WeakPtrFactory<URLRequestChromeJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestChromeJob);
};

URLRequestChromeJob::URLRequestChromeJob(net::URLRequest* request,
                                         net::NetworkDelegate* network_delegate,
                                         URLDataManagerBackend* backend,
                                         bool is_incognito)
    : net::URLRequestJob(request, network_delegate),
      data_offset_(0),
      data_available_status_(net::OK),
      pending_buf_size_(0),
      allow_caching_(true),
      add_content_security_policy_(true),
      deny_xframe_options_(true),
      send_content_type_header_(false),
      is_incognito_(is_incognito),
      is_gzipped_(false),
      backend_(backend),
      weak_factory_(this) {
  DCHECK(backend);
}

URLRequestChromeJob::~URLRequestChromeJob() {
  CHECK(!backend_->HasPendingJob(this));
}

void URLRequestChromeJob::Start() {
  const GURL url = request_->url();

  // Due to a race condition, DevTools relies on a legacy thread hop to the UI
  // thread before calling StartAsync.
  // TODO(caseq): Fix the race condition and remove this thread hop in
  // https://crbug.com/616641.
  if (url.SchemeIs(kChromeDevToolsScheme)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&URLRequestChromeJob::DelayStartForDevTools,
                   weak_factory_.GetWeakPtr()));
    return;
  }

  // Start reading asynchronously so that all error reporting and data
  // callbacks happen as they would for network requests.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&URLRequestChromeJob::StartAsync, weak_factory_.GetWeakPtr()));

  TRACE_EVENT_ASYNC_BEGIN1("browser", "DataManager:Request", this, "URL",
      url.possibly_invalid_spec());
}

void URLRequestChromeJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  backend_->RemoveRequest(this);
  URLRequestJob::Kill();
}

bool URLRequestChromeJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return !mime_type_.empty();
}

int URLRequestChromeJob::GetResponseCode() const {
  return net::HTTP_OK;
}

void URLRequestChromeJob::GetResponseInfo(net::HttpResponseInfo* info) {
  DCHECK(!info->headers.get());
  // Set the headers so that requests serviced by ChromeURLDataManager return a
  // status code of 200. Without this they return a 0, which makes the status
  // indistiguishable from other error types. Instant relies on getting a 200.
  info->headers = new net::HttpResponseHeaders("HTTP/1.1 200 OK");

  // Determine the least-privileged content security policy header, if any,
  // that is compatible with a given WebUI URL, and append it to the existing
  // response headers.
  if (add_content_security_policy_) {
    std::string base = kChromeURLContentSecurityPolicyHeaderBase;
    base.append(content_security_policy_script_source_);
    base.append(content_security_policy_object_source_);
    base.append(content_security_policy_child_source_);
    base.append(content_security_policy_style_source_);
    base.append(content_security_policy_image_source_);
    info->headers->AddHeader(base);
  }

  if (deny_xframe_options_)
    info->headers->AddHeader(kChromeURLXFrameOptionsHeader);

  if (!allow_caching_)
    info->headers->AddHeader("Cache-Control: no-cache");

  if (send_content_type_header_ && !mime_type_.empty()) {
    std::string content_type =
        base::StringPrintf("%s:%s", net::HttpRequestHeaders::kContentType,
                           mime_type_.c_str());
    info->headers->AddHeader(content_type);
  }

  if (!access_control_allow_origin_.empty()) {
    info->headers->AddHeader("Access-Control-Allow-Origin: " +
                             access_control_allow_origin_);
    info->headers->AddHeader("Vary: Origin");
  }

  if (is_gzipped_)
    info->headers->AddHeader("Content-Encoding: gzip");
}

std::unique_ptr<net::SourceStream> URLRequestChromeJob::SetUpSourceStream() {
  std::unique_ptr<net::SourceStream> source =
      net::URLRequestJob::SetUpSourceStream();
  return is_gzipped_ ? net::GzipSourceStream::Create(
                           std::move(source), net::SourceStream::TYPE_GZIP)
                     : std::move(source);
}

void URLRequestChromeJob::MimeTypeAvailable(const std::string& mime_type) {
  set_mime_type(mime_type);
  NotifyHeadersComplete();
}

void URLRequestChromeJob::DataAvailable(base::RefCountedMemory* bytes) {
  TRACE_EVENT_ASYNC_END0("browser", "DataManager:Request", this);
  DCHECK(!data_);

  // All further requests will be satisfied from the passed-in data.
  data_ = bytes;
  if (!bytes)
    data_available_status_ = net::ERR_FAILED;

  if (pending_buf_) {
    // The request has already been marked async.
    int result = bytes ? PostReadTask(pending_buf_, pending_buf_size_)
                       : data_available_status_;
    pending_buf_ = nullptr;
    if (result != net::ERR_IO_PENDING)
      ReadRawDataComplete(result);
  }
}

base::WeakPtr<URLRequestChromeJob> URLRequestChromeJob::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

int URLRequestChromeJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(!pending_buf_.get());

  // Handle the cases when DataAvailable() has already been called.
  if (data_available_status_ != net::OK)
    return data_available_status_;
  if (data_)
    return PostReadTask(buf, buf_size);

  // DataAvailable() has not been called yet.  Mark the request as async.
  pending_buf_ = buf;
  pending_buf_size_ = buf_size;
  return net::ERR_IO_PENDING;
}

int URLRequestChromeJob::PostReadTask(scoped_refptr<net::IOBuffer> buf,
                                      int buf_size) {
  DCHECK(buf);
  DCHECK(data_);
  CHECK(buf->data());

  int remaining = data_->size() - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;

  if (buf_size == 0)
    return 0;

  base::WorkerPool::GetTaskRunner(false)->PostTaskAndReply(
      FROM_HERE, base::Bind(&CopyData, base::RetainedRef(buf), buf_size, data_,
                            data_offset_),
      base::Bind(&URLRequestChromeJob::ReadRawDataComplete, AsWeakPtr(),
                 buf_size));
  data_offset_ += buf_size;

  return net::ERR_IO_PENDING;
}

void URLRequestChromeJob::DelayStartForDevTools(
    const base::WeakPtr<URLRequestChromeJob>& job) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&URLRequestChromeJob::StartAsync, job));
}

void URLRequestChromeJob::StartAsync() {
  if (!request_)
    return;

  if (!backend_->StartRequest(request_, this)) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_INVALID_URL));
  }
}

namespace {

// Gets mime type for data that is available from |source| by |path|.
// After that, notifies |job| that mime type is available. This method
// should be called on the UI thread, but notification is performed on
// the IO thread.
void GetMimeTypeOnUI(URLDataSourceImpl* source,
                     const std::string& path,
                     const base::WeakPtr<URLRequestChromeJob>& job) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string mime_type = source->source()->GetMimeType(path);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&URLRequestChromeJob::MimeTypeAvailable, job, mime_type));
}

}  // namespace

namespace {

bool IsValidNetworkErrorCode(int error_code) {
  std::unique_ptr<base::DictionaryValue> error_codes = net::GetNetConstants();
  const base::DictionaryValue* net_error_codes_dict = nullptr;

  for (base::DictionaryValue::Iterator itr(*error_codes); !itr.IsAtEnd();
           itr.Advance()) {
    if (itr.key() == kNetworkErrorKey) {
      itr.value().GetAsDictionary(&net_error_codes_dict);
      break;
    }
  }

  if (net_error_codes_dict != nullptr) {
    for (base::DictionaryValue::Iterator itr(*net_error_codes_dict);
             !itr.IsAtEnd(); itr.Advance()) {
      int net_error_code;
      itr.value().GetAsInteger(&net_error_code);
      if (error_code == net_error_code)
        return true;
    }
  }
  return false;
}

class ChromeProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |is_incognito| should be set for incognito profiles.
  ChromeProtocolHandler(ResourceContext* resource_context,
                        bool is_incognito,
                        ChromeBlobStorageContext* blob_storage_context)
      : resource_context_(resource_context),
        is_incognito_(is_incognito),
        blob_storage_context_(blob_storage_context) {}
  ~ChromeProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK(request);

    // Check for chrome://view-http-cache/*, which uses its own job type.
    if (ViewHttpCacheJobFactory::IsSupportedURL(request->url()))
      return ViewHttpCacheJobFactory::CreateJobForRequest(request,
                                                          network_delegate);

    // Next check for chrome://blob-internals/, which uses its own job type.
    if (ViewBlobInternalsJobFactory::IsSupportedURL(request->url())) {
      return ViewBlobInternalsJobFactory::CreateJobForRequest(
          request, network_delegate, blob_storage_context_->context());
    }

    // Next check for chrome://histograms/, which uses its own job type.
    if (request->url().SchemeIs(kChromeUIScheme) &&
        request->url().host_piece() == kChromeUIHistogramHost) {
      return new HistogramInternalsRequestJob(request, network_delegate);
    }

    // Check for chrome://network-error/, which uses its own job type.
    if (request->url().SchemeIs(kChromeUIScheme) &&
        request->url().host_piece() == kChromeUINetworkErrorHost) {
      // Get the error code passed in via the request URL path.
      std::basic_string<char> error_code_string =
          request->url().path().substr(1);

      int error_code;
      if (base::StringToInt(error_code_string, &error_code)) {
        // Check for a valid error code.
        if (IsValidNetworkErrorCode(error_code) &&
            error_code != net::Error::ERR_IO_PENDING) {
          return new net::URLRequestErrorJob(request, network_delegate,
                                             error_code);
        }
      }
    }

    // Check for chrome://dino which is an alias for chrome://network-error/-106
    if (request->url().SchemeIs(kChromeUIScheme) &&
        request->url().host() == kChromeUIDinoHost) {
      return new net::URLRequestErrorJob(request, network_delegate,
                                         net::Error::ERR_INTERNET_DISCONNECTED);
    }

    // Fall back to using a custom handler
    return new URLRequestChromeJob(
        request, network_delegate,
        GetURLDataManagerForResourceContext(resource_context_), is_incognito_);
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  // These members are owned by ProfileIOData, which owns this ProtocolHandler.
  ResourceContext* const resource_context_;

  // True when generated from an incognito profile.
  const bool is_incognito_;
  ChromeBlobStorageContext* blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProtocolHandler);
};

}  // namespace

URLDataManagerBackend::URLDataManagerBackend()
    : next_request_id_(0) {
  URLDataSource* shared_source = new SharedResourcesDataSource();
  URLDataSourceImpl* source_impl =
      new URLDataSourceImpl(shared_source->GetSource(), shared_source);
  AddDataSource(source_impl);
}

URLDataManagerBackend::~URLDataManagerBackend() {
  for (const auto& i : data_sources_)
    i.second->backend_ = nullptr;
  data_sources_.clear();
}

// static
std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>
URLDataManagerBackend::CreateProtocolHandler(
    ResourceContext* resource_context,
    bool is_incognito,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK(resource_context);
  return base::MakeUnique<ChromeProtocolHandler>(resource_context, is_incognito,
                                                 blob_storage_context);
}

void URLDataManagerBackend::AddDataSource(
    URLDataSourceImpl* source) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DataSourceMap::iterator i = data_sources_.find(source->source_name());
  if (i != data_sources_.end()) {
    if (!source->source()->ShouldReplaceExistingSource())
      return;
    i->second->backend_ = nullptr;
  }
  data_sources_[source->source_name()] = source;
  source->backend_ = this;
}

bool URLDataManagerBackend::HasPendingJob(
    URLRequestChromeJob* job) const {
  for (PendingRequestMap::const_iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job)
      return true;
  }
  return false;
}

bool URLDataManagerBackend::StartRequest(const net::URLRequest* request,
                                         URLRequestChromeJob* job) {
  if (!CheckURLIsValid(request->url()))
    return false;

  URLDataSourceImpl* source = GetDataSourceFromURL(request->url());
  if (!source)
    return false;

  if (!source->source()->ShouldServiceRequest(request))
    return false;

  std::string path;
  URLToRequestPath(request->url(), &path);
  source->source()->WillServiceRequest(request, &path);

  // Save this request so we know where to send the data.
  RequestID request_id = next_request_id_++;
  pending_requests_.insert(std::make_pair(request_id, job));

  job->set_allow_caching(source->source()->AllowCaching());
  job->set_add_content_security_policy(
      source->source()->ShouldAddContentSecurityPolicy());
  job->set_content_security_policy_script_source(
      source->source()->GetContentSecurityPolicyScriptSrc());
  job->set_content_security_policy_object_source(
      source->source()->GetContentSecurityPolicyObjectSrc());
  job->set_content_security_policy_child_source(
      source->source()->GetContentSecurityPolicyChildSrc());
  job->set_content_security_policy_style_source(
      source->source()->GetContentSecurityPolicyStyleSrc());
  job->set_content_security_policy_image_source(
      source->source()->GetContentSecurityPolicyImgSrc());
  job->set_deny_xframe_options(
      source->source()->ShouldDenyXFrameOptions());
  job->set_send_content_type_header(
      source->source()->ShouldServeMimeTypeAsContentTypeHeader());
  job->set_is_gzipped(source->source()->IsGzipped(path));

  std::string origin = GetOriginHeaderValue(request);
  if (!origin.empty()) {
    std::string header =
        source->source()->GetAccessControlAllowOriginForOrigin(origin);
    DCHECK(header.empty() || header == origin || header == "*" ||
           header == "null");
    job->set_access_control_allow_origin(header);
  }

  // Look up additional request info to pass down.
  int child_id = -1;
  ResourceRequestInfo::WebContentsGetter wc_getter;
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info) {
    child_id = info->GetChildID();
    wc_getter = info->GetWebContentsGetterForRequest();
  }

  // Forward along the request to the data source.
  scoped_refptr<base::SingleThreadTaskRunner> target_runner =
      source->source()->TaskRunnerForRequestPath(path);
  if (!target_runner) {
    job->MimeTypeAvailable(source->source()->GetMimeType(path));
    // Eliminate potentially dangling pointer to avoid future use.
    job = nullptr;

    // The DataSource is agnostic to which thread StartDataRequest is called
    // on for this path.  Call directly into it from this thread, the IO
    // thread.
    source->source()->StartDataRequest(
        path, wc_getter,
        base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
  } else {
    // URLRequestChromeJob should receive mime type before data. This
    // is guaranteed because request for mime type is placed in the
    // message loop before request for data. And correspondingly their
    // replies are put on the IO thread in the same order.
    target_runner->PostTask(
        FROM_HERE, base::Bind(&GetMimeTypeOnUI, base::RetainedRef(source), path,
                              job->AsWeakPtr()));

    // The DataSource wants StartDataRequest to be called on a specific thread,
    // usually the UI thread, for this path.
    target_runner->PostTask(
        FROM_HERE, base::Bind(&URLDataManagerBackend::CallStartRequest,
                              base::RetainedRef(source), path, child_id,
                              wc_getter, request_id));
  }
  return true;
}

URLDataSourceImpl* URLDataManagerBackend::GetDataSourceFromURL(
    const GURL& url) {
  // The input usually looks like: chrome://source_name/extra_bits?foo
  // so do a lookup using the host of the URL.
  DataSourceMap::iterator i = data_sources_.find(url.host());
  if (i != data_sources_.end())
    return i->second.get();

  // No match using the host of the URL, so do a lookup using the scheme for
  // URLs on the form source_name://extra_bits/foo .
  i = data_sources_.find(url.scheme() + "://");
  if (i != data_sources_.end())
    return i->second.get();

  // No matches found, so give up.
  return nullptr;
}

void URLDataManagerBackend::CallStartRequest(
    scoped_refptr<URLDataSourceImpl> source,
    const std::string& path,
    int child_id,
    const ResourceRequestInfo::WebContentsGetter& wc_getter,
    int request_id) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI) && child_id != -1 &&
      !RenderProcessHost::FromID(child_id)) {
    // Make the request fail if its initiating renderer is no longer valid.
    // This can happen when the IO thread posts this task just before the
    // renderer shuts down.
    // Note we check the process id instead of wc_getter because requests from
    // workers wouldn't have a WebContents.
    source->SendResponse(request_id, nullptr);
    return;
  }
  source->source()->StartDataRequest(
      path,
      wc_getter,
      base::Bind(&URLDataSourceImpl::SendResponse, source, request_id));
}

void URLDataManagerBackend::RemoveRequest(URLRequestChromeJob* job) {
  // Remove the request from our list of pending requests.
  // If/when the source sends the data that was requested, the data will just
  // be thrown away.
  for (PendingRequestMap::iterator i = pending_requests_.begin();
       i != pending_requests_.end(); ++i) {
    if (i->second == job) {
      pending_requests_.erase(i);
      return;
    }
  }
}

void URLDataManagerBackend::DataAvailable(RequestID request_id,
                                          base::RefCountedMemory* bytes) {
  // Forward this data on to the pending net::URLRequest, if it exists.
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i != pending_requests_.end()) {
    URLRequestChromeJob* job = i->second;
    pending_requests_.erase(i);
    job->DataAvailable(bytes);
  }
}

namespace {

class DevToolsJobFactory
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // |is_incognito| should be set for incognito profiles.
  DevToolsJobFactory(ResourceContext* resource_context, bool is_incognito);
  ~DevToolsJobFactory() override;

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  // |resource_context_| and |network_delegate_| are owned by ProfileIOData,
  // which owns this ProtocolHandler.
  ResourceContext* const resource_context_;

  // True when generated from an incognito profile.
  const bool is_incognito_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsJobFactory);
};

DevToolsJobFactory::DevToolsJobFactory(ResourceContext* resource_context,
                                       bool is_incognito)
    : resource_context_(resource_context), is_incognito_(is_incognito) {
  DCHECK(resource_context_);
}

DevToolsJobFactory::~DevToolsJobFactory() {}

net::URLRequestJob*
DevToolsJobFactory::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  return new URLRequestChromeJob(
      request, network_delegate,
      GetURLDataManagerForResourceContext(resource_context_), is_incognito_);
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler* CreateDevToolsProtocolHandler(
    ResourceContext* resource_context,
    bool is_incognito) {
  return new DevToolsJobFactory(resource_context, is_incognito);
}

}  // namespace content
