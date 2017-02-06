// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_proof_fetcher.h"

#include <iterator>
#include <memory>

#include "base/callback_helpers.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/safe_json/safe_json_parser.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/cert/ct_log_response_parser.h"
#include "net/cert/signed_tree_head.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace certificate_transparency {

namespace {

// Class for issuing a particular request from a CT log and assembling the
// response.
// Creates the URLRequest instance for fetching the URL from the log
// (supplied as |request_url| in the c'tor) and implements the
// URLRequest::Delegate interface for assembling the response.
class LogFetcher : public net::URLRequest::Delegate {
 public:
  using FailureCallback = base::Callback<void(int, int)>;

  LogFetcher(net::URLRequestContext* request_context,
             const GURL& request_url,
             const base::Closure& success_callback,
             const FailureCallback& failure_callback);
  ~LogFetcher() override {}

  // net::URLRequest::Delegate
  void OnResponseStarted(net::URLRequest* request) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;

  const std::string& assembled_response() const { return assembled_response_; }

 private:
  // Handles the final result of a URLRequest::Read call on the request.
  // Returns true if another read should be started, false if the read
  // failed completely or we have to wait for OnResponseStarted to
  // be called.
  bool HandleReadResult(int bytes_read);

  // Calls URLRequest::Read on |request| repeatedly, until HandleReadResult
  // indicates it should no longer be called. Usually this would be when there
  // is pending IO that requires waiting for OnResponseStarted to be called.
  void StartNextReadLoop();

  // Invokes the success callback. After this method is called, the LogFetcher
  // is deleted and no longer safe to call.
  void RequestComplete();

  // Invokes the failure callback with the supplied error information.
  // After this method the LogFetcher is deleted and no longer safe to call.
  void InvokeFailureCallback(int net_error, int http_response_code);

  std::unique_ptr<net::URLRequest> url_request_;
  const GURL request_url_;
  base::Closure success_callback_;
  FailureCallback failure_callback_;
  scoped_refptr<net::IOBufferWithSize> response_buffer_;
  std::string assembled_response_;

  DISALLOW_COPY_AND_ASSIGN(LogFetcher);
};

LogFetcher::LogFetcher(net::URLRequestContext* request_context,
                       const GURL& request_url,
                       const base::Closure& success_callback,
                       const FailureCallback& failure_callback)
    : request_url_(request_url),
      success_callback_(success_callback),
      failure_callback_(failure_callback) {
  DCHECK(request_url_.SchemeIsHTTPOrHTTPS());
  url_request_ =
      request_context->CreateRequest(request_url_, net::DEFAULT_PRIORITY, this);
  // This request should not send any cookies or otherwise identifying data,
  // as CT logs are expected to be publicly-accessible and connections to them
  // stateless.
  url_request_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);

  url_request_->Start();
}

void LogFetcher::OnResponseStarted(net::URLRequest* request) {
  DCHECK_EQ(url_request_.get(), request);
  int http_response_code = request->GetResponseCode();

  if (!request->status().is_success()) {
    InvokeFailureCallback(request->status().error(), http_response_code);
    return;
  }

  if (http_response_code != net::HTTP_OK) {
    InvokeFailureCallback(net::OK, http_response_code);
    return;
  }

  // Lazily initialize |response_buffer_| to avoid consuming memory until an
  // actual response has been received.
  if (!response_buffer_) {
    response_buffer_ =
        new net::IOBufferWithSize(LogProofFetcher::kMaxLogResponseSizeInBytes);
  }

  StartNextReadLoop();
}

void LogFetcher::OnReadCompleted(net::URLRequest* request, int bytes_read) {
  DCHECK_EQ(url_request_.get(), request);

  if (HandleReadResult(bytes_read))
    StartNextReadLoop();
}

bool LogFetcher::HandleReadResult(int bytes_read) {
  // Start by checking for an error condition.
  // If there are errors, invoke the failure callback and clean up the
  // request.
  if (!url_request_->status().is_success() || bytes_read < 0) {
    int net_error = url_request_->status().error();
    if (net_error == net::OK)
      net_error = net::URLRequestStatus::FAILED;

    InvokeFailureCallback(net_error, net::HTTP_OK);
    return false;
  }

  // Not an error, but no data available, so wait for OnReadCompleted
  // callback.
  if (url_request_->status().is_io_pending())
    return false;

  // Nothing more to read from the stream - finish handling the response.
  if (bytes_read == 0) {
    RequestComplete();
    return false;
  }

  // Data is available, collect it and indicate another read is needed.
  DCHECK_GE(bytes_read, 0);
  // |bytes_read| is non-negative at this point, casting to size_t should be
  // safe.
  if (base::checked_cast<size_t>(bytes_read) >
          LogProofFetcher::kMaxLogResponseSizeInBytes ||
      LogProofFetcher::kMaxLogResponseSizeInBytes <
          (assembled_response_.size() + bytes_read)) {
    // Log response is too big, invoke the failure callback.
    InvokeFailureCallback(net::ERR_FILE_TOO_BIG, net::HTTP_OK);
    return false;
  }

  assembled_response_.append(response_buffer_->data(), bytes_read);
  return true;
}

void LogFetcher::StartNextReadLoop() {
  bool continue_reading = true;
  while (continue_reading) {
    int read_bytes = 0;
    url_request_->Read(response_buffer_.get(), response_buffer_->size(),
                       &read_bytes);
    continue_reading = HandleReadResult(read_bytes);
  }
}

void LogFetcher::RequestComplete() {
  // Get rid of the buffer as it really isn't necessary.
  response_buffer_ = nullptr;
  base::ResetAndReturn(&success_callback_).Run();
  // NOTE: |this| is not valid after invoking the callback, as the LogFetcher
  // instance will be deleted by the callback.
}

void LogFetcher::InvokeFailureCallback(int net_error, int http_response_code) {
  base::ResetAndReturn(&failure_callback_).Run(net_error, http_response_code);
  // NOTE: |this| is not valid after this callback, as the LogFetcher instance
  // invoking the callback will be deleted by the callback.
}

}  // namespace

// Interface for handling the response from a CT log for a particular
// request.
// All log responses are JSON and should be parsed; however the response
// to each request should be parsed and validated differently.
//
// LogResponseHandler instances should be deleted by the |done_callback| when
// it is invoked.
class LogResponseHandler {
 public:
  using DoneCallback = base::Callback<void(const base::Closure&)>;

  // |log_id| will be passed to the |failure_callback| to indicate which log
  // failures pretain to.
  LogResponseHandler(
      const std::string& log_id,
      const LogProofFetcher::FetchFailedCallback& failure_callback);
  virtual ~LogResponseHandler();

  // Starts the actual fetching from the URL, storing |done_callback| for
  // invocation when fetching and parsing of the request finished.
  // It is safe, and expected, to delete this object in the |done_callback|.
  void StartFetch(net::URLRequestContext* request_context,
                  const GURL& request_url,
                  const DoneCallback& done_callback);

  // Handle successful fetch by the LogFetcher (by parsing the JSON and
  // handing the parsed JSON to HandleParsedJson, which is request-specific).
  void HandleFetchCompletion();

  // Handle network failure to complete the request to the log, by invoking
  // the |done_callback_|.
  virtual void HandleNetFailure(int net_error, int http_response_code);

 protected:
  // Handle successful parsing of JSON by invoking HandleParsedJson, then
  // invoking the |done_callback_| with the returned Closure.
  void OnJsonParseSuccess(std::unique_ptr<base::Value> parsed_json);

  // Handle failure to parse the JSON by invoking HandleJsonParseFailure, then
  // invoking the |done_callback_| with the returned Closure.
  void OnJsonParseError(const std::string& error);

  // Handle respones JSON that parsed successfully, usually by
  // returning the success callback bound to parsed values as a Closure.
  virtual base::Closure HandleParsedJson(const base::Value& parsed_json) = 0;

  // Handle failure to parse response JSON, usually by returning the failure
  // callback bound to a request-specific net error code.
  virtual base::Closure HandleJsonParseFailure(
      const std::string& json_error) = 0;

  const std::string log_id_;
  LogProofFetcher::FetchFailedCallback failure_callback_;
  std::unique_ptr<LogFetcher> fetcher_;
  DoneCallback done_callback_;

  base::WeakPtrFactory<LogResponseHandler> weak_factory_;
};

LogResponseHandler::LogResponseHandler(
    const std::string& log_id,
    const LogProofFetcher::FetchFailedCallback& failure_callback)
    : log_id_(log_id),
      failure_callback_(failure_callback),
      fetcher_(nullptr),
      weak_factory_(this) {
  DCHECK(!failure_callback_.is_null());
}

LogResponseHandler::~LogResponseHandler() {}

void LogResponseHandler::StartFetch(net::URLRequestContext* request_context,
                                    const GURL& request_url,
                                    const DoneCallback& done_callback) {
  done_callback_ = done_callback;
  fetcher_.reset(
      new LogFetcher(request_context, request_url,
                     base::Bind(&LogResponseHandler::HandleFetchCompletion,
                                weak_factory_.GetWeakPtr()),
                     base::Bind(&LogResponseHandler::HandleNetFailure,
                                weak_factory_.GetWeakPtr())));
}

void LogResponseHandler::HandleFetchCompletion() {
  safe_json::SafeJsonParser::Parse(
      fetcher_->assembled_response(),
      base::Bind(&LogResponseHandler::OnJsonParseSuccess,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&LogResponseHandler::OnJsonParseError,
                 weak_factory_.GetWeakPtr()));

  // The assembled_response string is copied into the SafeJsonParser so it
  // is safe to get rid of the object that owns it.
  fetcher_.reset();
}

void LogResponseHandler::HandleNetFailure(int net_error,
                                          int http_response_code) {
  fetcher_.reset();
  LogProofFetcher::FetchFailedCallback failure_callback =
      base::ResetAndReturn(&failure_callback_);

  base::ResetAndReturn(&done_callback_)
      .Run(
          base::Bind(failure_callback, log_id_, net_error, http_response_code));
  // NOTE: |this| is not valid after the |done_callback_| is invoked.
}

void LogResponseHandler::OnJsonParseSuccess(
    std::unique_ptr<base::Value> parsed_json) {
  base::ResetAndReturn(&done_callback_).Run(HandleParsedJson(*parsed_json));
  // NOTE: |this| is not valid after the |done_callback_| is invoked.
}

void LogResponseHandler::OnJsonParseError(const std::string& error) {
  base::ResetAndReturn(&done_callback_).Run(HandleJsonParseFailure(error));
  // NOTE: |this| is not valid after the |done_callback_| is invoked.
}

class GetSTHLogResponseHandler : public LogResponseHandler {
 public:
  GetSTHLogResponseHandler(
      const std::string& log_id,
      const LogProofFetcher::SignedTreeHeadFetchedCallback& sth_fetch_callback,
      const LogProofFetcher::FetchFailedCallback& failure_callback)
      : LogResponseHandler(log_id, failure_callback),
        sth_fetched_(sth_fetch_callback) {}

  // Parses the JSON into a net::ct::SignedTreeHead and, if successful,
  // invokes the success callback with it. Otherwise, invokes the failure
  // callback indicating the error that occurred.
  base::Closure HandleParsedJson(const base::Value& parsed_json) override {
    net::ct::SignedTreeHead signed_tree_head;
    if (!net::ct::FillSignedTreeHead(parsed_json, &signed_tree_head)) {
      return base::Bind(base::ResetAndReturn(&failure_callback_), log_id_,
                        net::ERR_CT_STH_INCOMPLETE, net::HTTP_OK);
    }
    // The log id is not a part of the response, fill in manually.
    signed_tree_head.log_id = log_id_;

    return base::Bind(base::ResetAndReturn(&sth_fetched_), log_id_,
                      signed_tree_head);
  }

  // Invoke the error callback indicating that STH parsing failed.
  base::Closure HandleJsonParseFailure(const std::string& json_error) override {
    return base::Bind(base::ResetAndReturn(&failure_callback_), log_id_,
                      net::ERR_CT_STH_PARSING_FAILED, net::HTTP_OK);
  }

 private:
  LogProofFetcher::SignedTreeHeadFetchedCallback sth_fetched_;
};

class GetConsistencyProofLogResponseHandler : public LogResponseHandler {
 public:
  GetConsistencyProofLogResponseHandler(
      const std::string& log_id,
      const LogProofFetcher::ConsistencyProofFetchedCallback&
          proof_fetch_callback,
      const LogProofFetcher::FetchFailedCallback& failure_callback)
      : LogResponseHandler(log_id, failure_callback),
        proof_fetched_(proof_fetch_callback) {}

  // Fills a vector of strings with nodes from the received consistency proof
  // in |parsed_json|, and, if successful, invokes the success callback with the
  // vector. Otherwise, invokes the failure callback indicating proof parsing
  // has failed.
  base::Closure HandleParsedJson(const base::Value& parsed_json) override {
    std::vector<std::string> consistency_proof;
    if (!net::ct::FillConsistencyProof(parsed_json, &consistency_proof)) {
      return base::Bind(base::ResetAndReturn(&failure_callback_), log_id_,
                        net::ERR_CT_CONSISTENCY_PROOF_PARSING_FAILED,
                        net::HTTP_OK);
    }

    return base::Bind(base::ResetAndReturn(&proof_fetched_), log_id_,
                      consistency_proof);
  }

  // Invoke the error callback indicating proof fetching failed.
  base::Closure HandleJsonParseFailure(const std::string& json_error) override {
    return base::Bind(base::ResetAndReturn(&failure_callback_), log_id_,
                      net::ERR_CT_CONSISTENCY_PROOF_PARSING_FAILED,
                      net::HTTP_OK);
  }

 private:
  LogProofFetcher::ConsistencyProofFetchedCallback proof_fetched_;
};

LogProofFetcher::LogProofFetcher(net::URLRequestContext* request_context)
    : request_context_(request_context), weak_factory_(this) {
  DCHECK(request_context);
}

LogProofFetcher::~LogProofFetcher() {
  STLDeleteContainerPointers(inflight_fetches_.begin(),
                             inflight_fetches_.end());
}

void LogProofFetcher::FetchSignedTreeHead(
    const GURL& base_log_url,
    const std::string& log_id,
    const SignedTreeHeadFetchedCallback& fetched_callback,
    const FetchFailedCallback& failed_callback) {
  GURL request_url = base_log_url.Resolve("ct/v1/get-sth");
  StartFetch(request_url, new GetSTHLogResponseHandler(log_id, fetched_callback,
                                                       failed_callback));
}

void LogProofFetcher::FetchConsistencyProof(
    const GURL& base_log_url,
    const std::string& log_id,
    uint64_t old_tree_size,
    uint64_t new_tree_size,
    const ConsistencyProofFetchedCallback& fetched_callback,
    const FetchFailedCallback& failed_callback) {
  GURL request_url = base_log_url.Resolve(base::StringPrintf(
      "ct/v1/get-sth-consistency?first=%" PRIu64 "&second=%" PRIu64,
      old_tree_size, new_tree_size));
  StartFetch(request_url, new GetConsistencyProofLogResponseHandler(
                              log_id, fetched_callback, failed_callback));
}

void LogProofFetcher::StartFetch(const GURL& request_url,
                                 LogResponseHandler* log_request) {
  log_request->StartFetch(request_context_, request_url,
                          base::Bind(&LogProofFetcher::OnFetchDone,
                                     weak_factory_.GetWeakPtr(), log_request));
  inflight_fetches_.insert(log_request);
}

void LogProofFetcher::OnFetchDone(LogResponseHandler* log_handler,
                                  const base::Closure& requestor_callback) {
  auto it = inflight_fetches_.find(log_handler);
  DCHECK(it != inflight_fetches_.end());

  delete *it;
  inflight_fetches_.erase(it);
  requestor_callback.Run();
}

}  // namespace certificate_transparency
