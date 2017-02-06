// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_request_core.h"

#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "device/power_save_blocker/power_save_blocker.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context.h"

namespace content {

namespace {

// This is a UserData::Data that will be attached to a URLRequest as a
// side-channel for passing download parameters.
class DownloadRequestData : public base::SupportsUserData::Data {
 public:
  ~DownloadRequestData() override {}

  static void Attach(net::URLRequest* request,
                     DownloadUrlParameters* download_parameters,
                     uint32_t download_id);
  static DownloadRequestData* Get(net::URLRequest* request);
  static void Detach(net::URLRequest* request);

  std::unique_ptr<DownloadSaveInfo> TakeSaveInfo() {
    return std::move(save_info_);
  }
  uint32_t download_id() const { return download_id_; }
  const DownloadUrlParameters::OnStartedCallback& callback() const {
    return on_started_callback_;
  }

 private:
  static const int kKey;

  std::unique_ptr<DownloadSaveInfo> save_info_;
  uint32_t download_id_ = DownloadItem::kInvalidId;
  DownloadUrlParameters::OnStartedCallback on_started_callback_;
};

// static
const int DownloadRequestData::kKey = 0;

// static
void DownloadRequestData::Attach(net::URLRequest* request,
                                 DownloadUrlParameters* parameters,
                                 uint32_t download_id) {
  DownloadRequestData* request_data = new DownloadRequestData;
  request_data->save_info_.reset(
      new DownloadSaveInfo(parameters->GetSaveInfo()));
  request_data->download_id_ = download_id;
  request_data->on_started_callback_ = parameters->callback();
  request->SetUserData(&kKey, request_data);
}

// static
DownloadRequestData* DownloadRequestData::Get(net::URLRequest* request) {
  return static_cast<DownloadRequestData*>(request->GetUserData(&kKey));
}

// static
void DownloadRequestData::Detach(net::URLRequest* request) {
  request->RemoveUserData(&kKey);
}

}  // namespace

const int DownloadRequestCore::kDownloadByteStreamSize = 100 * 1024;

// static
std::unique_ptr<net::URLRequest> DownloadRequestCore::CreateRequestOnIOThread(
    uint32_t download_id,
    DownloadUrlParameters* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(download_id == DownloadItem::kInvalidId ||
         !params->content_initiated())
      << "Content initiated downloads shouldn't specify a download ID";

  // ResourceDispatcherHost{Base} is-not-a URLRequest::Delegate, and
  // DownloadUrlParameters can-not include resource_dispatcher_host_impl.h, so
  // we must down cast. RDHI is the only subclass of RDH as of 2012 May 4.
  std::unique_ptr<net::URLRequest> request(
      params->url_request_context_getter()
          ->GetURLRequestContext()
          ->CreateRequest(params->url(), net::DEFAULT_PRIORITY, nullptr));
  request->set_method(params->method());

  if (!params->post_body().empty()) {
    const std::string& body = params->post_body();
    std::unique_ptr<net::UploadElementReader> reader(
        net::UploadOwnedBytesElementReader::CreateWithString(body));
    request->set_upload(
        net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));
  }

  if (params->post_id() >= 0) {
    // The POST in this case does not have an actual body, and only works
    // when retrieving data from cache. This is done because we don't want
    // to do a re-POST without user consent, and currently don't have a good
    // plan on how to display the UI for that.
    DCHECK(params->prefer_cache());
    DCHECK_EQ("POST", params->method());
    std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
    request->set_upload(base::WrapUnique(new net::ElementsUploadDataStream(
        std::move(element_readers), params->post_id())));
  }

  int load_flags = request->load_flags();
  if (params->prefer_cache()) {
    // If there is upload data attached, only retrieve from cache because there
    // is no current mechanism to prompt the user for their consent for a
    // re-post. For GETs, try to retrieve data from the cache and skip
    // validating the entry if present.
    if (request->get_upload())
      load_flags |= net::LOAD_ONLY_FROM_CACHE;
    else
      load_flags |= net::LOAD_PREFERRING_CACHE;
  } else {
    load_flags |= net::LOAD_DISABLE_CACHE;
  }
  request->SetLoadFlags(load_flags);

  bool has_last_modified = !params->last_modified().empty();
  bool has_etag = !params->etag().empty();

  // If we've asked for a range, we want to make sure that we only get that
  // range if our current copy of the information is good.  We shouldn't be
  // asked to continue if we don't have a verifier.
  DCHECK(params->offset() == 0 || has_etag || has_last_modified);

  // If we're not at the beginning of the file, retrieve only the remaining
  // portion.
  if (params->offset() > 0 && (has_etag || has_last_modified)) {
    request->SetExtraRequestHeaderByName(
        "Range", base::StringPrintf("bytes=%" PRId64 "-", params->offset()),
        true);

    // In accordance with RFC 2616 Section 14.27, use If-Range to specify that
    // the server return the entire entity if the validator doesn't match.
    // Last-Modified can be used in the absence of ETag as a validator if the
    // response headers satisfied the HttpUtil::HasStrongValidators() predicate.
    //
    // This function assumes that HasStrongValidators() was true and that the
    // ETag and Last-Modified header values supplied are valid.
    request->SetExtraRequestHeaderByName(
        "If-Range", has_etag ? params->etag() : params->last_modified(), true);
  }

  for (const auto& header : params->request_headers())
    request->SetExtraRequestHeaderByName(header.first, header.second,
                                         false /*overwrite*/);

  DownloadRequestData::Attach(request.get(), params, download_id);
  return request;
}

DownloadRequestCore::DownloadRequestCore(net::URLRequest* request,
                                         Delegate* delegate)
    : delegate_(delegate),
      request_(request),
      download_id_(DownloadItem::kInvalidId),
      last_buffer_size_(0),
      bytes_read_(0),
      pause_count_(0),
      was_deferred_(false),
      is_partial_request_(false),
      started_(false),
      abort_reason_(DOWNLOAD_INTERRUPT_REASON_NONE) {
  DCHECK(request_);
  DCHECK(delegate_);
  RecordDownloadCount(UNTHROTTLED_COUNT);
  power_save_blocker_.reset(new device::PowerSaveBlocker(
      device::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension,
      device::PowerSaveBlocker::kReasonOther, "Download in progress",
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)));
  DownloadRequestData* request_data = DownloadRequestData::Get(request_);
  if (request_data) {
    save_info_ = request_data->TakeSaveInfo();
    download_id_ = request_data->download_id();
    on_started_callback_ = request_data->callback();
    DownloadRequestData::Detach(request_);
    is_partial_request_ = save_info_->offset > 0;
  } else {
    save_info_.reset(new DownloadSaveInfo);
  }
}

DownloadRequestCore::~DownloadRequestCore() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Remove output stream callback if a stream exists.
  if (stream_writer_)
    stream_writer_->RegisterCallback(base::Closure());

  UMA_HISTOGRAM_TIMES("SB2.DownloadDuration",
                      base::TimeTicks::Now() - download_start_time_);
}

std::unique_ptr<DownloadCreateInfo>
DownloadRequestCore::CreateDownloadCreateInfo(DownloadInterruptReason result) {
  DCHECK(!started_);
  started_ = true;
  std::unique_ptr<DownloadCreateInfo> create_info(new DownloadCreateInfo(
      base::Time::Now(), request()->net_log(), std::move(save_info_)));

  if (result == DOWNLOAD_INTERRUPT_REASON_NONE)
    create_info->remote_address = request()->GetSocketAddress().host();
  create_info->url_chain = request()->url_chain();
  create_info->referrer_url = GURL(request()->referrer());
  create_info->result = result;
  create_info->download_id = download_id_;
  return create_info;
}

bool DownloadRequestCore::OnResponseStarted(
    const std::string& override_mime_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(20) << __FUNCTION__ << "()" << DebugString();
  download_start_time_ = base::TimeTicks::Now();

  DownloadInterruptReason result =
      request()->response_headers()
          ? HandleSuccessfulServerResponse(*request()->response_headers(),
                                           save_info_.get())
          : DOWNLOAD_INTERRUPT_REASON_NONE;

  std::unique_ptr<DownloadCreateInfo> create_info =
      CreateDownloadCreateInfo(result);
  if (result != DOWNLOAD_INTERRUPT_REASON_NONE) {
    delegate_->OnStart(std::move(create_info),
                       std::unique_ptr<ByteStreamReader>(),
                       base::ResetAndReturn(&on_started_callback_));
    return false;
  }

  // If it's a download, we don't want to poison the cache with it.
  request()->StopCaching();

  // Lower priority as well, so downloads don't contend for resources
  // with main frames.
  request()->SetPriority(net::IDLE);

  // If the content-length header is not present (or contains something other
  // than numbers), the incoming content_length is -1 (unknown size).
  // Set the content length to 0 to indicate unknown size to DownloadManager.
  int64_t content_length = request()->GetExpectedContentSize() > 0
                               ? request()->GetExpectedContentSize()
                               : 0;
  create_info->total_bytes = content_length;

  // Create the ByteStream for sending data to the download sink.
  std::unique_ptr<ByteStreamReader> stream_reader;
  CreateByteStream(
      base::ThreadTaskRunnerHandle::Get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      kDownloadByteStreamSize, &stream_writer_, &stream_reader);
  stream_writer_->RegisterCallback(
      base::Bind(&DownloadRequestCore::ResumeRequest, AsWeakPtr()));

  if (!override_mime_type.empty())
    create_info->mime_type = override_mime_type;
  else
    request()->GetMimeType(&create_info->mime_type);

  // Get the last modified time and etag.
  const net::HttpResponseHeaders* headers = request()->response_headers();
  if (headers) {
    if (headers->HasStrongValidators()) {
      // If we don't have strong validators as per RFC 2616 section 13.3.3, then
      // we neither store nor use them for range requests.
      if (!headers->EnumerateHeader(nullptr, "Last-Modified",
                                    &create_info->last_modified))
        create_info->last_modified.clear();
      if (!headers->EnumerateHeader(nullptr, "ETag", &create_info->etag))
        create_info->etag.clear();
    }

    // Grab the first content-disposition header.  There may be more than one,
    // though as of this writing, the network stack ensures if there are, they
    // are all duplicates.
    headers->EnumerateHeader(nullptr, "Content-Disposition",
                             &create_info->content_disposition);

    if (!headers->GetMimeType(&create_info->original_mime_type))
      create_info->original_mime_type.clear();
  }

  // Blink verifies that the requester of this download is allowed to set a
  // suggested name for the security origin of the download URL. However, this
  // assumption doesn't hold if there were cross origin redirects. Therefore,
  // clear the suggested_name for such requests.
  if (create_info->url_chain.size() > 1 &&
      create_info->url_chain.front().GetOrigin() !=
          create_info->url_chain.back().GetOrigin())
    create_info->save_info->suggested_name.clear();

  RecordDownloadMimeType(create_info->mime_type);
  RecordDownloadContentDisposition(create_info->content_disposition);

  delegate_->OnStart(std::move(create_info), std::move(stream_reader),
                     base::ResetAndReturn(&on_started_callback_));
  return true;
}

bool DownloadRequestCore::OnRequestRedirected() {
  DVLOG(20) << __FUNCTION__ << "() " << DebugString();
  if (is_partial_request_) {
    // A redirect while attempting a partial resumption indicates a potential
    // middle box. Trigger another interruption so that the DownloadItem can
    // retry.
    abort_reason_ = DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE;
    return false;
  }
  return true;
}

// Create a new buffer, which will be handed to the download thread for file
// writing and deletion.
bool DownloadRequestCore::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                     int* buf_size,
                                     int min_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(buf && buf_size);
  DCHECK(!read_buffer_.get());

  *buf_size = min_size < 0 ? kReadBufSize : min_size;
  last_buffer_size_ = *buf_size;
  read_buffer_ = new net::IOBuffer(*buf_size);
  *buf = read_buffer_.get();
  return true;
}

// Pass the buffer to the download file writer.
bool DownloadRequestCore::OnReadCompleted(int bytes_read, bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(read_buffer_.get());

  base::TimeTicks now(base::TimeTicks::Now());
  if (!last_read_time_.is_null()) {
    double seconds_since_last_read = (now - last_read_time_).InSecondsF();
    if (now == last_read_time_)
      // Use 1/10 ms as a "very small number" so that we avoid
      // divide-by-zero error and still record a very high potential bandwidth.
      seconds_since_last_read = 0.00001;

    double actual_bandwidth = (bytes_read) / seconds_since_last_read;
    double potential_bandwidth = last_buffer_size_ / seconds_since_last_read;
    RecordBandwidth(actual_bandwidth, potential_bandwidth);
  }
  last_read_time_ = now;

  if (!bytes_read)
    return true;
  bytes_read_ += bytes_read;
  DCHECK(read_buffer_.get());

  // Take the data ship it down the stream.  If the stream is full, pause the
  // request; the stream callback will resume it.
  if (!stream_writer_->Write(read_buffer_, bytes_read)) {
    PauseRequest();
    *defer = was_deferred_ = true;
    last_stream_pause_time_ = now;
  }

  read_buffer_ = NULL;  // Drop our reference.

  if (pause_count_ > 0)
    *defer = was_deferred_ = true;

  return true;
}

void DownloadRequestCore::OnWillAbort(DownloadInterruptReason reason) {
  DVLOG(20) << __FUNCTION__ << "() reason=" << reason << " " << DebugString();
  DCHECK(!started_);
  abort_reason_ = reason;
}

void DownloadRequestCore::OnResponseCompleted(
    const net::URLRequestStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int response_code = status.is_success() ? request()->GetResponseCode() : 0;
  DVLOG(20) << __FUNCTION__ << "()" << DebugString()
            << " status.status() = " << status.status()
            << " status.error() = " << status.error()
            << " response_code = " << response_code;

  DownloadInterruptReason reason = HandleRequestStatus(status);

  if (status.status() == net::URLRequestStatus::CANCELED) {
    if (abort_reason_ != DOWNLOAD_INTERRUPT_REASON_NONE) {
      // If a more specific interrupt reason was specified before the request
      // was explicitly cancelled, then use it.
      reason = abort_reason_;
    } else if (status.error() == net::ERR_ABORTED) {
      // CANCELED + ERR_ABORTED == something outside of the network
      // stack cancelled the request.  There aren't that many things that
      // could do this to a download request (whose lifetime is separated from
      // the tab from which it came).  We map this to USER_CANCELLED as the
      // case we know about (system suspend because of laptop close) corresponds
      // to a user action.
      // TODO(asanka): A lid close or other power event should result in an
      // interruption that doesn't discard the partial state, unlike
      // USER_CANCELLED. (https://crbug.com/166179)
      if (net::IsCertStatusError(request()->ssl_info().cert_status))
        reason = DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM;
      else
        reason = DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
    }
  }

  std::string accept_ranges;
  bool has_strong_validators = false;
  if (request()->response_headers()) {
    request()->response_headers()->EnumerateHeader(nullptr, "Accept-Ranges",
                                                   &accept_ranges);
    has_strong_validators =
        request()->response_headers()->HasStrongValidators();
  }
  RecordAcceptsRanges(accept_ranges, bytes_read_, has_strong_validators);
  RecordNetworkBlockage(base::TimeTicks::Now() - download_start_time_,
                        total_pause_time_);

  // Send the info down the stream.  Conditional is in case we get
  // OnResponseCompleted without OnResponseStarted.
  if (stream_writer_)
    stream_writer_->Close(reason);

  // If the error mapped to something unknown, record it so that
  // we can drill down.
  if (reason == DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Download.MapErrorNetworkFailed",
                                std::abs(status.error()));
  }

  stream_writer_.reset();  // We no longer need the stream.
  read_buffer_ = nullptr;

  if (started_)
    return;

  // OnResponseCompleted() called without OnResponseStarted(). This should only
  // happen when the request was aborted.
  DCHECK_NE(reason, DOWNLOAD_INTERRUPT_REASON_NONE);
  std::unique_ptr<DownloadCreateInfo> create_info =
      CreateDownloadCreateInfo(reason);
  std::unique_ptr<ByteStreamReader> empty_byte_stream;
  delegate_->OnStart(std::move(create_info), std::move(empty_byte_stream),
                     base::ResetAndReturn(&on_started_callback_));
}

void DownloadRequestCore::PauseRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ++pause_count_;
}

void DownloadRequestCore::ResumeRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_LT(0, pause_count_);

  --pause_count_;

  if (!was_deferred_)
    return;
  if (pause_count_ > 0)
    return;

  was_deferred_ = false;
  if (!last_stream_pause_time_.is_null()) {
    total_pause_time_ += (base::TimeTicks::Now() - last_stream_pause_time_);
    last_stream_pause_time_ = base::TimeTicks();
  }

  delegate_->OnReadyToRead();
}

std::string DownloadRequestCore::DebugString() const {
  return base::StringPrintf(
      "{"
      " this=%p "
      " url_ = "
      "\"%s\""
      " }",
      reinterpret_cast<const void*>(this),
      request() ? request()->url().spec().c_str() : "<NULL request>");
}

// static
DownloadInterruptReason DownloadRequestCore::HandleRequestStatus(
    const net::URLRequestStatus& status) {
  net::Error error_code = net::OK;
  if (!status.is_success()) {
    error_code = static_cast<net::Error>(status.error());  // Normal case.
    // Make sure that at least the fact of failure comes through.
    if (error_code == net::OK)
      error_code = net::ERR_FAILED;
  }

  // ERR_CONTENT_LENGTH_MISMATCH is allowed since a number of servers in the
  // wild close the connection too early by mistake. Other browsers - IE9,
  // Firefox 11.0, and Safari 5.1.4 - treat downloads as complete in both cases,
  // so we follow their lead.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH)
    error_code = net::OK;
  DownloadInterruptReason reason = ConvertNetErrorToInterruptReason(
      error_code, DOWNLOAD_INTERRUPT_FROM_NETWORK);

  return reason;
}

// static
DownloadInterruptReason DownloadRequestCore::HandleSuccessfulServerResponse(
    const net::HttpResponseHeaders& http_headers,
    DownloadSaveInfo* save_info) {
  switch (http_headers.response_code()) {
    case -1:  // Non-HTTP request.
    case net::HTTP_OK:
    case net::HTTP_NON_AUTHORITATIVE_INFORMATION:
    case net::HTTP_PARTIAL_CONTENT:
      // Expected successful codes.
      break;

    case net::HTTP_CREATED:
    case net::HTTP_ACCEPTED:
      // Per RFC 2616 the entity being transferred is metadata about the
      // resource at the target URL and not the resource at that URL (or the
      // resource that would be at the URL once processing is completed in the
      // case of HTTP_ACCEPTED). However, we currently don't have special
      // handling for these response and they are downloaded the same as a
      // regular response.
      break;

    case net::HTTP_NO_CONTENT:
    case net::HTTP_RESET_CONTENT:
    // These two status codes don't have an entity (or rather RFC 2616
    // requires that there be no entity). They are treated the same as the
    // resource not being found since there is no entity to download.

    case net::HTTP_NOT_FOUND:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
      break;

    case net::HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
      // Retry by downloading from the start automatically:
      // If we haven't received data when we get this error, we won't.
      return DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE;
      break;
    case net::HTTP_UNAUTHORIZED:
    case net::HTTP_PROXY_AUTHENTICATION_REQUIRED:
      // Server didn't authorize this request.
      return DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED;
      break;
    case net::HTTP_FORBIDDEN:
      // Server forbids access to this resource.
      return DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN;
      break;
    default:  // All other errors.
      // Redirection and informational codes should have been handled earlier
      // in the stack.
      DCHECK_NE(3, http_headers.response_code() / 100);
      DCHECK_NE(1, http_headers.response_code() / 100);
      return DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
  }

  if (save_info && save_info->offset > 0) {
    // The caller is expecting a partial response.

    if (http_headers.response_code() != net::HTTP_PARTIAL_CONTENT) {
      // Requested a partial range, but received the entire response.
      save_info->offset = 0;
      save_info->hash_of_partial_file.clear();
      save_info->hash_state.reset();
      return DOWNLOAD_INTERRUPT_REASON_NONE;
    }

    int64_t first_byte = -1;
    int64_t last_byte = -1;
    int64_t length = -1;
    if (!http_headers.GetContentRange(&first_byte, &last_byte, &length))
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    DCHECK_GE(first_byte, 0);

    if (first_byte != save_info->offset) {
      // The server returned a different range than the one we requested. Assume
      // the response is bad.
      //
      // In the future we should consider allowing offsets that are less than
      // the offset we've requested, since in theory we can truncate the partial
      // file at the offset and continue.
      return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;
    }

    return DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  if (http_headers.response_code() == net::HTTP_PARTIAL_CONTENT)
    return DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT;

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

}  // namespace content
