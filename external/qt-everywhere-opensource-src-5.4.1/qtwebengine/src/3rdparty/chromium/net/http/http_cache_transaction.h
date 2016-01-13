// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares HttpCache::Transaction, a private class of HttpCache so
// it should only be included by http_cache.cc

#ifndef NET_HTTP_HTTP_CACHE_TRANSACTION_H_
#define NET_HTTP_HTTP_CACHE_TRANSACTION_H_

#include <string>

#include "base/time/time.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"
#include "net/base/request_priority.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"

namespace net {

class PartialData;
struct HttpRequestInfo;
struct LoadTimingInfo;

// This is the transaction that is returned by the HttpCache transaction
// factory.
class HttpCache::Transaction : public HttpTransaction {
 public:
  // The transaction has the following modes, which apply to how it may access
  // its cache entry.
  //
  //  o If the mode of the transaction is NONE, then it is in "pass through"
  //    mode and all methods just forward to the inner network transaction.
  //
  //  o If the mode of the transaction is only READ, then it may only read from
  //    the cache entry.
  //
  //  o If the mode of the transaction is only WRITE, then it may only write to
  //    the cache entry.
  //
  //  o If the mode of the transaction is READ_WRITE, then the transaction may
  //    optionally modify the cache entry (e.g., possibly corresponding to
  //    cache validation).
  //
  //  o If the mode of the transaction is UPDATE, then the transaction may
  //    update existing cache entries, but will never create a new entry or
  //    respond using the entry read from the cache.
  enum Mode {
    NONE            = 0,
    READ_META       = 1 << 0,
    READ_DATA       = 1 << 1,
    READ            = READ_META | READ_DATA,
    WRITE           = 1 << 2,
    READ_WRITE      = READ | WRITE,
    UPDATE          = READ_META | WRITE,  // READ_WRITE & ~READ_DATA
  };

  Transaction(RequestPriority priority,
              HttpCache* cache);
  virtual ~Transaction();

  Mode mode() const { return mode_; }

  const std::string& key() const { return cache_key_; }

  // Writes |buf_len| bytes of meta-data from the provided buffer |buf|. to the
  // HTTP cache entry that backs this transaction (if any).
  // Returns the number of bytes actually written, or a net error code. If the
  // operation cannot complete immediately, returns ERR_IO_PENDING, grabs a
  // reference to the buffer (until completion), and notifies the caller using
  // the provided |callback| when the operation finishes.
  //
  // The first time this method is called for a given transaction, previous
  // meta-data will be overwritten with the provided data, and subsequent
  // invocations will keep appending to the cached entry.
  //
  // In order to guarantee that the metadata is set to the correct entry, the
  // response (or response info) must be evaluated by the caller, for instance
  // to make sure that the response_time is as expected, before calling this
  // method.
  int WriteMetadata(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback);

  // This transaction is being deleted and we are not done writing to the cache.
  // We need to indicate that the response data was truncated.  Returns true on
  // success. Keep in mind that this operation may have side effects, such as
  // deleting the active entry.
  bool AddTruncatedFlag();

  HttpCache::ActiveEntry* entry() { return entry_; }

  // Returns the LoadState of the writer transaction of a given ActiveEntry. In
  // other words, returns the LoadState of this transaction without asking the
  // http cache, because this transaction should be the one currently writing
  // to the cache entry.
  LoadState GetWriterLoadState() const;

  const CompletionCallback& io_callback() { return io_callback_; }

  const BoundNetLog& net_log() const;

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo* request_info,
                    const CompletionCallback& callback,
                    const BoundNetLog& net_log) OVERRIDE;
  virtual int RestartIgnoringLastError(
      const CompletionCallback& callback) OVERRIDE;
  virtual int RestartWithCertificate(
      X509Certificate* client_cert,
      const CompletionCallback& callback) OVERRIDE;
  virtual int RestartWithAuth(const AuthCredentials& credentials,
                              const CompletionCallback& callback) OVERRIDE;
  virtual bool IsReadyToRestartForAuth() OVERRIDE;
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual void StopCaching() OVERRIDE;
  virtual bool GetFullRequestHeaders(
      HttpRequestHeaders* headers) const OVERRIDE;
  virtual int64 GetTotalReceivedBytes() const OVERRIDE;
  virtual void DoneReading() OVERRIDE;
  virtual const HttpResponseInfo* GetResponseInfo() const OVERRIDE;
  virtual LoadState GetLoadState() const OVERRIDE;
  virtual UploadProgress GetUploadProgress(void) const OVERRIDE;
  virtual void SetQuicServerInfo(QuicServerInfo* quic_server_info) OVERRIDE;
  virtual bool GetLoadTimingInfo(
      LoadTimingInfo* load_timing_info) const OVERRIDE;
  virtual void SetPriority(RequestPriority priority) OVERRIDE;
  virtual void SetWebSocketHandshakeStreamCreateHelper(
      net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) OVERRIDE;
  virtual void SetBeforeNetworkStartCallback(
      const BeforeNetworkStartCallback& callback) OVERRIDE;
  virtual int ResumeNetworkStart() OVERRIDE;

 private:
  static const size_t kNumValidationHeaders = 2;
  // Helper struct to pair a header name with its value, for
  // headers used to validate cache entries.
  struct ValidationHeaders {
    ValidationHeaders() : initialized(false) {}

    std::string values[kNumValidationHeaders];
    bool initialized;
  };

  enum State {
    STATE_NONE,
    STATE_GET_BACKEND,
    STATE_GET_BACKEND_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_SUCCESSFUL_SEND_REQUEST,
    STATE_NETWORK_READ,
    STATE_NETWORK_READ_COMPLETE,
    STATE_INIT_ENTRY,
    STATE_OPEN_ENTRY,
    STATE_OPEN_ENTRY_COMPLETE,
    STATE_CREATE_ENTRY,
    STATE_CREATE_ENTRY_COMPLETE,
    STATE_DOOM_ENTRY,
    STATE_DOOM_ENTRY_COMPLETE,
    STATE_ADD_TO_ENTRY,
    STATE_ADD_TO_ENTRY_COMPLETE,
    STATE_START_PARTIAL_CACHE_VALIDATION,
    STATE_COMPLETE_PARTIAL_CACHE_VALIDATION,
    STATE_UPDATE_CACHED_RESPONSE,
    STATE_UPDATE_CACHED_RESPONSE_COMPLETE,
    STATE_OVERWRITE_CACHED_RESPONSE,
    STATE_TRUNCATE_CACHED_DATA,
    STATE_TRUNCATE_CACHED_DATA_COMPLETE,
    STATE_TRUNCATE_CACHED_METADATA,
    STATE_TRUNCATE_CACHED_METADATA_COMPLETE,
    STATE_PARTIAL_HEADERS_RECEIVED,
    STATE_CACHE_READ_RESPONSE,
    STATE_CACHE_READ_RESPONSE_COMPLETE,
    STATE_CACHE_WRITE_RESPONSE,
    STATE_CACHE_WRITE_TRUNCATED_RESPONSE,
    STATE_CACHE_WRITE_RESPONSE_COMPLETE,
    STATE_CACHE_READ_METADATA,
    STATE_CACHE_READ_METADATA_COMPLETE,
    STATE_CACHE_QUERY_DATA,
    STATE_CACHE_QUERY_DATA_COMPLETE,
    STATE_CACHE_READ_DATA,
    STATE_CACHE_READ_DATA_COMPLETE,
    STATE_CACHE_WRITE_DATA,
    STATE_CACHE_WRITE_DATA_COMPLETE
  };

  // Used for categorizing transactions for reporting in histograms. Patterns
  // cover relatively common use cases being measured and considered for
  // optimization. Many use cases that are more complex or uncommon are binned
  // as PATTERN_NOT_COVERED, and details are not reported.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum TransactionPattern {
    PATTERN_UNDEFINED,
    PATTERN_NOT_COVERED,
    PATTERN_ENTRY_NOT_CACHED,
    PATTERN_ENTRY_USED,
    PATTERN_ENTRY_VALIDATED,
    PATTERN_ENTRY_UPDATED,
    PATTERN_ENTRY_CANT_CONDITIONALIZE,
    PATTERN_MAX,
  };

  // This is a helper function used to trigger a completion callback.  It may
  // only be called if callback_ is non-null.
  void DoCallback(int rv);

  // This will trigger the completion callback if appropriate.
  int HandleResult(int rv);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  If there is an
  // argument, the value corresponds to the return of the previous state or
  // corresponding callback.
  int DoGetBackend();
  int DoGetBackendComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoSuccessfulSendRequest();
  int DoNetworkRead();
  int DoNetworkReadComplete(int result);
  int DoInitEntry();
  int DoOpenEntry();
  int DoOpenEntryComplete(int result);
  int DoCreateEntry();
  int DoCreateEntryComplete(int result);
  int DoDoomEntry();
  int DoDoomEntryComplete(int result);
  int DoAddToEntry();
  int DoAddToEntryComplete(int result);
  int DoStartPartialCacheValidation();
  int DoCompletePartialCacheValidation(int result);
  int DoUpdateCachedResponse();
  int DoUpdateCachedResponseComplete(int result);
  int DoOverwriteCachedResponse();
  int DoTruncateCachedData();
  int DoTruncateCachedDataComplete(int result);
  int DoTruncateCachedMetadata();
  int DoTruncateCachedMetadataComplete(int result);
  int DoPartialHeadersReceived();
  int DoCacheReadResponse();
  int DoCacheReadResponseComplete(int result);
  int DoCacheWriteResponse();
  int DoCacheWriteTruncatedResponse();
  int DoCacheWriteResponseComplete(int result);
  int DoCacheReadMetadata();
  int DoCacheReadMetadataComplete(int result);
  int DoCacheQueryData();
  int DoCacheQueryDataComplete(int result);
  int DoCacheReadData();
  int DoCacheReadDataComplete(int result);
  int DoCacheWriteData(int num_bytes);
  int DoCacheWriteDataComplete(int result);

  // Sets request_ and fields derived from it.
  void SetRequest(const BoundNetLog& net_log, const HttpRequestInfo* request);

  // Returns true if the request should be handled exclusively by the network
  // layer (skipping the cache entirely).
  bool ShouldPassThrough();

  // Called to begin reading from the cache.  Returns network error code.
  int BeginCacheRead();

  // Called to begin validating the cache entry.  Returns network error code.
  int BeginCacheValidation();

  // Called to begin validating an entry that stores partial content.  Returns
  // a network error code.
  int BeginPartialCacheValidation();

  // Validates the entry headers against the requested range and continues with
  // the validation of the rest of the entry.  Returns a network error code.
  int ValidateEntryHeadersAndContinue();

  // Called to start requests which were given an "if-modified-since" or
  // "if-none-match" validation header by the caller (NOT when the request was
  // conditionalized internally in response to LOAD_VALIDATE_CACHE).
  // Returns a network error code.
  int BeginExternallyConditionalizedRequest();

  // Called to restart a network transaction after an error.  Returns network
  // error code.
  int RestartNetworkRequest();

  // Called to restart a network transaction with a client certificate.
  // Returns network error code.
  int RestartNetworkRequestWithCertificate(X509Certificate* client_cert);

  // Called to restart a network transaction with authentication credentials.
  // Returns network error code.
  int RestartNetworkRequestWithAuth(const AuthCredentials& credentials);

  // Called to determine if we need to validate the cache entry before using it.
  bool RequiresValidation();

  // Called to make the request conditional (to ask the server if the cached
  // copy is valid).  Returns true if able to make the request conditional.
  bool ConditionalizeRequest();

  // Makes sure that a 206 response is expected.  Returns true on success.
  // On success, handling_206_ will be set to true if we are processing a
  // partial entry.
  bool ValidatePartialResponse();

  // Handles a response validation error by bypassing the cache.
  void IgnoreRangeRequest();

  // Changes the response code of a range request to be 416 (Requested range not
  // satisfiable).
  void FailRangeRequest();

  // Setups the transaction for reading from the cache entry.
  int SetupEntryForRead();

  // Reads data from the network.
  int ReadFromNetwork(IOBuffer* data, int data_len);

  // Reads data from the cache entry.
  int ReadFromEntry(IOBuffer* data, int data_len);

  // Called to write data to the cache entry.  If the write fails, then the
  // cache entry is destroyed.  Future calls to this function will just do
  // nothing without side-effect.  Returns a network error code.
  int WriteToEntry(int index, int offset, IOBuffer* data, int data_len,
                   const CompletionCallback& callback);

  // Called to write response_ to the cache entry. |truncated| indicates if the
  // entry should be marked as incomplete.
  int WriteResponseInfoToEntry(bool truncated);

  // Called to append response data to the cache entry.  Returns a network error
  // code.
  int AppendResponseDataToEntry(IOBuffer* data, int data_len,
                                const CompletionCallback& callback);

  // Called when we are done writing to the cache entry.
  void DoneWritingToEntry(bool success);

  // Returns an error to signal the caller that the current read failed. The
  // current operation |result| is also logged. If |restart| is true, the
  // transaction should be restarted.
  int OnCacheReadError(int result, bool restart);

  // Deletes the current partial cache entry (sparse), and optionally removes
  // the control object (partial_).
  void DoomPartialEntry(bool delete_object);

  // Performs the needed work after receiving data from the network, when
  // working with range requests.
  int DoPartialNetworkReadCompleted(int result);

  // Performs the needed work after receiving data from the cache, when
  // working with range requests.
  int DoPartialCacheReadCompleted(int result);

  // Restarts this transaction after deleting the cached data. It is meant to
  // be used when the current request cannot be fulfilled due to conflicts
  // between the byte range request and the cached entry.
  int DoRestartPartialRequest();

  // Resets |network_trans_|, which must be non-NULL.  Also updates
  // |old_network_trans_load_timing_|, which must be NULL when this is called.
  void ResetNetworkTransaction();

  // Returns true if we should bother attempting to resume this request if it
  // is aborted while in progress. If |has_data| is true, the size of the stored
  // data is considered for the result.
  bool CanResume(bool has_data);

  void UpdateTransactionPattern(TransactionPattern new_transaction_pattern);
  void RecordHistograms();

  // Called to signal completion of asynchronous IO.
  void OnIOComplete(int result);

  State next_state_;
  const HttpRequestInfo* request_;
  RequestPriority priority_;
  BoundNetLog net_log_;
  scoped_ptr<HttpRequestInfo> custom_request_;
  HttpRequestHeaders request_headers_copy_;
  // If extra_headers specified a "if-modified-since" or "if-none-match",
  // |external_validation_| contains the value of those headers.
  ValidationHeaders external_validation_;
  base::WeakPtr<HttpCache> cache_;
  HttpCache::ActiveEntry* entry_;
  HttpCache::ActiveEntry* new_entry_;
  scoped_ptr<HttpTransaction> network_trans_;
  CompletionCallback callback_;  // Consumer's callback.
  HttpResponseInfo response_;
  HttpResponseInfo auth_response_;
  const HttpResponseInfo* new_response_;
  std::string cache_key_;
  Mode mode_;
  State target_state_;
  bool reading_;  // We are already reading. Never reverts to false once set.
  bool invalid_range_;  // We may bypass the cache for this request.
  bool truncated_;  // We don't have all the response data.
  bool is_sparse_;  // The data is stored in sparse byte ranges.
  bool range_requested_;  // The user requested a byte range.
  bool handling_206_;  // We must deal with this 206 response.
  bool cache_pending_;  // We are waiting for the HttpCache.
  bool done_reading_;  // All available data was read.
  bool vary_mismatch_;  // The request doesn't match the stored vary data.
  bool couldnt_conditionalize_request_;
  scoped_refptr<IOBuffer> read_buf_;
  int io_buf_len_;
  int read_offset_;
  int effective_load_flags_;
  int write_len_;
  scoped_ptr<PartialData> partial_;  // We are dealing with range requests.
  UploadProgress final_upload_progress_;
  base::WeakPtrFactory<Transaction> weak_factory_;
  CompletionCallback io_callback_;

  // Members used to track data for histograms.
  TransactionPattern transaction_pattern_;
  base::TimeTicks entry_lock_waiting_since_;
  base::TimeTicks first_cache_access_since_;
  base::TimeTicks send_request_since_;

  int64 total_received_bytes_;

  // Load timing information for the last network request, if any.  Set in the
  // 304 and 206 response cases, as the network transaction may be destroyed
  // before the caller requests load timing information.
  scoped_ptr<LoadTimingInfo> old_network_trans_load_timing_;

  // The helper object to use to create WebSocketHandshakeStreamBase
  // objects. Only relevant when establishing a WebSocket connection.
  // This is passed to the underlying network transaction. It is stored here in
  // case the transaction does not exist yet.
  WebSocketHandshakeStreamBase::CreateHelper*
      websocket_handshake_stream_base_create_helper_;

  BeforeNetworkStartCallback before_network_start_callback_;

  DISALLOW_COPY_AND_ASSIGN(Transaction);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_TRANSACTION_H_
