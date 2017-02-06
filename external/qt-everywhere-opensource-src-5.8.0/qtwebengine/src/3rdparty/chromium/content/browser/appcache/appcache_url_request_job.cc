// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_request_job.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

AppCacheURLRequestJob::AppCacheURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    AppCacheStorage* storage,
    AppCacheHost* host,
    bool is_main_resource,
    const OnPrepareToRestartCallback& restart_callback)
    : net::URLRequestJob(request, network_delegate),
      host_(host),
      storage_(storage),
      has_been_started_(false),
      has_been_killed_(false),
      delivery_type_(AWAITING_DELIVERY_ORDERS),
      cache_id_(kAppCacheNoCacheId),
      is_fallback_(false),
      is_main_resource_(is_main_resource),
      cache_entry_not_found_(false),
      on_prepare_to_restart_callback_(restart_callback),
      weak_factory_(this) {
  DCHECK(storage_);
}

AppCacheURLRequestJob::~AppCacheURLRequestJob() {
  if (storage_)
    storage_->CancelDelegateCallbacks(this);
}

void AppCacheURLRequestJob::DeliverAppCachedResponse(const GURL& manifest_url,
                                                     int64_t cache_id,
                                                     const AppCacheEntry& entry,
                                                     bool is_fallback) {
  DCHECK(!has_delivery_orders());
  DCHECK(entry.has_response_id());
  delivery_type_ = APPCACHED_DELIVERY;
  manifest_url_ = manifest_url;
  cache_id_ = cache_id;
  entry_ = entry;
  is_fallback_ = is_fallback;
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::DeliverNetworkResponse() {
  DCHECK(!has_delivery_orders());
  delivery_type_ = NETWORK_DELIVERY;
  storage_ = NULL;  // not needed
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::DeliverErrorResponse() {
  DCHECK(!has_delivery_orders());
  delivery_type_ = ERROR_DELIVERY;
  storage_ = NULL;  // not needed
  MaybeBeginDelivery();
}

base::WeakPtr<AppCacheURLRequestJob> AppCacheURLRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheURLRequestJob::MaybeBeginDelivery() {
  if (has_been_started() && has_delivery_orders()) {
    // Start asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&AppCacheURLRequestJob::BeginDelivery,
                              weak_factory_.GetWeakPtr()));
  }
}

void AppCacheURLRequestJob::BeginDelivery() {
  DCHECK(has_delivery_orders() && has_been_started());

  if (has_been_killed())
    return;

  switch (delivery_type_) {
    case NETWORK_DELIVERY:
      AppCacheHistograms::AddNetworkJobStartDelaySample(
          base::TimeTicks::Now() - start_time_tick_);
      // To fallthru to the network, we restart the request which will
      // cause a new job to be created to retrieve the resource from the
      // network. Our caller is responsible for arranging to not re-intercept
      // the same request.
      NotifyRestartRequired();
      break;

    case ERROR_DELIVERY:
      AppCacheHistograms::AddErrorJobStartDelaySample(
          base::TimeTicks::Now() - start_time_tick_);
      request()->net_log().AddEvent(
          net::NetLog::TYPE_APPCACHE_DELIVERING_ERROR_RESPONSE);
      NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                             net::ERR_FAILED));
      break;

    case APPCACHED_DELIVERY:
      if (entry_.IsExecutable()) {
        BeginExecutableHandlerDelivery();
        return;
      }
      AppCacheHistograms::AddAppCacheJobStartDelaySample(
          base::TimeTicks::Now() - start_time_tick_);
      request()->net_log().AddEvent(
          is_fallback_ ?
              net::NetLog::TYPE_APPCACHE_DELIVERING_FALLBACK_RESPONSE :
              net::NetLog::TYPE_APPCACHE_DELIVERING_CACHED_RESPONSE);
      storage_->LoadResponseInfo(manifest_url_, entry_.response_id(), this);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void AppCacheURLRequestJob::BeginExecutableHandlerDelivery() {
  DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      kEnableExecutableHandlers));
  if (!storage_->service()->handler_factory()) {
    BeginErrorDelivery("missing handler factory");
    return;
  }

  request()->net_log().AddEvent(
      net::NetLog::TYPE_APPCACHE_DELIVERING_EXECUTABLE_RESPONSE);

  // We defer job delivery until the executable handler is spun up and
  // provides a response. The sequence goes like this...
  //
  // 1. First we load the cache.
  // 2. Then if the handler is not spun up, we load the script resource which
  //    is needed to spin it up.
  // 3. Then we ask then we ask the handler to compute a response.
  // 4. Finally we deilver that response to the caller.
  storage_->LoadCache(cache_id_, this);
}

void AppCacheURLRequestJob::OnCacheLoaded(AppCache* cache, int64_t cache_id) {
  DCHECK_EQ(cache_id_, cache_id);
  DCHECK(!has_been_killed());

  if (!cache) {
    BeginErrorDelivery("cache load failed");
    return;
  }

  // Keep references to ensure they don't go out of scope until job completion.
  cache_ = cache;
  group_ = cache->owning_group();

  // If the handler is spun up, ask it to compute a response.
  AppCacheExecutableHandler* handler =
      cache->GetExecutableHandler(entry_.response_id());
  if (handler) {
    InvokeExecutableHandler(handler);
    return;
  }

  // Handler is not spun up yet, load the script resource to do that.
  // NOTE: This is not ideal since multiple jobs may be doing this,
  // concurrently but close enough for now, the first to load the script
  // will win.

  // Read the script data, truncating if its too large.
  // NOTE: we just issue one read and don't bother chaining if the resource
  // is very (very) large, close enough for now.
  const int64_t kLimit = 500 * 1000;
  handler_source_buffer_ = new net::GrowableIOBuffer();
  handler_source_buffer_->SetCapacity(kLimit);
  handler_source_reader_.reset(
      storage_->CreateResponseReader(manifest_url_, entry_.response_id()));
  handler_source_reader_->ReadData(
      handler_source_buffer_.get(),
      kLimit,
      base::Bind(&AppCacheURLRequestJob::OnExecutableSourceLoaded,
                 base::Unretained(this)));
}

void AppCacheURLRequestJob::OnExecutableSourceLoaded(int result) {
  DCHECK(!has_been_killed());
  handler_source_reader_.reset();
  if (result < 0) {
    BeginErrorDelivery("script source load failed");
    return;
  }

  handler_source_buffer_->SetCapacity(result);  // Free up some memory.

  AppCacheExecutableHandler* handler = cache_->GetOrCreateExecutableHandler(
      entry_.response_id(), handler_source_buffer_.get());
  handler_source_buffer_ = NULL;  // not needed anymore
  if (handler) {
    InvokeExecutableHandler(handler);
    return;
  }

  BeginErrorDelivery("factory failed to produce a handler");
}

void AppCacheURLRequestJob::InvokeExecutableHandler(
    AppCacheExecutableHandler* handler) {
  handler->HandleRequest(
      request(),
      base::Bind(&AppCacheURLRequestJob::OnExecutableResponseCallback,
                 weak_factory_.GetWeakPtr()));
}

void AppCacheURLRequestJob::OnExecutableResponseCallback(
    const AppCacheExecutableHandler::Response& response) {
  DCHECK(!has_been_killed());
  if (response.use_network) {
    delivery_type_ = NETWORK_DELIVERY;
    storage_ = NULL;
    BeginDelivery();
    return;
  }

  if (!response.cached_resource_url.is_empty()) {
    AppCacheEntry* entry_ptr = cache_->GetEntry(response.cached_resource_url);
    if (entry_ptr && !entry_ptr->IsExecutable()) {
      entry_ = *entry_ptr;
      BeginDelivery();
      return;
    }
  }

  if (!response.redirect_url.is_empty()) {
    // TODO(michaeln): playback a redirect
    // response_headers_(new HttpResponseHeaders(response_headers)),
    // fallthru for now to deliver an error
  }

  // Otherwise, return an error.
  BeginErrorDelivery("handler returned an invalid response");
}

void AppCacheURLRequestJob::BeginErrorDelivery(const char* message) {
  if (host_)
    host_->frontend()->OnLogMessage(host_->host_id(), APPCACHE_LOG_ERROR,
                                    message);
  delivery_type_ = ERROR_DELIVERY;
  storage_ = NULL;
  BeginDelivery();
}

void AppCacheURLRequestJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  DCHECK(is_delivering_appcache_response());
  if (response_info) {
    info_ = response_info;
    reader_.reset(
        storage_->CreateResponseReader(manifest_url_, entry_.response_id()));

    if (is_range_request())
      SetupRangeResponse();

    NotifyHeadersComplete();
  } else {
    if (storage_->service()->storage() == storage_) {
      // A resource that is expected to be in the appcache is missing.
      // See http://code.google.com/p/chromium/issues/detail?id=50657
      // Instead of failing the request, we restart the request. The retry
      // attempt will fallthru to the network instead of trying to load
      // from the appcache.
      storage_->service()->CheckAppCacheResponse(manifest_url_, cache_id_,
                                                 entry_.response_id());
      AppCacheHistograms::CountResponseRetrieval(
          false, is_main_resource_, manifest_url_.GetOrigin());
    }
    cache_entry_not_found_ = true;
    NotifyRestartRequired();
  }
}

const net::HttpResponseInfo* AppCacheURLRequestJob::http_info() const {
  if (!info_.get())
    return NULL;
  if (range_response_info_)
    return range_response_info_.get();
  return info_->http_response_info();
}

void AppCacheURLRequestJob::SetupRangeResponse() {
  DCHECK(is_range_request() && info_.get() && reader_.get() &&
         is_delivering_appcache_response());
  int resource_size = static_cast<int>(info_->response_data_size());
  if (resource_size < 0 || !range_requested_.ComputeBounds(resource_size)) {
    range_requested_ = net::HttpByteRange();
    return;
  }

  DCHECK(range_requested_.IsValid());
  int offset = static_cast<int>(range_requested_.first_byte_position());
  int length = static_cast<int>(range_requested_.last_byte_position() -
                                range_requested_.first_byte_position() + 1);

  // Tell the reader about the range to read.
  reader_->SetReadRange(offset, length);

  // Make a copy of the full response headers and fix them up
  // for the range we'll be returning.
  range_response_info_.reset(
      new net::HttpResponseInfo(*info_->http_response_info()));
  net::HttpResponseHeaders* headers = range_response_info_->headers.get();
  headers->UpdateWithNewRange(
      range_requested_, resource_size, true /* replace status line */);
}

void AppCacheURLRequestJob::OnReadComplete(int result) {
  DCHECK(is_delivering_appcache_response());
  if (result == 0) {
    AppCacheHistograms::CountResponseRetrieval(
        true, is_main_resource_, manifest_url_.GetOrigin());
  } else if (result < 0) {
    if (storage_->service()->storage() == storage_) {
      storage_->service()->CheckAppCacheResponse(manifest_url_, cache_id_,
                                                 entry_.response_id());
    }
    AppCacheHistograms::CountResponseRetrieval(
        false, is_main_resource_, manifest_url_.GetOrigin());
  }
  ReadRawDataComplete(result);
}

// net::URLRequestJob overrides ------------------------------------------------

void AppCacheURLRequestJob::Start() {
  DCHECK(!has_been_started());
  has_been_started_ = true;
  start_time_tick_ = base::TimeTicks::Now();
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::Kill() {
  if (!has_been_killed_) {
    has_been_killed_ = true;
    reader_.reset();
    handler_source_reader_.reset();
    if (storage_) {
      storage_->CancelDelegateCallbacks(this);
      storage_ = NULL;
    }
    host_ = NULL;
    info_ = NULL;
    cache_ = NULL;
    group_ = NULL;
    range_response_info_.reset();
    net::URLRequestJob::Kill();
    weak_factory_.InvalidateWeakPtrs();
  }
}

net::LoadState AppCacheURLRequestJob::GetLoadState() const {
  if (!has_been_started())
    return net::LOAD_STATE_IDLE;
  if (!has_delivery_orders())
    return net::LOAD_STATE_WAITING_FOR_APPCACHE;
  if (delivery_type_ != APPCACHED_DELIVERY)
    return net::LOAD_STATE_IDLE;
  if (!info_.get())
    return net::LOAD_STATE_WAITING_FOR_APPCACHE;
  if (reader_.get() && reader_->IsReadPending())
    return net::LOAD_STATE_READING_RESPONSE;
  return net::LOAD_STATE_IDLE;
}

bool AppCacheURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

bool AppCacheURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

void AppCacheURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int AppCacheURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

int AppCacheURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(is_delivering_appcache_response());
  DCHECK_NE(buf_size, 0);
  DCHECK(!reader_->IsReadPending());
  reader_->ReadData(buf, buf_size,
                    base::Bind(&AppCacheURLRequestJob::OnReadComplete,
                               base::Unretained(this)));
  return net::ERR_IO_PENDING;
}

net::HostPortPair AppCacheURLRequestJob::GetSocketAddress() const {
  if (!http_info())
    return net::HostPortPair();
  return http_info()->socket_address;
}

void AppCacheURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string value;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &value) ||
      !net::HttpUtil::ParseRangeHeader(value, &ranges)) {
    return;
  }

  // If multiple ranges are requested, we play dumb and
  // return the entire response with 200 OK.
  if (ranges.size() == 1U)
    range_requested_ = ranges[0];
}

void AppCacheURLRequestJob::NotifyRestartRequired() {
  on_prepare_to_restart_callback_.Run();
  URLRequestJob::NotifyRestartRequired();
}

}  // namespace content
