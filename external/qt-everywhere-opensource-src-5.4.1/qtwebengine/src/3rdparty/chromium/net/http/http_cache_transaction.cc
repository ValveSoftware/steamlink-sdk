// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache_transaction.h"

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/upload_data_stream.h"
#include "net/cert/cert_status_flags.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_transaction.h"
#include "net/http/http_util.h"
#include "net/http/partial_data.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

// From http://tools.ietf.org/html/draft-ietf-httpbis-p6-cache-21#section-6
//      a "non-error response" is one with a 2xx (Successful) or 3xx
//      (Redirection) status code.
bool NonErrorResponse(int status_code) {
  int status_code_range = status_code / 100;
  return status_code_range == 2 || status_code_range == 3;
}

// Error codes that will be considered indicative of a page being offline/
// unreachable for LOAD_FROM_CACHE_IF_OFFLINE.
bool IsOfflineError(int error) {
  return (error == net::ERR_NAME_NOT_RESOLVED ||
          error == net::ERR_INTERNET_DISCONNECTED ||
          error == net::ERR_ADDRESS_UNREACHABLE ||
          error == net::ERR_CONNECTION_TIMED_OUT);
}

// Enum for UMA, indicating the status (with regard to offline mode) of
// a particular request.
enum RequestOfflineStatus {
  // A cache transaction hit in cache (data was present and not stale)
  // and returned it.
  OFFLINE_STATUS_FRESH_CACHE,

  // A network request was required for a cache entry, and it succeeded.
  OFFLINE_STATUS_NETWORK_SUCCEEDED,

  // A network request was required for a cache entry, and it failed with
  // a non-offline error.
  OFFLINE_STATUS_NETWORK_FAILED,

  // A network request was required for a cache entry, it failed with an
  // offline error, and we could serve stale data if
  // LOAD_FROM_CACHE_IF_OFFLINE was set.
  OFFLINE_STATUS_DATA_AVAILABLE_OFFLINE,

  // A network request was required for a cache entry, it failed with
  // an offline error, and there was no servable data in cache (even
  // stale data).
  OFFLINE_STATUS_DATA_UNAVAILABLE_OFFLINE,

  OFFLINE_STATUS_MAX_ENTRIES
};

void RecordOfflineStatus(int load_flags, RequestOfflineStatus status) {
  // Restrict to main frame to keep statistics close to
  // "would have shown them something useful if offline mode was enabled".
  if (load_flags & net::LOAD_MAIN_FRAME) {
    UMA_HISTOGRAM_ENUMERATION("HttpCache.OfflineStatus", status,
                              OFFLINE_STATUS_MAX_ENTRIES);
  }
}

// TODO(rvargas): Remove once we get the data.
void RecordVaryHeaderHistogram(const net::HttpResponseInfo* response) {
  enum VaryType {
    VARY_NOT_PRESENT,
    VARY_UA,
    VARY_OTHER,
    VARY_MAX
  };
  VaryType vary = VARY_NOT_PRESENT;
  if (response->vary_data.is_valid()) {
    vary = VARY_OTHER;
    if (response->headers->HasHeaderValue("vary", "user-agent"))
      vary = VARY_UA;
  }
  UMA_HISTOGRAM_ENUMERATION("HttpCache.Vary", vary, VARY_MAX);
}

void RecordNoStoreHeaderHistogram(int load_flags,
                                  const net::HttpResponseInfo* response) {
  if (load_flags & net::LOAD_MAIN_FRAME) {
    UMA_HISTOGRAM_BOOLEAN(
        "Net.MainFrameNoStore",
        response->headers->HasHeaderValue("cache-control", "no-store"));
  }
}

}  // namespace

namespace net {

struct HeaderNameAndValue {
  const char* name;
  const char* value;
};

// If the request includes one of these request headers, then avoid caching
// to avoid getting confused.
static const HeaderNameAndValue kPassThroughHeaders[] = {
  { "if-unmodified-since", NULL },  // causes unexpected 412s
  { "if-match", NULL },             // causes unexpected 412s
  { "if-range", NULL },
  { NULL, NULL }
};

struct ValidationHeaderInfo {
  const char* request_header_name;
  const char* related_response_header_name;
};

static const ValidationHeaderInfo kValidationHeaders[] = {
  { "if-modified-since", "last-modified" },
  { "if-none-match", "etag" },
};

// If the request includes one of these request headers, then avoid reusing
// our cached copy if any.
static const HeaderNameAndValue kForceFetchHeaders[] = {
  { "cache-control", "no-cache" },
  { "pragma", "no-cache" },
  { NULL, NULL }
};

// If the request includes one of these request headers, then force our
// cached copy (if any) to be revalidated before reusing it.
static const HeaderNameAndValue kForceValidateHeaders[] = {
  { "cache-control", "max-age=0" },
  { NULL, NULL }
};

static bool HeaderMatches(const HttpRequestHeaders& headers,
                          const HeaderNameAndValue* search) {
  for (; search->name; ++search) {
    std::string header_value;
    if (!headers.GetHeader(search->name, &header_value))
      continue;

    if (!search->value)
      return true;

    HttpUtil::ValuesIterator v(header_value.begin(), header_value.end(), ',');
    while (v.GetNext()) {
      if (LowerCaseEqualsASCII(v.value_begin(), v.value_end(), search->value))
        return true;
    }
  }
  return false;
}

//-----------------------------------------------------------------------------

HttpCache::Transaction::Transaction(
    RequestPriority priority,
    HttpCache* cache)
    : next_state_(STATE_NONE),
      request_(NULL),
      priority_(priority),
      cache_(cache->GetWeakPtr()),
      entry_(NULL),
      new_entry_(NULL),
      new_response_(NULL),
      mode_(NONE),
      target_state_(STATE_NONE),
      reading_(false),
      invalid_range_(false),
      truncated_(false),
      is_sparse_(false),
      range_requested_(false),
      handling_206_(false),
      cache_pending_(false),
      done_reading_(false),
      vary_mismatch_(false),
      couldnt_conditionalize_request_(false),
      io_buf_len_(0),
      read_offset_(0),
      effective_load_flags_(0),
      write_len_(0),
      weak_factory_(this),
      io_callback_(base::Bind(&Transaction::OnIOComplete,
                              weak_factory_.GetWeakPtr())),
      transaction_pattern_(PATTERN_UNDEFINED),
      total_received_bytes_(0),
      websocket_handshake_stream_base_create_helper_(NULL) {
  COMPILE_ASSERT(HttpCache::Transaction::kNumValidationHeaders ==
                 arraysize(kValidationHeaders),
                 Invalid_number_of_validation_headers);
}

HttpCache::Transaction::~Transaction() {
  // We may have to issue another IO, but we should never invoke the callback_
  // after this point.
  callback_.Reset();

  if (cache_) {
    if (entry_) {
      bool cancel_request = reading_ && response_.headers;
      if (cancel_request) {
        if (partial_) {
          entry_->disk_entry->CancelSparseIO();
        } else {
          cancel_request &= (response_.headers->response_code() == 200);
        }
      }

      cache_->DoneWithEntry(entry_, this, cancel_request);
    } else if (cache_pending_) {
      cache_->RemovePendingTransaction(this);
    }
  }
}

int HttpCache::Transaction::WriteMetadata(IOBuffer* buf, int buf_len,
                                          const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(!callback.is_null());
  if (!cache_.get() || !entry_)
    return ERR_UNEXPECTED;

  // We don't need to track this operation for anything.
  // It could be possible to check if there is something already written and
  // avoid writing again (it should be the same, right?), but let's allow the
  // caller to "update" the contents with something new.
  return entry_->disk_entry->WriteData(kMetadataIndex, 0, buf, buf_len,
                                       callback, true);
}

bool HttpCache::Transaction::AddTruncatedFlag() {
  DCHECK(mode_ & WRITE || mode_ == NONE);

  // Don't set the flag for sparse entries.
  if (partial_.get() && !truncated_)
    return true;

  if (!CanResume(true))
    return false;

  // We may have received the whole resource already.
  if (done_reading_)
    return true;

  truncated_ = true;
  target_state_ = STATE_NONE;
  next_state_ = STATE_CACHE_WRITE_TRUNCATED_RESPONSE;
  DoLoop(OK);
  return true;
}

LoadState HttpCache::Transaction::GetWriterLoadState() const {
  if (network_trans_.get())
    return network_trans_->GetLoadState();
  if (entry_ || !request_)
    return LOAD_STATE_IDLE;
  return LOAD_STATE_WAITING_FOR_CACHE;
}

const BoundNetLog& HttpCache::Transaction::net_log() const {
  return net_log_;
}

int HttpCache::Transaction::Start(const HttpRequestInfo* request,
                                  const CompletionCallback& callback,
                                  const BoundNetLog& net_log) {
  DCHECK(request);
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());
  DCHECK(!reading_);
  DCHECK(!network_trans_.get());
  DCHECK(!entry_);

  if (!cache_.get())
    return ERR_UNEXPECTED;

  SetRequest(net_log, request);

  // We have to wait until the backend is initialized so we start the SM.
  next_state_ = STATE_GET_BACKEND;
  int rv = DoLoop(OK);

  // Setting this here allows us to check for the existence of a callback_ to
  // determine if we are still inside Start.
  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartIgnoringLastError(
    const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());

  if (!cache_.get())
    return ERR_UNEXPECTED;

  int rv = RestartNetworkRequest();

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartWithCertificate(
    X509Certificate* client_cert,
    const CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());

  if (!cache_.get())
    return ERR_UNEXPECTED;

  int rv = RestartNetworkRequestWithCertificate(client_cert);

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

int HttpCache::Transaction::RestartWithAuth(
    const AuthCredentials& credentials,
    const CompletionCallback& callback) {
  DCHECK(auth_response_.headers.get());
  DCHECK(!callback.is_null());

  // Ensure that we only have one asynchronous call at a time.
  DCHECK(callback_.is_null());

  if (!cache_.get())
    return ERR_UNEXPECTED;

  // Clear the intermediate response since we are going to start over.
  auth_response_ = HttpResponseInfo();

  int rv = RestartNetworkRequestWithAuth(credentials);

  if (rv == ERR_IO_PENDING)
    callback_ = callback;

  return rv;
}

bool HttpCache::Transaction::IsReadyToRestartForAuth() {
  if (!network_trans_.get())
    return false;
  return network_trans_->IsReadyToRestartForAuth();
}

int HttpCache::Transaction::Read(IOBuffer* buf, int buf_len,
                                 const CompletionCallback& callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);
  DCHECK(!callback.is_null());

  DCHECK(callback_.is_null());

  if (!cache_.get())
    return ERR_UNEXPECTED;

  // If we have an intermediate auth response at this point, then it means the
  // user wishes to read the network response (the error page).  If there is a
  // previous response in the cache then we should leave it intact.
  if (auth_response_.headers.get() && mode_ != NONE) {
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    DCHECK(mode_ & WRITE);
    DoneWritingToEntry(mode_ == READ_WRITE);
    mode_ = NONE;
  }

  reading_ = true;
  int rv;

  switch (mode_) {
    case READ_WRITE:
      DCHECK(partial_.get());
      if (!network_trans_.get()) {
        // We are just reading from the cache, but we may be writing later.
        rv = ReadFromEntry(buf, buf_len);
        break;
      }
    case NONE:
    case WRITE:
      DCHECK(network_trans_.get());
      rv = ReadFromNetwork(buf, buf_len);
      break;
    case READ:
      rv = ReadFromEntry(buf, buf_len);
      break;
    default:
      NOTREACHED();
      rv = ERR_FAILED;
  }

  if (rv == ERR_IO_PENDING) {
    DCHECK(callback_.is_null());
    callback_ = callback;
  }
  return rv;
}

void HttpCache::Transaction::StopCaching() {
  // We really don't know where we are now. Hopefully there is no operation in
  // progress, but nothing really prevents this method to be called after we
  // returned ERR_IO_PENDING. We cannot attempt to truncate the entry at this
  // point because we need the state machine for that (and even if we are really
  // free, that would be an asynchronous operation). In other words, keep the
  // entry how it is (it will be marked as truncated at destruction), and let
  // the next piece of code that executes know that we are now reading directly
  // from the net.
  // TODO(mmenke):  This doesn't release the lock on the cache entry, so a
  //                future request for the resource will be blocked on this one.
  //                Fix this.
  if (cache_.get() && entry_ && (mode_ & WRITE) && network_trans_.get() &&
      !is_sparse_ && !range_requested_) {
    mode_ = NONE;
  }
}

bool HttpCache::Transaction::GetFullRequestHeaders(
    HttpRequestHeaders* headers) const {
  if (network_trans_)
    return network_trans_->GetFullRequestHeaders(headers);

  // TODO(ttuttle): Read headers from cache.
  return false;
}

int64 HttpCache::Transaction::GetTotalReceivedBytes() const {
  int64 total_received_bytes = total_received_bytes_;
  if (network_trans_)
    total_received_bytes += network_trans_->GetTotalReceivedBytes();
  return total_received_bytes;
}

void HttpCache::Transaction::DoneReading() {
  if (cache_.get() && entry_) {
    DCHECK_NE(mode_, UPDATE);
    if (mode_ & WRITE) {
      DoneWritingToEntry(true);
    } else if (mode_ & READ) {
      // It is necessary to check mode_ & READ because it is possible
      // for mode_ to be NONE and entry_ non-NULL with a write entry
      // if StopCaching was called.
      cache_->DoneReadingFromEntry(entry_, this);
      entry_ = NULL;
    }
  }
}

const HttpResponseInfo* HttpCache::Transaction::GetResponseInfo() const {
  // Null headers means we encountered an error or haven't a response yet
  if (auth_response_.headers.get())
    return &auth_response_;
  return (response_.headers.get() || response_.ssl_info.cert.get() ||
          response_.cert_request_info.get())
             ? &response_
             : NULL;
}

LoadState HttpCache::Transaction::GetLoadState() const {
  LoadState state = GetWriterLoadState();
  if (state != LOAD_STATE_WAITING_FOR_CACHE)
    return state;

  if (cache_.get())
    return cache_->GetLoadStateForPendingTransaction(this);

  return LOAD_STATE_IDLE;
}

UploadProgress HttpCache::Transaction::GetUploadProgress() const {
  if (network_trans_.get())
    return network_trans_->GetUploadProgress();
  return final_upload_progress_;
}

void HttpCache::Transaction::SetQuicServerInfo(
    QuicServerInfo* quic_server_info) {}

bool HttpCache::Transaction::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  if (network_trans_)
    return network_trans_->GetLoadTimingInfo(load_timing_info);

  if (old_network_trans_load_timing_) {
    *load_timing_info = *old_network_trans_load_timing_;
    return true;
  }

  if (first_cache_access_since_.is_null())
    return false;

  // If the cache entry was opened, return that time.
  load_timing_info->send_start = first_cache_access_since_;
  // This time doesn't make much sense when reading from the cache, so just use
  // the same time as send_start.
  load_timing_info->send_end = first_cache_access_since_;
  return true;
}

void HttpCache::Transaction::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (network_trans_)
    network_trans_->SetPriority(priority_);
}

void HttpCache::Transaction::SetWebSocketHandshakeStreamCreateHelper(
    WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  websocket_handshake_stream_base_create_helper_ = create_helper;
  if (network_trans_)
    network_trans_->SetWebSocketHandshakeStreamCreateHelper(create_helper);
}

void HttpCache::Transaction::SetBeforeNetworkStartCallback(
    const BeforeNetworkStartCallback& callback) {
  DCHECK(!network_trans_);
  before_network_start_callback_ = callback;
}

int HttpCache::Transaction::ResumeNetworkStart() {
  if (network_trans_)
    return network_trans_->ResumeNetworkStart();
  return ERR_UNEXPECTED;
}

//-----------------------------------------------------------------------------

void HttpCache::Transaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  DCHECK(!callback_.is_null());

  read_buf_ = NULL;  // Release the buffer before invoking the callback.

  // Since Run may result in Read being called, clear callback_ up front.
  CompletionCallback c = callback_;
  callback_.Reset();
  c.Run(rv);
}

int HttpCache::Transaction::HandleResult(int rv) {
  DCHECK(rv != ERR_IO_PENDING);
  if (!callback_.is_null())
    DoCallback(rv);

  return rv;
}

// A few common patterns: (Foo* means Foo -> FooComplete)
//
// Not-cached entry:
//   Start():
//   GetBackend* -> InitEntry -> OpenEntry* -> CreateEntry* -> AddToEntry* ->
//   SendRequest* -> SuccessfulSendRequest -> OverwriteCachedResponse ->
//   CacheWriteResponse* -> TruncateCachedData* -> TruncateCachedMetadata* ->
//   PartialHeadersReceived
//
//   Read():
//   NetworkRead* -> CacheWriteData*
//
// Cached entry, no validation:
//   Start():
//   GetBackend* -> InitEntry -> OpenEntry* -> AddToEntry* -> CacheReadResponse*
//   -> BeginPartialCacheValidation() -> BeginCacheValidation()
//
//   Read():
//   CacheReadData*
//
// Cached entry, validation (304):
//   Start():
//   GetBackend* -> InitEntry -> OpenEntry* -> AddToEntry* -> CacheReadResponse*
//   -> BeginPartialCacheValidation() -> BeginCacheValidation() ->
//   SendRequest* -> SuccessfulSendRequest -> UpdateCachedResponse ->
//   CacheWriteResponse* -> UpdateCachedResponseComplete ->
//   OverwriteCachedResponse -> PartialHeadersReceived
//
//   Read():
//   CacheReadData*
//
// Cached entry, validation and replace (200):
//   Start():
//   GetBackend* -> InitEntry -> OpenEntry* -> AddToEntry* -> CacheReadResponse*
//   -> BeginPartialCacheValidation() -> BeginCacheValidation() ->
//   SendRequest* -> SuccessfulSendRequest -> OverwriteCachedResponse ->
//   CacheWriteResponse* -> DoTruncateCachedData* -> TruncateCachedMetadata* ->
//   PartialHeadersReceived
//
//   Read():
//   NetworkRead* -> CacheWriteData*
//
// Sparse entry, partially cached, byte range request:
//   Start():
//   GetBackend* -> InitEntry -> OpenEntry* -> AddToEntry* -> CacheReadResponse*
//   -> BeginPartialCacheValidation() -> CacheQueryData* ->
//   ValidateEntryHeadersAndContinue() -> StartPartialCacheValidation ->
//   CompletePartialCacheValidation -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> UpdateCachedResponse -> CacheWriteResponse* ->
//   UpdateCachedResponseComplete -> OverwriteCachedResponse ->
//   PartialHeadersReceived
//
//   Read() 1:
//   NetworkRead* -> CacheWriteData*
//
//   Read() 2:
//   NetworkRead* -> CacheWriteData* -> StartPartialCacheValidation ->
//   CompletePartialCacheValidation -> CacheReadData* ->
//
//   Read() 3:
//   CacheReadData* -> StartPartialCacheValidation ->
//   CompletePartialCacheValidation -> BeginCacheValidation() -> SendRequest* ->
//   SuccessfulSendRequest -> UpdateCachedResponse* -> OverwriteCachedResponse
//   -> PartialHeadersReceived -> NetworkRead* -> CacheWriteData*
//
int HttpCache::Transaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_GET_BACKEND:
        DCHECK_EQ(OK, rv);
        rv = DoGetBackend();
        break;
      case STATE_GET_BACKEND_COMPLETE:
        rv = DoGetBackendComplete(rv);
        break;
      case STATE_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSendRequest();
        break;
      case STATE_SEND_REQUEST_COMPLETE:
        rv = DoSendRequestComplete(rv);
        break;
      case STATE_SUCCESSFUL_SEND_REQUEST:
        DCHECK_EQ(OK, rv);
        rv = DoSuccessfulSendRequest();
        break;
      case STATE_NETWORK_READ:
        DCHECK_EQ(OK, rv);
        rv = DoNetworkRead();
        break;
      case STATE_NETWORK_READ_COMPLETE:
        rv = DoNetworkReadComplete(rv);
        break;
      case STATE_INIT_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoInitEntry();
        break;
      case STATE_OPEN_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoOpenEntry();
        break;
      case STATE_OPEN_ENTRY_COMPLETE:
        rv = DoOpenEntryComplete(rv);
        break;
      case STATE_CREATE_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoCreateEntry();
        break;
      case STATE_CREATE_ENTRY_COMPLETE:
        rv = DoCreateEntryComplete(rv);
        break;
      case STATE_DOOM_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoDoomEntry();
        break;
      case STATE_DOOM_ENTRY_COMPLETE:
        rv = DoDoomEntryComplete(rv);
        break;
      case STATE_ADD_TO_ENTRY:
        DCHECK_EQ(OK, rv);
        rv = DoAddToEntry();
        break;
      case STATE_ADD_TO_ENTRY_COMPLETE:
        rv = DoAddToEntryComplete(rv);
        break;
      case STATE_START_PARTIAL_CACHE_VALIDATION:
        DCHECK_EQ(OK, rv);
        rv = DoStartPartialCacheValidation();
        break;
      case STATE_COMPLETE_PARTIAL_CACHE_VALIDATION:
        rv = DoCompletePartialCacheValidation(rv);
        break;
      case STATE_UPDATE_CACHED_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoUpdateCachedResponse();
        break;
      case STATE_UPDATE_CACHED_RESPONSE_COMPLETE:
        rv = DoUpdateCachedResponseComplete(rv);
        break;
      case STATE_OVERWRITE_CACHED_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoOverwriteCachedResponse();
        break;
      case STATE_TRUNCATE_CACHED_DATA:
        DCHECK_EQ(OK, rv);
        rv = DoTruncateCachedData();
        break;
      case STATE_TRUNCATE_CACHED_DATA_COMPLETE:
        rv = DoTruncateCachedDataComplete(rv);
        break;
      case STATE_TRUNCATE_CACHED_METADATA:
        DCHECK_EQ(OK, rv);
        rv = DoTruncateCachedMetadata();
        break;
      case STATE_TRUNCATE_CACHED_METADATA_COMPLETE:
        rv = DoTruncateCachedMetadataComplete(rv);
        break;
      case STATE_PARTIAL_HEADERS_RECEIVED:
        DCHECK_EQ(OK, rv);
        rv = DoPartialHeadersReceived();
        break;
      case STATE_CACHE_READ_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheReadResponse();
        break;
      case STATE_CACHE_READ_RESPONSE_COMPLETE:
        rv = DoCacheReadResponseComplete(rv);
        break;
      case STATE_CACHE_WRITE_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheWriteResponse();
        break;
      case STATE_CACHE_WRITE_TRUNCATED_RESPONSE:
        DCHECK_EQ(OK, rv);
        rv = DoCacheWriteTruncatedResponse();
        break;
      case STATE_CACHE_WRITE_RESPONSE_COMPLETE:
        rv = DoCacheWriteResponseComplete(rv);
        break;
      case STATE_CACHE_READ_METADATA:
        DCHECK_EQ(OK, rv);
        rv = DoCacheReadMetadata();
        break;
      case STATE_CACHE_READ_METADATA_COMPLETE:
        rv = DoCacheReadMetadataComplete(rv);
        break;
      case STATE_CACHE_QUERY_DATA:
        DCHECK_EQ(OK, rv);
        rv = DoCacheQueryData();
        break;
      case STATE_CACHE_QUERY_DATA_COMPLETE:
        rv = DoCacheQueryDataComplete(rv);
        break;
      case STATE_CACHE_READ_DATA:
        DCHECK_EQ(OK, rv);
        rv = DoCacheReadData();
        break;
      case STATE_CACHE_READ_DATA_COMPLETE:
        rv = DoCacheReadDataComplete(rv);
        break;
      case STATE_CACHE_WRITE_DATA:
        rv = DoCacheWriteData(rv);
        break;
      case STATE_CACHE_WRITE_DATA_COMPLETE:
        rv = DoCacheWriteDataComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  if (rv != ERR_IO_PENDING)
    HandleResult(rv);

  return rv;
}

int HttpCache::Transaction::DoGetBackend() {
  cache_pending_ = true;
  next_state_ = STATE_GET_BACKEND_COMPLETE;
  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_GET_BACKEND);
  return cache_->GetBackendForTransaction(this);
}

int HttpCache::Transaction::DoGetBackendComplete(int result) {
  DCHECK(result == OK || result == ERR_FAILED);
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_GET_BACKEND,
                                    result);
  cache_pending_ = false;

  if (!ShouldPassThrough()) {
    cache_key_ = cache_->GenerateCacheKey(request_);

    // Requested cache access mode.
    if (effective_load_flags_ & LOAD_ONLY_FROM_CACHE) {
      mode_ = READ;
    } else if (effective_load_flags_ & LOAD_BYPASS_CACHE) {
      mode_ = WRITE;
    } else {
      mode_ = READ_WRITE;
    }

    // Downgrade to UPDATE if the request has been externally conditionalized.
    if (external_validation_.initialized) {
      if (mode_ & WRITE) {
        // Strip off the READ_DATA bit (and maybe add back a READ_META bit
        // in case READ was off).
        mode_ = UPDATE;
      } else {
        mode_ = NONE;
      }
    }
  }

  // Use PUT and DELETE only to invalidate existing stored entries.
  if ((request_->method == "PUT" || request_->method == "DELETE") &&
      mode_ != READ_WRITE && mode_ != WRITE) {
    mode_ = NONE;
  }

  // If must use cache, then we must fail.  This can happen for back/forward
  // navigations to a page generated via a form post.
  if (!(mode_ & READ) && effective_load_flags_ & LOAD_ONLY_FROM_CACHE)
    return ERR_CACHE_MISS;

  if (mode_ == NONE) {
    if (partial_.get()) {
      partial_->RestoreHeaders(&custom_request_->extra_headers);
      partial_.reset();
    }
    next_state_ = STATE_SEND_REQUEST;
  } else {
    next_state_ = STATE_INIT_ENTRY;
  }

  // This is only set if we have something to do with the response.
  range_requested_ = (partial_.get() != NULL);

  return OK;
}

int HttpCache::Transaction::DoSendRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(!network_trans_.get());

  send_request_since_ = TimeTicks::Now();

  // Create a network transaction.
  int rv = cache_->network_layer_->CreateTransaction(priority_,
                                                     &network_trans_);
  if (rv != OK)
    return rv;
  network_trans_->SetBeforeNetworkStartCallback(before_network_start_callback_);

  // Old load timing information, if any, is now obsolete.
  old_network_trans_load_timing_.reset();

  if (websocket_handshake_stream_base_create_helper_)
    network_trans_->SetWebSocketHandshakeStreamCreateHelper(
        websocket_handshake_stream_base_create_helper_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  rv = network_trans_->Start(request_, io_callback_, net_log_);
  return rv;
}

int HttpCache::Transaction::DoSendRequestComplete(int result) {
  if (!cache_.get())
    return ERR_UNEXPECTED;

  // If requested, and we have a readable cache entry, and we have
  // an error indicating that we're offline as opposed to in contact
  // with a bad server, read from cache anyway.
  if (IsOfflineError(result)) {
    if (mode_ == READ_WRITE && entry_ && !partial_) {
      RecordOfflineStatus(effective_load_flags_,
                          OFFLINE_STATUS_DATA_AVAILABLE_OFFLINE);
      if (effective_load_flags_ & LOAD_FROM_CACHE_IF_OFFLINE) {
        UpdateTransactionPattern(PATTERN_NOT_COVERED);
        response_.server_data_unavailable = true;
        return SetupEntryForRead();
      }
    } else {
      RecordOfflineStatus(effective_load_flags_,
                          OFFLINE_STATUS_DATA_UNAVAILABLE_OFFLINE);
    }
  } else {
    RecordOfflineStatus(effective_load_flags_,
                        (result == OK ? OFFLINE_STATUS_NETWORK_SUCCEEDED :
                                        OFFLINE_STATUS_NETWORK_FAILED));
  }

  // If we tried to conditionalize the request and failed, we know
  // we won't be reading from the cache after this point.
  if (couldnt_conditionalize_request_)
    mode_ = WRITE;

  if (result == OK) {
    next_state_ = STATE_SUCCESSFUL_SEND_REQUEST;
    return OK;
  }

  // Do not record requests that have network errors or restarts.
  UpdateTransactionPattern(PATTERN_NOT_COVERED);
  if (IsCertificateError(result)) {
    const HttpResponseInfo* response = network_trans_->GetResponseInfo();
    // If we get a certificate error, then there is a certificate in ssl_info,
    // so GetResponseInfo() should never return NULL here.
    DCHECK(response);
    response_.ssl_info = response->ssl_info;
  } else if (result == ERR_SSL_CLIENT_AUTH_CERT_NEEDED) {
    const HttpResponseInfo* response = network_trans_->GetResponseInfo();
    DCHECK(response);
    response_.cert_request_info = response->cert_request_info;
  } else if (response_.was_cached) {
    DoneWritingToEntry(true);
  }
  return result;
}

// We received the response headers and there is no error.
int HttpCache::Transaction::DoSuccessfulSendRequest() {
  DCHECK(!new_response_);
  const HttpResponseInfo* new_response = network_trans_->GetResponseInfo();
  bool authentication_failure = false;

  if (new_response->headers->response_code() == 401 ||
      new_response->headers->response_code() == 407) {
    auth_response_ = *new_response;
    if (!reading_)
      return OK;

    // We initiated a second request the caller doesn't know about. We should be
    // able to authenticate this request because we should have authenticated
    // this URL moments ago.
    if (IsReadyToRestartForAuth()) {
      DCHECK(!response_.auth_challenge.get());
      next_state_ = STATE_SEND_REQUEST_COMPLETE;
      // In theory we should check to see if there are new cookies, but there
      // is no way to do that from here.
      return network_trans_->RestartWithAuth(AuthCredentials(), io_callback_);
    }

    // We have to perform cleanup at this point so that at least the next
    // request can succeed.
    authentication_failure = true;
    if (entry_)
      DoomPartialEntry(false);
    mode_ = NONE;
    partial_.reset();
  }

  new_response_ = new_response;
  if (authentication_failure ||
      (!ValidatePartialResponse() && !auth_response_.headers.get())) {
    // Something went wrong with this request and we have to restart it.
    // If we have an authentication response, we are exposed to weird things
    // hapenning if the user cancels the authentication before we receive
    // the new response.
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    response_ = HttpResponseInfo();
    ResetNetworkTransaction();
    new_response_ = NULL;
    next_state_ = STATE_SEND_REQUEST;
    return OK;
  }

  if (handling_206_ && mode_ == READ_WRITE && !truncated_ && !is_sparse_) {
    // We have stored the full entry, but it changed and the server is
    // sending a range. We have to delete the old entry.
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    DoneWritingToEntry(false);
  }

  if (mode_ == WRITE &&
      transaction_pattern_ != PATTERN_ENTRY_CANT_CONDITIONALIZE) {
    UpdateTransactionPattern(PATTERN_ENTRY_NOT_CACHED);
  }

  if (mode_ == WRITE &&
      (request_->method == "PUT" || request_->method == "DELETE")) {
    if (NonErrorResponse(new_response->headers->response_code())) {
      int ret = cache_->DoomEntry(cache_key_, NULL);
      DCHECK_EQ(OK, ret);
    }
    cache_->DoneWritingToEntry(entry_, true);
    entry_ = NULL;
    mode_ = NONE;
  }

  if (request_->method == "POST" &&
      NonErrorResponse(new_response->headers->response_code())) {
    cache_->DoomMainEntryForUrl(request_->url);
  }

  RecordVaryHeaderHistogram(new_response);
  RecordNoStoreHeaderHistogram(request_->load_flags, new_response);

  if (new_response_->headers->response_code() == 416 &&
      (request_->method == "GET" || request_->method == "POST")) {
    // If there is an ective entry it may be destroyed with this transaction.
    response_ = *new_response_;
    return OK;
  }

  // Are we expecting a response to a conditional query?
  if (mode_ == READ_WRITE || mode_ == UPDATE) {
    if (new_response->headers->response_code() == 304 || handling_206_) {
      UpdateTransactionPattern(PATTERN_ENTRY_VALIDATED);
      next_state_ = STATE_UPDATE_CACHED_RESPONSE;
      return OK;
    }
    UpdateTransactionPattern(PATTERN_ENTRY_UPDATED);
    mode_ = WRITE;
  }

  next_state_ = STATE_OVERWRITE_CACHED_RESPONSE;
  return OK;
}

int HttpCache::Transaction::DoNetworkRead() {
  next_state_ = STATE_NETWORK_READ_COMPLETE;
  return network_trans_->Read(read_buf_.get(), io_buf_len_, io_callback_);
}

int HttpCache::Transaction::DoNetworkReadComplete(int result) {
  DCHECK(mode_ & WRITE || mode_ == NONE);

  if (!cache_.get())
    return ERR_UNEXPECTED;

  // If there is an error or we aren't saving the data, we are done; just wait
  // until the destructor runs to see if we can keep the data.
  if (mode_ == NONE || result < 0)
    return result;

  next_state_ = STATE_CACHE_WRITE_DATA;
  return result;
}

int HttpCache::Transaction::DoInitEntry() {
  DCHECK(!new_entry_);

  if (!cache_.get())
    return ERR_UNEXPECTED;

  if (mode_ == WRITE) {
    next_state_ = STATE_DOOM_ENTRY;
    return OK;
  }

  next_state_ = STATE_OPEN_ENTRY;
  return OK;
}

int HttpCache::Transaction::DoOpenEntry() {
  DCHECK(!new_entry_);
  next_state_ = STATE_OPEN_ENTRY_COMPLETE;
  cache_pending_ = true;
  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY);
  first_cache_access_since_ = TimeTicks::Now();
  return cache_->OpenEntry(cache_key_, &new_entry_, this);
}

int HttpCache::Transaction::DoOpenEntryComplete(int result) {
  // It is important that we go to STATE_ADD_TO_ENTRY whenever the result is
  // OK, otherwise the cache will end up with an active entry without any
  // transaction attached.
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_OPEN_ENTRY, result);
  cache_pending_ = false;
  if (result == OK) {
    next_state_ = STATE_ADD_TO_ENTRY;
    return OK;
  }

  if (result == ERR_CACHE_RACE) {
    next_state_ = STATE_INIT_ENTRY;
    return OK;
  }

  if (request_->method == "PUT" || request_->method == "DELETE") {
    DCHECK(mode_ == READ_WRITE || mode_ == WRITE);
    mode_ = NONE;
    next_state_ = STATE_SEND_REQUEST;
    return OK;
  }

  if (mode_ == READ_WRITE) {
    mode_ = WRITE;
    next_state_ = STATE_CREATE_ENTRY;
    return OK;
  }
  if (mode_ == UPDATE) {
    // There is no cache entry to update; proceed without caching.
    mode_ = NONE;
    next_state_ = STATE_SEND_REQUEST;
    return OK;
  }
  if (cache_->mode() == PLAYBACK)
    DVLOG(1) << "Playback Cache Miss: " << request_->url;

  // The entry does not exist, and we are not permitted to create a new entry,
  // so we must fail.
  return ERR_CACHE_MISS;
}

int HttpCache::Transaction::DoCreateEntry() {
  DCHECK(!new_entry_);
  next_state_ = STATE_CREATE_ENTRY_COMPLETE;
  cache_pending_ = true;
  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY);
  return cache_->CreateEntry(cache_key_, &new_entry_, this);
}

int HttpCache::Transaction::DoCreateEntryComplete(int result) {
  // It is important that we go to STATE_ADD_TO_ENTRY whenever the result is
  // OK, otherwise the cache will end up with an active entry without any
  // transaction attached.
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_CREATE_ENTRY,
                                    result);
  cache_pending_ = false;
  next_state_ = STATE_ADD_TO_ENTRY;

  if (result == ERR_CACHE_RACE) {
    next_state_ = STATE_INIT_ENTRY;
    return OK;
  }

  if (result == OK) {
    UMA_HISTOGRAM_BOOLEAN("HttpCache.OpenToCreateRace", false);
  } else {
    UMA_HISTOGRAM_BOOLEAN("HttpCache.OpenToCreateRace", true);
    // We have a race here: Maybe we failed to open the entry and decided to
    // create one, but by the time we called create, another transaction already
    // created the entry. If we want to eliminate this issue, we need an atomic
    // OpenOrCreate() method exposed by the disk cache.
    DLOG(WARNING) << "Unable to create cache entry";
    mode_ = NONE;
    if (partial_.get())
      partial_->RestoreHeaders(&custom_request_->extra_headers);
    next_state_ = STATE_SEND_REQUEST;
  }
  return OK;
}

int HttpCache::Transaction::DoDoomEntry() {
  next_state_ = STATE_DOOM_ENTRY_COMPLETE;
  cache_pending_ = true;
  if (first_cache_access_since_.is_null())
    first_cache_access_since_ = TimeTicks::Now();
  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_DOOM_ENTRY);
  return cache_->DoomEntry(cache_key_, this);
}

int HttpCache::Transaction::DoDoomEntryComplete(int result) {
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_DOOM_ENTRY, result);
  next_state_ = STATE_CREATE_ENTRY;
  cache_pending_ = false;
  if (result == ERR_CACHE_RACE)
    next_state_ = STATE_INIT_ENTRY;
  return OK;
}

int HttpCache::Transaction::DoAddToEntry() {
  DCHECK(new_entry_);
  cache_pending_ = true;
  next_state_ = STATE_ADD_TO_ENTRY_COMPLETE;
  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY);
  DCHECK(entry_lock_waiting_since_.is_null());
  entry_lock_waiting_since_ = TimeTicks::Now();
  return cache_->AddTransactionToEntry(new_entry_, this);
}

int HttpCache::Transaction::DoAddToEntryComplete(int result) {
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_ADD_TO_ENTRY,
                                    result);
  const TimeDelta entry_lock_wait =
      TimeTicks::Now() - entry_lock_waiting_since_;
  UMA_HISTOGRAM_TIMES("HttpCache.EntryLockWait", entry_lock_wait);

  entry_lock_waiting_since_ = TimeTicks();
  DCHECK(new_entry_);
  cache_pending_ = false;

  if (result == OK)
    entry_ = new_entry_;

  // If there is a failure, the cache should have taken care of new_entry_.
  new_entry_ = NULL;

  if (result == ERR_CACHE_RACE) {
    next_state_ = STATE_INIT_ENTRY;
    return OK;
  }

  if (result != OK) {
    NOTREACHED();
    return result;
  }

  if (mode_ == WRITE) {
    if (partial_.get())
      partial_->RestoreHeaders(&custom_request_->extra_headers);
    next_state_ = STATE_SEND_REQUEST;
  } else {
    // We have to read the headers from the cached entry.
    DCHECK(mode_ & READ_META);
    next_state_ = STATE_CACHE_READ_RESPONSE;
  }
  return OK;
}

// We may end up here multiple times for a given request.
int HttpCache::Transaction::DoStartPartialCacheValidation() {
  if (mode_ == NONE)
    return OK;

  next_state_ = STATE_COMPLETE_PARTIAL_CACHE_VALIDATION;
  return partial_->ShouldValidateCache(entry_->disk_entry, io_callback_);
}

int HttpCache::Transaction::DoCompletePartialCacheValidation(int result) {
  if (!result) {
    // This is the end of the request.
    if (mode_ & WRITE) {
      DoneWritingToEntry(true);
    } else {
      cache_->DoneReadingFromEntry(entry_, this);
      entry_ = NULL;
    }
    return result;
  }

  if (result < 0)
    return result;

  partial_->PrepareCacheValidation(entry_->disk_entry,
                                   &custom_request_->extra_headers);

  if (reading_ && partial_->IsCurrentRangeCached()) {
    next_state_ = STATE_CACHE_READ_DATA;
    return OK;
  }

  return BeginCacheValidation();
}

// We received 304 or 206 and we want to update the cached response headers.
int HttpCache::Transaction::DoUpdateCachedResponse() {
  next_state_ = STATE_UPDATE_CACHED_RESPONSE_COMPLETE;
  int rv = OK;
  // Update cached response based on headers in new_response.
  // TODO(wtc): should we update cached certificate (response_.ssl_info), too?
  response_.headers->Update(*new_response_->headers.get());
  response_.response_time = new_response_->response_time;
  response_.request_time = new_response_->request_time;
  response_.network_accessed = new_response_->network_accessed;

  if (response_.headers->HasHeaderValue("cache-control", "no-store")) {
    if (!entry_->doomed) {
      int ret = cache_->DoomEntry(cache_key_, NULL);
      DCHECK_EQ(OK, ret);
    }
  } else {
    // If we are already reading, we already updated the headers for this
    // request; doing it again will change Content-Length.
    if (!reading_) {
      target_state_ = STATE_UPDATE_CACHED_RESPONSE_COMPLETE;
      next_state_ = STATE_CACHE_WRITE_RESPONSE;
      rv = OK;
    }
  }
  return rv;
}

int HttpCache::Transaction::DoUpdateCachedResponseComplete(int result) {
  if (mode_ == UPDATE) {
    DCHECK(!handling_206_);
    // We got a "not modified" response and already updated the corresponding
    // cache entry above.
    //
    // By closing the cached entry now, we make sure that the 304 rather than
    // the cached 200 response, is what will be returned to the user.
    DoneWritingToEntry(true);
  } else if (entry_ && !handling_206_) {
    DCHECK_EQ(READ_WRITE, mode_);
    if (!partial_.get() || partial_->IsLastRange()) {
      cache_->ConvertWriterToReader(entry_);
      mode_ = READ;
    }
    // We no longer need the network transaction, so destroy it.
    final_upload_progress_ = network_trans_->GetUploadProgress();
    ResetNetworkTransaction();
  } else if (entry_ && handling_206_ && truncated_ &&
             partial_->initial_validation()) {
    // We just finished the validation of a truncated entry, and the server
    // is willing to resume the operation. Now we go back and start serving
    // the first part to the user.
    ResetNetworkTransaction();
    new_response_ = NULL;
    next_state_ = STATE_START_PARTIAL_CACHE_VALIDATION;
    partial_->SetRangeToStartDownload();
    return OK;
  }
  next_state_ = STATE_OVERWRITE_CACHED_RESPONSE;
  return OK;
}

int HttpCache::Transaction::DoOverwriteCachedResponse() {
  if (mode_ & READ) {
    next_state_ = STATE_PARTIAL_HEADERS_RECEIVED;
    return OK;
  }

  // We change the value of Content-Length for partial content.
  if (handling_206_ && partial_.get())
    partial_->FixContentLength(new_response_->headers.get());

  response_ = *new_response_;

  if (handling_206_ && !CanResume(false)) {
    // There is no point in storing this resource because it will never be used.
    DoneWritingToEntry(false);
    if (partial_.get())
      partial_->FixResponseHeaders(response_.headers.get(), true);
    next_state_ = STATE_PARTIAL_HEADERS_RECEIVED;
    return OK;
  }

  target_state_ = STATE_TRUNCATE_CACHED_DATA;
  next_state_ = truncated_ ? STATE_CACHE_WRITE_TRUNCATED_RESPONSE :
                             STATE_CACHE_WRITE_RESPONSE;
  return OK;
}

int HttpCache::Transaction::DoTruncateCachedData() {
  next_state_ = STATE_TRUNCATE_CACHED_DATA_COMPLETE;
  if (!entry_)
    return OK;
  if (net_log_.IsLogging())
    net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_WRITE_DATA);
  // Truncate the stream.
  return WriteToEntry(kResponseContentIndex, 0, NULL, 0, io_callback_);
}

int HttpCache::Transaction::DoTruncateCachedDataComplete(int result) {
  if (entry_) {
    if (net_log_.IsLogging()) {
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_WRITE_DATA,
                                        result);
    }
  }

  next_state_ = STATE_TRUNCATE_CACHED_METADATA;
  return OK;
}

int HttpCache::Transaction::DoTruncateCachedMetadata() {
  next_state_ = STATE_TRUNCATE_CACHED_METADATA_COMPLETE;
  if (!entry_)
    return OK;

  if (net_log_.IsLogging())
    net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_WRITE_INFO);
  return WriteToEntry(kMetadataIndex, 0, NULL, 0, io_callback_);
}

int HttpCache::Transaction::DoTruncateCachedMetadataComplete(int result) {
  if (entry_) {
    if (net_log_.IsLogging()) {
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_WRITE_INFO,
                                        result);
    }
  }

  next_state_ = STATE_PARTIAL_HEADERS_RECEIVED;
  return OK;
}

int HttpCache::Transaction::DoPartialHeadersReceived() {
  new_response_ = NULL;
  if (entry_ && !partial_.get() &&
      entry_->disk_entry->GetDataSize(kMetadataIndex))
    next_state_ = STATE_CACHE_READ_METADATA;

  if (!partial_.get())
    return OK;

  if (reading_) {
    if (network_trans_.get()) {
      next_state_ = STATE_NETWORK_READ;
    } else {
      next_state_ = STATE_CACHE_READ_DATA;
    }
  } else if (mode_ != NONE) {
    // We are about to return the headers for a byte-range request to the user,
    // so let's fix them.
    partial_->FixResponseHeaders(response_.headers.get(), true);
  }
  return OK;
}

int HttpCache::Transaction::DoCacheReadResponse() {
  DCHECK(entry_);
  next_state_ = STATE_CACHE_READ_RESPONSE_COMPLETE;

  io_buf_len_ = entry_->disk_entry->GetDataSize(kResponseInfoIndex);
  read_buf_ = new IOBuffer(io_buf_len_);

  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_READ_INFO);
  return entry_->disk_entry->ReadData(kResponseInfoIndex, 0, read_buf_.get(),
                                      io_buf_len_, io_callback_);
}

int HttpCache::Transaction::DoCacheReadResponseComplete(int result) {
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_READ_INFO, result);
  if (result != io_buf_len_ ||
      !HttpCache::ParseResponseInfo(read_buf_->data(), io_buf_len_,
                                    &response_, &truncated_)) {
    return OnCacheReadError(result, true);
  }

  // Some resources may have slipped in as truncated when they're not.
  int current_size = entry_->disk_entry->GetDataSize(kResponseContentIndex);
  if (response_.headers->GetContentLength() == current_size)
    truncated_ = false;

  // We now have access to the cache entry.
  //
  //  o if we are a reader for the transaction, then we can start reading the
  //    cache entry.
  //
  //  o if we can read or write, then we should check if the cache entry needs
  //    to be validated and then issue a network request if needed or just read
  //    from the cache if the cache entry is already valid.
  //
  //  o if we are set to UPDATE, then we are handling an externally
  //    conditionalized request (if-modified-since / if-none-match). We check
  //    if the request headers define a validation request.
  //
  switch (mode_) {
    case READ:
      UpdateTransactionPattern(PATTERN_ENTRY_USED);
      result = BeginCacheRead();
      break;
    case READ_WRITE:
      result = BeginPartialCacheValidation();
      break;
    case UPDATE:
      result = BeginExternallyConditionalizedRequest();
      break;
    case WRITE:
    default:
      NOTREACHED();
      result = ERR_FAILED;
  }
  return result;
}

int HttpCache::Transaction::DoCacheWriteResponse() {
  if (entry_) {
    if (net_log_.IsLogging())
      net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_WRITE_INFO);
  }
  return WriteResponseInfoToEntry(false);
}

int HttpCache::Transaction::DoCacheWriteTruncatedResponse() {
  if (entry_) {
    if (net_log_.IsLogging())
      net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_WRITE_INFO);
  }
  return WriteResponseInfoToEntry(true);
}

int HttpCache::Transaction::DoCacheWriteResponseComplete(int result) {
  next_state_ = target_state_;
  target_state_ = STATE_NONE;
  if (!entry_)
    return OK;
  if (net_log_.IsLogging()) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_WRITE_INFO,
                                      result);
  }

  // Balance the AddRef from WriteResponseInfoToEntry.
  if (result != io_buf_len_) {
    DLOG(ERROR) << "failed to write response info to cache";
    DoneWritingToEntry(false);
  }
  return OK;
}

int HttpCache::Transaction::DoCacheReadMetadata() {
  DCHECK(entry_);
  DCHECK(!response_.metadata.get());
  next_state_ = STATE_CACHE_READ_METADATA_COMPLETE;

  response_.metadata =
      new IOBufferWithSize(entry_->disk_entry->GetDataSize(kMetadataIndex));

  net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_READ_INFO);
  return entry_->disk_entry->ReadData(kMetadataIndex, 0,
                                      response_.metadata.get(),
                                      response_.metadata->size(),
                                      io_callback_);
}

int HttpCache::Transaction::DoCacheReadMetadataComplete(int result) {
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_READ_INFO, result);
  if (result != response_.metadata->size())
    return OnCacheReadError(result, false);
  return OK;
}

int HttpCache::Transaction::DoCacheQueryData() {
  next_state_ = STATE_CACHE_QUERY_DATA_COMPLETE;
  return entry_->disk_entry->ReadyForSparseIO(io_callback_);
}

int HttpCache::Transaction::DoCacheQueryDataComplete(int result) {
  if (result == ERR_NOT_IMPLEMENTED) {
    // Restart the request overwriting the cache entry.
    // TODO(pasko): remove this workaround as soon as the SimpleBackendImpl
    // supports Sparse IO.
    return DoRestartPartialRequest();
  }
  DCHECK_EQ(OK, result);
  if (!cache_.get())
    return ERR_UNEXPECTED;

  return ValidateEntryHeadersAndContinue();
}

int HttpCache::Transaction::DoCacheReadData() {
  DCHECK(entry_);
  next_state_ = STATE_CACHE_READ_DATA_COMPLETE;

  if (net_log_.IsLogging())
    net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_READ_DATA);
  if (partial_.get()) {
    return partial_->CacheRead(entry_->disk_entry, read_buf_.get(), io_buf_len_,
                               io_callback_);
  }

  return entry_->disk_entry->ReadData(kResponseContentIndex, read_offset_,
                                      read_buf_.get(), io_buf_len_,
                                      io_callback_);
}

int HttpCache::Transaction::DoCacheReadDataComplete(int result) {
  if (net_log_.IsLogging()) {
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_READ_DATA,
                                      result);
  }

  if (!cache_.get())
    return ERR_UNEXPECTED;

  if (partial_.get()) {
    // Partial requests are confusing to report in histograms because they may
    // have multiple underlying requests.
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    return DoPartialCacheReadCompleted(result);
  }

  if (result > 0) {
    read_offset_ += result;
  } else if (result == 0) {  // End of file.
    RecordHistograms();
    cache_->DoneReadingFromEntry(entry_, this);
    entry_ = NULL;
  } else {
    return OnCacheReadError(result, false);
  }
  return result;
}

int HttpCache::Transaction::DoCacheWriteData(int num_bytes) {
  next_state_ = STATE_CACHE_WRITE_DATA_COMPLETE;
  write_len_ = num_bytes;
  if (entry_) {
    if (net_log_.IsLogging())
      net_log_.BeginEvent(NetLog::TYPE_HTTP_CACHE_WRITE_DATA);
  }

  return AppendResponseDataToEntry(read_buf_.get(), num_bytes, io_callback_);
}

int HttpCache::Transaction::DoCacheWriteDataComplete(int result) {
  if (entry_) {
    if (net_log_.IsLogging()) {
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_HTTP_CACHE_WRITE_DATA,
                                        result);
    }
  }
  // Balance the AddRef from DoCacheWriteData.
  if (!cache_.get())
    return ERR_UNEXPECTED;

  if (result != write_len_) {
    DLOG(ERROR) << "failed to write response data to cache";
    DoneWritingToEntry(false);

    // We want to ignore errors writing to disk and just keep reading from
    // the network.
    result = write_len_;
  } else if (!done_reading_ && entry_) {
    int current_size = entry_->disk_entry->GetDataSize(kResponseContentIndex);
    int64 body_size = response_.headers->GetContentLength();
    if (body_size >= 0 && body_size <= current_size)
      done_reading_ = true;
  }

  if (partial_.get()) {
    // This may be the last request.
    if (!(result == 0 && !truncated_ &&
          (partial_->IsLastRange() || mode_ == WRITE)))
      return DoPartialNetworkReadCompleted(result);
  }

  if (result == 0) {
    // End of file. This may be the result of a connection problem so see if we
    // have to keep the entry around to be flagged as truncated later on.
    if (done_reading_ || !entry_ || partial_.get() ||
        response_.headers->GetContentLength() <= 0)
      DoneWritingToEntry(true);
  }

  return result;
}

//-----------------------------------------------------------------------------

void HttpCache::Transaction::SetRequest(const BoundNetLog& net_log,
                                        const HttpRequestInfo* request) {
  net_log_ = net_log;
  request_ = request;
  effective_load_flags_ = request_->load_flags;

  switch (cache_->mode()) {
    case NORMAL:
      break;
    case RECORD:
      // When in record mode, we want to NEVER load from the cache.
      // The reason for this is beacuse we save the Set-Cookie headers
      // (intentionally).  If we read from the cache, we replay them
      // prematurely.
      effective_load_flags_ |= LOAD_BYPASS_CACHE;
      break;
    case PLAYBACK:
      // When in playback mode, we want to load exclusively from the cache.
      effective_load_flags_ |= LOAD_ONLY_FROM_CACHE;
      break;
    case DISABLE:
      effective_load_flags_ |= LOAD_DISABLE_CACHE;
      break;
  }

  // Some headers imply load flags.  The order here is significant.
  //
  //   LOAD_DISABLE_CACHE   : no cache read or write
  //   LOAD_BYPASS_CACHE    : no cache read
  //   LOAD_VALIDATE_CACHE  : no cache read unless validation
  //
  // The former modes trump latter modes, so if we find a matching header we
  // can stop iterating kSpecialHeaders.
  //
  static const struct {
    const HeaderNameAndValue* search;
    int load_flag;
  } kSpecialHeaders[] = {
    { kPassThroughHeaders, LOAD_DISABLE_CACHE },
    { kForceFetchHeaders, LOAD_BYPASS_CACHE },
    { kForceValidateHeaders, LOAD_VALIDATE_CACHE },
  };

  bool range_found = false;
  bool external_validation_error = false;

  if (request_->extra_headers.HasHeader(HttpRequestHeaders::kRange))
    range_found = true;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kSpecialHeaders); ++i) {
    if (HeaderMatches(request_->extra_headers, kSpecialHeaders[i].search)) {
      effective_load_flags_ |= kSpecialHeaders[i].load_flag;
      break;
    }
  }

  // Check for conditionalization headers which may correspond with a
  // cache validation request.
  for (size_t i = 0; i < arraysize(kValidationHeaders); ++i) {
    const ValidationHeaderInfo& info = kValidationHeaders[i];
    std::string validation_value;
    if (request_->extra_headers.GetHeader(
            info.request_header_name, &validation_value)) {
      if (!external_validation_.values[i].empty() ||
          validation_value.empty()) {
        external_validation_error = true;
      }
      external_validation_.values[i] = validation_value;
      external_validation_.initialized = true;
    }
  }

  // We don't support ranges and validation headers.
  if (range_found && external_validation_.initialized) {
    LOG(WARNING) << "Byte ranges AND validation headers found.";
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  // If there is more than one validation header, we can't treat this request as
  // a cache validation, since we don't know for sure which header the server
  // will give us a response for (and they could be contradictory).
  if (external_validation_error) {
    LOG(WARNING) << "Multiple or malformed validation headers found.";
    effective_load_flags_ |= LOAD_DISABLE_CACHE;
  }

  if (range_found && !(effective_load_flags_ & LOAD_DISABLE_CACHE)) {
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    partial_.reset(new PartialData);
    if (request_->method == "GET" && partial_->Init(request_->extra_headers)) {
      // We will be modifying the actual range requested to the server, so
      // let's remove the header here.
      custom_request_.reset(new HttpRequestInfo(*request_));
      custom_request_->extra_headers.RemoveHeader(HttpRequestHeaders::kRange);
      request_ = custom_request_.get();
      partial_->SetHeaders(custom_request_->extra_headers);
    } else {
      // The range is invalid or we cannot handle it properly.
      VLOG(1) << "Invalid byte range found.";
      effective_load_flags_ |= LOAD_DISABLE_CACHE;
      partial_.reset(NULL);
    }
  }
}

bool HttpCache::Transaction::ShouldPassThrough() {
  // We may have a null disk_cache if there is an error we cannot recover from,
  // like not enough disk space, or sharing violations.
  if (!cache_->disk_cache_.get())
    return true;

  // When using the record/playback modes, we always use the cache
  // and we never pass through.
  if (cache_->mode() == RECORD || cache_->mode() == PLAYBACK)
    return false;

  if (effective_load_flags_ & LOAD_DISABLE_CACHE)
    return true;

  if (request_->method == "GET")
    return false;

  if (request_->method == "POST" && request_->upload_data_stream &&
      request_->upload_data_stream->identifier()) {
    return false;
  }

  if (request_->method == "PUT" && request_->upload_data_stream)
    return false;

  if (request_->method == "DELETE")
    return false;

  // TODO(darin): add support for caching HEAD responses
  return true;
}

int HttpCache::Transaction::BeginCacheRead() {
  // We don't support any combination of LOAD_ONLY_FROM_CACHE and byte ranges.
  if (response_.headers->response_code() == 206 || partial_.get()) {
    NOTREACHED();
    return ERR_CACHE_MISS;
  }

  // We don't have the whole resource.
  if (truncated_)
    return ERR_CACHE_MISS;

  if (entry_->disk_entry->GetDataSize(kMetadataIndex))
    next_state_ = STATE_CACHE_READ_METADATA;

  return OK;
}

int HttpCache::Transaction::BeginCacheValidation() {
  DCHECK(mode_ == READ_WRITE);

  bool skip_validation = !RequiresValidation();

  if (truncated_) {
    // Truncated entries can cause partial gets, so we shouldn't record this
    // load in histograms.
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    skip_validation = !partial_->initial_validation();
  }

  if (partial_.get() && (is_sparse_ || truncated_) &&
      (!partial_->IsCurrentRangeCached() || invalid_range_)) {
    // Force revalidation for sparse or truncated entries. Note that we don't
    // want to ignore the regular validation logic just because a byte range was
    // part of the request.
    skip_validation = false;
  }

  if (skip_validation) {
    UpdateTransactionPattern(PATTERN_ENTRY_USED);
    RecordOfflineStatus(effective_load_flags_, OFFLINE_STATUS_FRESH_CACHE);
    return SetupEntryForRead();
  } else {
    // Make the network request conditional, to see if we may reuse our cached
    // response.  If we cannot do so, then we just resort to a normal fetch.
    // Our mode remains READ_WRITE for a conditional request.  Even if the
    // conditionalization fails, we don't switch to WRITE mode until we
    // know we won't be falling back to using the cache entry in the
    // LOAD_FROM_CACHE_IF_OFFLINE case.
    if (!ConditionalizeRequest()) {
      couldnt_conditionalize_request_ = true;
      UpdateTransactionPattern(PATTERN_ENTRY_CANT_CONDITIONALIZE);
      if (partial_.get())
        return DoRestartPartialRequest();

      DCHECK_NE(206, response_.headers->response_code());
    }
    next_state_ = STATE_SEND_REQUEST;
  }
  return OK;
}

int HttpCache::Transaction::BeginPartialCacheValidation() {
  DCHECK(mode_ == READ_WRITE);

  if (response_.headers->response_code() != 206 && !partial_.get() &&
      !truncated_) {
    return BeginCacheValidation();
  }

  // Partial requests should not be recorded in histograms.
  UpdateTransactionPattern(PATTERN_NOT_COVERED);
  if (range_requested_) {
    next_state_ = STATE_CACHE_QUERY_DATA;
    return OK;
  }
  // The request is not for a range, but we have stored just ranges.
  partial_.reset(new PartialData());
  partial_->SetHeaders(request_->extra_headers);
  if (!custom_request_.get()) {
    custom_request_.reset(new HttpRequestInfo(*request_));
    request_ = custom_request_.get();
  }

  return ValidateEntryHeadersAndContinue();
}

// This should only be called once per request.
int HttpCache::Transaction::ValidateEntryHeadersAndContinue() {
  DCHECK(mode_ == READ_WRITE);

  if (!partial_->UpdateFromStoredHeaders(
          response_.headers.get(), entry_->disk_entry, truncated_)) {
    return DoRestartPartialRequest();
  }

  if (response_.headers->response_code() == 206)
    is_sparse_ = true;

  if (!partial_->IsRequestedRangeOK()) {
    // The stored data is fine, but the request may be invalid.
    invalid_range_ = true;
  }

  next_state_ = STATE_START_PARTIAL_CACHE_VALIDATION;
  return OK;
}

int HttpCache::Transaction::BeginExternallyConditionalizedRequest() {
  DCHECK_EQ(UPDATE, mode_);
  DCHECK(external_validation_.initialized);

  for (size_t i = 0;  i < arraysize(kValidationHeaders); i++) {
    if (external_validation_.values[i].empty())
      continue;
    // Retrieve either the cached response's "etag" or "last-modified" header.
    std::string validator;
    response_.headers->EnumerateHeader(
        NULL,
        kValidationHeaders[i].related_response_header_name,
        &validator);

    if (response_.headers->response_code() != 200 || truncated_ ||
        validator.empty() || validator != external_validation_.values[i]) {
      // The externally conditionalized request is not a validation request
      // for our existing cache entry. Proceed with caching disabled.
      UpdateTransactionPattern(PATTERN_NOT_COVERED);
      DoneWritingToEntry(true);
    }
  }

  next_state_ = STATE_SEND_REQUEST;
  return OK;
}

int HttpCache::Transaction::RestartNetworkRequest() {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  int rv = network_trans_->RestartIgnoringLastError(io_callback_);
  if (rv != ERR_IO_PENDING)
    return DoLoop(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithCertificate(
    X509Certificate* client_cert) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  int rv = network_trans_->RestartWithCertificate(client_cert, io_callback_);
  if (rv != ERR_IO_PENDING)
    return DoLoop(rv);
  return rv;
}

int HttpCache::Transaction::RestartNetworkRequestWithAuth(
    const AuthCredentials& credentials) {
  DCHECK(mode_ & WRITE || mode_ == NONE);
  DCHECK(network_trans_.get());
  DCHECK_EQ(STATE_NONE, next_state_);

  next_state_ = STATE_SEND_REQUEST_COMPLETE;
  int rv = network_trans_->RestartWithAuth(credentials, io_callback_);
  if (rv != ERR_IO_PENDING)
    return DoLoop(rv);
  return rv;
}

bool HttpCache::Transaction::RequiresValidation() {
  // TODO(darin): need to do more work here:
  //  - make sure we have a matching request method
  //  - watch out for cached responses that depend on authentication

  // In playback mode, nothing requires validation.
  if (cache_->mode() == net::HttpCache::PLAYBACK)
    return false;

  if (response_.vary_data.is_valid() &&
      !response_.vary_data.MatchesRequest(*request_,
                                          *response_.headers.get())) {
    vary_mismatch_ = true;
    return true;
  }

  if (effective_load_flags_ & LOAD_PREFERRING_CACHE)
    return false;

  if (effective_load_flags_ & LOAD_VALIDATE_CACHE)
    return true;

  if (request_->method == "PUT" || request_->method == "DELETE")
    return true;

  if (response_.headers->RequiresValidation(
          response_.request_time, response_.response_time, Time::Now())) {
    return true;
  }

  return false;
}

bool HttpCache::Transaction::ConditionalizeRequest() {
  DCHECK(response_.headers.get());

  if (request_->method == "PUT" || request_->method == "DELETE")
    return false;

  // This only makes sense for cached 200 or 206 responses.
  if (response_.headers->response_code() != 200 &&
      response_.headers->response_code() != 206) {
    return false;
  }

  // We should have handled this case before.
  DCHECK(response_.headers->response_code() != 206 ||
         response_.headers->HasStrongValidators());

  // Just use the first available ETag and/or Last-Modified header value.
  // TODO(darin): Or should we use the last?

  std::string etag_value;
  if (response_.headers->GetHttpVersion() >= HttpVersion(1, 1))
    response_.headers->EnumerateHeader(NULL, "etag", &etag_value);

  std::string last_modified_value;
  if (!vary_mismatch_) {
    response_.headers->EnumerateHeader(NULL, "last-modified",
                                       &last_modified_value);
  }

  if (etag_value.empty() && last_modified_value.empty())
    return false;

  if (!partial_.get()) {
    // Need to customize the request, so this forces us to allocate :(
    custom_request_.reset(new HttpRequestInfo(*request_));
    request_ = custom_request_.get();
  }
  DCHECK(custom_request_.get());

  bool use_if_range = partial_.get() && !partial_->IsCurrentRangeCached() &&
                      !invalid_range_;

  if (!etag_value.empty()) {
    if (use_if_range) {
      // We don't want to switch to WRITE mode if we don't have this block of a
      // byte-range request because we may have other parts cached.
      custom_request_->extra_headers.SetHeader(
          HttpRequestHeaders::kIfRange, etag_value);
    } else {
      custom_request_->extra_headers.SetHeader(
          HttpRequestHeaders::kIfNoneMatch, etag_value);
    }
    // For byte-range requests, make sure that we use only one way to validate
    // the request.
    if (partial_.get() && !partial_->IsCurrentRangeCached())
      return true;
  }

  if (!last_modified_value.empty()) {
    if (use_if_range) {
      custom_request_->extra_headers.SetHeader(
          HttpRequestHeaders::kIfRange, last_modified_value);
    } else {
      custom_request_->extra_headers.SetHeader(
          HttpRequestHeaders::kIfModifiedSince, last_modified_value);
    }
  }

  return true;
}

// We just received some headers from the server. We may have asked for a range,
// in which case partial_ has an object. This could be the first network request
// we make to fulfill the original request, or we may be already reading (from
// the net and / or the cache). If we are not expecting a certain response, we
// just bypass the cache for this request (but again, maybe we are reading), and
// delete partial_ (so we are not able to "fix" the headers that we return to
// the user). This results in either a weird response for the caller (we don't
// expect it after all), or maybe a range that was not exactly what it was asked
// for.
//
// If the server is simply telling us that the resource has changed, we delete
// the cached entry and restart the request as the caller intended (by returning
// false from this method). However, we may not be able to do that at any point,
// for instance if we already returned the headers to the user.
//
// WARNING: Whenever this code returns false, it has to make sure that the next
// time it is called it will return true so that we don't keep retrying the
// request.
bool HttpCache::Transaction::ValidatePartialResponse() {
  const HttpResponseHeaders* headers = new_response_->headers.get();
  int response_code = headers->response_code();
  bool partial_response = (response_code == 206);
  handling_206_ = false;

  if (!entry_ || request_->method != "GET")
    return true;

  if (invalid_range_) {
    // We gave up trying to match this request with the stored data. If the
    // server is ok with the request, delete the entry, otherwise just ignore
    // this request
    DCHECK(!reading_);
    if (partial_response || response_code == 200) {
      DoomPartialEntry(true);
      mode_ = NONE;
    } else {
      if (response_code == 304)
        FailRangeRequest();
      IgnoreRangeRequest();
    }
    return true;
  }

  if (!partial_.get()) {
    // We are not expecting 206 but we may have one.
    if (partial_response)
      IgnoreRangeRequest();

    return true;
  }

  // TODO(rvargas): Do we need to consider other results here?.
  bool failure = response_code == 200 || response_code == 416;

  if (partial_->IsCurrentRangeCached()) {
    // We asked for "If-None-Match: " so a 206 means a new object.
    if (partial_response)
      failure = true;

    if (response_code == 304 && partial_->ResponseHeadersOK(headers))
      return true;
  } else {
    // We asked for "If-Range: " so a 206 means just another range.
    if (partial_response && partial_->ResponseHeadersOK(headers)) {
      handling_206_ = true;
      return true;
    }

    if (!reading_ && !is_sparse_ && !partial_response) {
      // See if we can ignore the fact that we issued a byte range request.
      // If the server sends 200, just store it. If it sends an error, redirect
      // or something else, we may store the response as long as we didn't have
      // anything already stored.
      if (response_code == 200 ||
          (!truncated_ && response_code != 304 && response_code != 416)) {
        // The server is sending something else, and we can save it.
        DCHECK((truncated_ && !partial_->IsLastRange()) || range_requested_);
        partial_.reset();
        truncated_ = false;
        return true;
      }
    }

    // 304 is not expected here, but we'll spare the entry (unless it was
    // truncated).
    if (truncated_)
      failure = true;
  }

  if (failure) {
    // We cannot truncate this entry, it has to be deleted.
    UpdateTransactionPattern(PATTERN_NOT_COVERED);
    DoomPartialEntry(false);
    mode_ = NONE;
    if (!reading_ && !partial_->IsLastRange()) {
      // We'll attempt to issue another network request, this time without us
      // messing up the headers.
      partial_->RestoreHeaders(&custom_request_->extra_headers);
      partial_.reset();
      truncated_ = false;
      return false;
    }
    LOG(WARNING) << "Failed to revalidate partial entry";
    partial_.reset();
    return true;
  }

  IgnoreRangeRequest();
  return true;
}

void HttpCache::Transaction::IgnoreRangeRequest() {
  // We have a problem. We may or may not be reading already (in which case we
  // returned the headers), but we'll just pretend that this request is not
  // using the cache and see what happens. Most likely this is the first
  // response from the server (it's not changing its mind midway, right?).
  UpdateTransactionPattern(PATTERN_NOT_COVERED);
  if (mode_ & WRITE)
    DoneWritingToEntry(mode_ != WRITE);
  else if (mode_ & READ && entry_)
    cache_->DoneReadingFromEntry(entry_, this);

  partial_.reset(NULL);
  entry_ = NULL;
  mode_ = NONE;
}

void HttpCache::Transaction::FailRangeRequest() {
  response_ = *new_response_;
  partial_->FixResponseHeaders(response_.headers.get(), false);
}

int HttpCache::Transaction::SetupEntryForRead() {
  if (network_trans_)
    ResetNetworkTransaction();
  if (partial_.get()) {
    if (truncated_ || is_sparse_ || !invalid_range_) {
      // We are going to return the saved response headers to the caller, so
      // we may need to adjust them first.
      next_state_ = STATE_PARTIAL_HEADERS_RECEIVED;
      return OK;
    } else {
      partial_.reset();
    }
  }
  cache_->ConvertWriterToReader(entry_);
  mode_ = READ;

  if (entry_->disk_entry->GetDataSize(kMetadataIndex))
    next_state_ = STATE_CACHE_READ_METADATA;
  return OK;
}


int HttpCache::Transaction::ReadFromNetwork(IOBuffer* data, int data_len) {
  read_buf_ = data;
  io_buf_len_ = data_len;
  next_state_ = STATE_NETWORK_READ;
  return DoLoop(OK);
}

int HttpCache::Transaction::ReadFromEntry(IOBuffer* data, int data_len) {
  read_buf_ = data;
  io_buf_len_ = data_len;
  next_state_ = STATE_CACHE_READ_DATA;
  return DoLoop(OK);
}

int HttpCache::Transaction::WriteToEntry(int index, int offset,
                                         IOBuffer* data, int data_len,
                                         const CompletionCallback& callback) {
  if (!entry_)
    return data_len;

  int rv = 0;
  if (!partial_.get() || !data_len) {
    rv = entry_->disk_entry->WriteData(index, offset, data, data_len, callback,
                                       true);
  } else {
    rv = partial_->CacheWrite(entry_->disk_entry, data, data_len, callback);
  }
  return rv;
}

int HttpCache::Transaction::WriteResponseInfoToEntry(bool truncated) {
  next_state_ = STATE_CACHE_WRITE_RESPONSE_COMPLETE;
  if (!entry_)
    return OK;

  // Do not cache no-store content (unless we are record mode).  Do not cache
  // content with cert errors either.  This is to prevent not reporting net
  // errors when loading a resource from the cache.  When we load a page over
  // HTTPS with a cert error we show an SSL blocking page.  If the user clicks
  // proceed we reload the resource ignoring the errors.  The loaded resource
  // is then cached.  If that resource is subsequently loaded from the cache,
  // no net error is reported (even though the cert status contains the actual
  // errors) and no SSL blocking page is shown.  An alternative would be to
  // reverse-map the cert status to a net error and replay the net error.
  if ((cache_->mode() != RECORD &&
       response_.headers->HasHeaderValue("cache-control", "no-store")) ||
      net::IsCertStatusError(response_.ssl_info.cert_status)) {
    DoneWritingToEntry(false);
    if (net_log_.IsLogging())
      net_log_.EndEvent(NetLog::TYPE_HTTP_CACHE_WRITE_INFO);
    return OK;
  }

  // When writing headers, we normally only write the non-transient
  // headers; when in record mode, record everything.
  bool skip_transient_headers = (cache_->mode() != RECORD);

  if (truncated)
    DCHECK_EQ(200, response_.headers->response_code());

  scoped_refptr<PickledIOBuffer> data(new PickledIOBuffer());
  response_.Persist(data->pickle(), skip_transient_headers, truncated);
  data->Done();

  io_buf_len_ = data->pickle()->size();
  return entry_->disk_entry->WriteData(kResponseInfoIndex, 0, data.get(),
                                       io_buf_len_, io_callback_, true);
}

int HttpCache::Transaction::AppendResponseDataToEntry(
    IOBuffer* data, int data_len, const CompletionCallback& callback) {
  if (!entry_ || !data_len)
    return data_len;

  int current_size = entry_->disk_entry->GetDataSize(kResponseContentIndex);
  return WriteToEntry(kResponseContentIndex, current_size, data, data_len,
                      callback);
}

void HttpCache::Transaction::DoneWritingToEntry(bool success) {
  if (!entry_)
    return;

  RecordHistograms();

  cache_->DoneWritingToEntry(entry_, success);
  entry_ = NULL;
  mode_ = NONE;  // switch to 'pass through' mode
}

int HttpCache::Transaction::OnCacheReadError(int result, bool restart) {
  DLOG(ERROR) << "ReadData failed: " << result;
  const int result_for_histogram = std::max(0, -result);
  if (restart) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("HttpCache.ReadErrorRestartable",
                                result_for_histogram);
  } else {
    UMA_HISTOGRAM_SPARSE_SLOWLY("HttpCache.ReadErrorNonRestartable",
                                result_for_histogram);
  }

  // Avoid using this entry in the future.
  if (cache_.get())
    cache_->DoomActiveEntry(cache_key_);

  if (restart) {
    DCHECK(!reading_);
    DCHECK(!network_trans_.get());
    cache_->DoneWithEntry(entry_, this, false);
    entry_ = NULL;
    is_sparse_ = false;
    partial_.reset();
    next_state_ = STATE_GET_BACKEND;
    return OK;
  }

  return ERR_CACHE_READ_FAILURE;
}

void HttpCache::Transaction::DoomPartialEntry(bool delete_object) {
  DVLOG(2) << "DoomPartialEntry";
  int rv = cache_->DoomEntry(cache_key_, NULL);
  DCHECK_EQ(OK, rv);
  cache_->DoneWithEntry(entry_, this, false);
  entry_ = NULL;
  is_sparse_ = false;
  if (delete_object)
    partial_.reset(NULL);
}

int HttpCache::Transaction::DoPartialNetworkReadCompleted(int result) {
  partial_->OnNetworkReadCompleted(result);

  if (result == 0) {
    // We need to move on to the next range.
    ResetNetworkTransaction();
    next_state_ = STATE_START_PARTIAL_CACHE_VALIDATION;
  }
  return result;
}

int HttpCache::Transaction::DoPartialCacheReadCompleted(int result) {
  partial_->OnCacheReadCompleted(result);

  if (result == 0 && mode_ == READ_WRITE) {
    // We need to move on to the next range.
    next_state_ = STATE_START_PARTIAL_CACHE_VALIDATION;
  } else if (result < 0) {
    return OnCacheReadError(result, false);
  }
  return result;
}

int HttpCache::Transaction::DoRestartPartialRequest() {
  // The stored data cannot be used. Get rid of it and restart this request.
  // We need to also reset the |truncated_| flag as a new entry is created.
  DoomPartialEntry(!range_requested_);
  mode_ = WRITE;
  truncated_ = false;
  next_state_ = STATE_INIT_ENTRY;
  return OK;
}

void HttpCache::Transaction::ResetNetworkTransaction() {
  DCHECK(!old_network_trans_load_timing_);
  DCHECK(network_trans_);
  LoadTimingInfo load_timing;
  if (network_trans_->GetLoadTimingInfo(&load_timing))
    old_network_trans_load_timing_.reset(new LoadTimingInfo(load_timing));
  total_received_bytes_ += network_trans_->GetTotalReceivedBytes();
  network_trans_.reset();
}

// Histogram data from the end of 2010 show the following distribution of
// response headers:
//
//   Content-Length............... 87%
//   Date......................... 98%
//   Last-Modified................ 49%
//   Etag......................... 19%
//   Accept-Ranges: bytes......... 25%
//   Accept-Ranges: none.......... 0.4%
//   Strong Validator............. 50%
//   Strong Validator + ranges.... 24%
//   Strong Validator + CL........ 49%
//
bool HttpCache::Transaction::CanResume(bool has_data) {
  // Double check that there is something worth keeping.
  if (has_data && !entry_->disk_entry->GetDataSize(kResponseContentIndex))
    return false;

  if (request_->method != "GET")
    return false;

  // Note that if this is a 206, content-length was already fixed after calling
  // PartialData::ResponseHeadersOK().
  if (response_.headers->GetContentLength() <= 0 ||
      response_.headers->HasHeaderValue("Accept-Ranges", "none") ||
      !response_.headers->HasStrongValidators()) {
    return false;
  }

  return true;
}

void HttpCache::Transaction::UpdateTransactionPattern(
    TransactionPattern new_transaction_pattern) {
  if (transaction_pattern_ == PATTERN_NOT_COVERED)
    return;
  DCHECK(transaction_pattern_ == PATTERN_UNDEFINED ||
         new_transaction_pattern == PATTERN_NOT_COVERED);
  transaction_pattern_ = new_transaction_pattern;
}

void HttpCache::Transaction::RecordHistograms() {
  DCHECK_NE(PATTERN_UNDEFINED, transaction_pattern_);
  if (!cache_.get() || !cache_->GetCurrentBackend() ||
      cache_->GetCurrentBackend()->GetCacheType() != DISK_CACHE ||
      cache_->mode() != NORMAL || request_->method != "GET") {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION(
      "HttpCache.Pattern", transaction_pattern_, PATTERN_MAX);
  if (transaction_pattern_ == PATTERN_NOT_COVERED)
    return;
  DCHECK(!range_requested_);
  DCHECK(!first_cache_access_since_.is_null());

  TimeDelta total_time = base::TimeTicks::Now() - first_cache_access_since_;

  UMA_HISTOGRAM_TIMES("HttpCache.AccessToDone", total_time);

  bool did_send_request = !send_request_since_.is_null();
  DCHECK(
      (did_send_request &&
       (transaction_pattern_ == PATTERN_ENTRY_NOT_CACHED ||
        transaction_pattern_ == PATTERN_ENTRY_VALIDATED ||
        transaction_pattern_ == PATTERN_ENTRY_UPDATED ||
        transaction_pattern_ == PATTERN_ENTRY_CANT_CONDITIONALIZE)) ||
      (!did_send_request && transaction_pattern_ == PATTERN_ENTRY_USED));

  if (!did_send_request) {
    DCHECK(transaction_pattern_ == PATTERN_ENTRY_USED);
    UMA_HISTOGRAM_TIMES("HttpCache.AccessToDone.Used", total_time);
    return;
  }

  TimeDelta before_send_time = send_request_since_ - first_cache_access_since_;
  int before_send_percent =
      total_time.ToInternalValue() == 0 ? 0
                                        : before_send_time * 100 / total_time;
  DCHECK_LE(0, before_send_percent);
  DCHECK_GE(100, before_send_percent);

  UMA_HISTOGRAM_TIMES("HttpCache.AccessToDone.SentRequest", total_time);
  UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend", before_send_time);
  UMA_HISTOGRAM_PERCENTAGE("HttpCache.PercentBeforeSend", before_send_percent);

  // TODO(gavinp): Remove or minimize these histograms, particularly the ones
  // below this comment after we have received initial data.
  switch (transaction_pattern_) {
    case PATTERN_ENTRY_CANT_CONDITIONALIZE: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.CantConditionalize",
                          before_send_time);
      UMA_HISTOGRAM_PERCENTAGE("HttpCache.PercentBeforeSend.CantConditionalize",
                               before_send_percent);
      break;
    }
    case PATTERN_ENTRY_NOT_CACHED: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.NotCached", before_send_time);
      UMA_HISTOGRAM_PERCENTAGE("HttpCache.PercentBeforeSend.NotCached",
                               before_send_percent);
      break;
    }
    case PATTERN_ENTRY_VALIDATED: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.Validated", before_send_time);
      UMA_HISTOGRAM_PERCENTAGE("HttpCache.PercentBeforeSend.Validated",
                               before_send_percent);
      break;
    }
    case PATTERN_ENTRY_UPDATED: {
      UMA_HISTOGRAM_TIMES("HttpCache.BeforeSend.Updated", before_send_time);
      UMA_HISTOGRAM_PERCENTAGE("HttpCache.PercentBeforeSend.Updated",
                               before_send_percent);
      break;
    }
    default:
      NOTREACHED();
  }
}

void HttpCache::Transaction::OnIOComplete(int result) {
  DoLoop(result);
}

}  // namespace net
