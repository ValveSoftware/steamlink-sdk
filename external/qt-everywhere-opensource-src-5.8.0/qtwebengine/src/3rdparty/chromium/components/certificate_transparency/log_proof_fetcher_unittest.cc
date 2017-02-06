// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/log_proof_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/safe_json/testing_json_parser.h"
#include "net/base/net_errors.h"
#include "net/base/network_delegate.h"
#include "net/cert/signed_tree_head.h"
#include "net/http/http_status_code.h"
#include "net/test/ct_test_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace certificate_transparency {

namespace {

const char kGetResponseHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: application/json; charset=ISO-8859-1\n";

const char kGetResponseNotFoundHeaders[] =
    "HTTP/1.1 404 Not Found\n"
    "Content-Type: text/html; charset=iso-8859-1\n";

const char kLogSchema[] = "https";
const char kLogHost[] = "ct.log.example.com";
const char kLogPathPrefix[] = "somelog";
const char kLogID[] = "some_id";

// Gets a dummy consistency proof for the given |node_id|.
std::string GetDummyConsistencyProofNode(uint64_t node_id) {
  // Take the low 8 bits and repeat them as a string. This
  // has no special meaning, other than making it easier to
  // debug which consistency proof was used.
  return std::string(32, static_cast<char>(node_id));
}

// Number of nodes in a dummy consistency proof.
const size_t kDummyConsistencyProofNumNodes = 4;

class LogFetchTestJob : public net::URLRequestTestJob {
 public:
  LogFetchTestJob(const std::string& get_log_data,
                  const std::string& get_log_headers,
                  net::URLRequest* request,
                  net::NetworkDelegate* network_delegate)
      : URLRequestTestJob(request,
                          network_delegate,
                          get_log_headers,
                          get_log_data,
                          true),
        async_io_(false) {}

  void set_async_io(bool async_io) { async_io_ = async_io; }

 private:
  ~LogFetchTestJob() override {}

  bool NextReadAsync() override {
    // Response with indication of async IO only once, otherwise the final
    // Read would (incorrectly) be classified as async, causing the
    // URLRequestJob to try reading another time and failing on a CHECK
    // that the raw_read_buffer_ is not null.
    // According to mmenke@, this is a bug in the URLRequestTestJob code.
    // TODO(eranm): Once said bug is fixed, switch most tests to using async
    // IO.
    if (async_io_) {
      async_io_ = false;
      return true;
    }
    return false;
  }

  bool async_io_;

  DISALLOW_COPY_AND_ASSIGN(LogFetchTestJob);
};

class LogGetResponseHandler : public net::URLRequestInterceptor {
 public:
  LogGetResponseHandler()
      : async_io_(false),
        response_headers_(
            std::string(kGetResponseHeaders, arraysize(kGetResponseHeaders))) {}
  ~LogGetResponseHandler() override {}

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    EXPECT_EQ(expected_url_, request->url());

    LogFetchTestJob* job = new LogFetchTestJob(
        response_body_, response_headers_, request, network_delegate);
    job->set_async_io(async_io_);
    return job;
  }

  void set_response_body(const std::string& response_body) {
    response_body_ = response_body;
  }

  void set_response_headers(const std::string& response_headers) {
    response_headers_ = response_headers;
  }

  void set_async_io(bool async_io) { async_io_ = async_io; }

  void set_expected_url(const GURL& url) { expected_url_ = url; }

 private:
  bool async_io_;
  std::string response_body_;
  std::string response_headers_;

  // Stored for test body to assert on
  GURL expected_url_;

  DISALLOW_COPY_AND_ASSIGN(LogGetResponseHandler);
};

enum InterceptedResultType {
  NOTHING,
  FAILURE,
  STH_FETCH,
  CONSISTENCY_PROOF_FETCH
};

class RecordFetchCallbackInvocations {
 public:
  RecordFetchCallbackInvocations(bool expect_success)
      : expect_success_(expect_success),
        net_error_(net::OK),
        http_response_code_(-1),
        request_type_(NOTHING) {}

  void STHFetched(base::Closure quit_closure,
                  const std::string& log_id,
                  const net::ct::SignedTreeHead& sth) {
    ASSERT_TRUE(expect_success_);
    ASSERT_EQ(NOTHING, request_type_);
    request_type_ = STH_FETCH;
    sth_ = sth;
    log_id_ = log_id;
    quit_closure.Run();
  }

  void ConsistencyProofFetched(
      base::Closure quit_closure,
      const std::string& log_id,
      const std::vector<std::string>& consistency_proof) {
    ASSERT_TRUE(expect_success_);
    ASSERT_EQ(NOTHING, request_type_);
    request_type_ = CONSISTENCY_PROOF_FETCH;
    consistency_proof_.assign(consistency_proof.begin(),
                              consistency_proof.end());
    log_id_ = log_id;
    quit_closure.Run();
  }

  void FetchingFailed(base::Closure quit_closure,
                      const std::string& log_id,
                      int net_error,
                      int http_response_code) {
    ASSERT_FALSE(expect_success_);
    ASSERT_EQ(NOTHING, request_type_);
    request_type_ = FAILURE;
    net_error_ = net_error;
    http_response_code_ = http_response_code;
    if (net_error_ == net::OK) {
      EXPECT_NE(net::HTTP_OK, http_response_code_);
    }

    quit_closure.Run();
  }

  InterceptedResultType intercepted_result_type() const {
    return request_type_;
  }

  int net_error() const { return net_error_; }

  int http_response_code() const { return http_response_code_; }

  const net::ct::SignedTreeHead& intercepted_sth() const { return sth_; }

  const std::string& intercepted_log_id() const { return log_id_; }

  const std::vector<std::string>& intercepted_proof() const {
    return consistency_proof_;
  }

 private:
  const bool expect_success_;
  int net_error_;
  int http_response_code_;
  InterceptedResultType request_type_;
  net::ct::SignedTreeHead sth_;
  std::string log_id_;
  std::vector<std::string> consistency_proof_;
};

class LogProofFetcherTest : public ::testing::Test {
 public:
  LogProofFetcherTest()
      : log_url_(base::StringPrintf("%s://%s/%s/",
                                    kLogSchema,
                                    kLogHost,
                                    kLogPathPrefix)) {
    std::unique_ptr<LogGetResponseHandler> handler(new LogGetResponseHandler());
    handler_ = handler.get();

    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        kLogSchema, kLogHost, std::move(handler));

    fetcher_.reset(new LogProofFetcher(&context_));
  }

  ~LogProofFetcherTest() override {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(kLogSchema,
                                                                kLogHost);
  }

 protected:
  void SetValidSTHJSONResponse() {
    std::string sth_json_reply_data = net::ct::GetSampleSTHAsJson();
    handler_->set_response_body(sth_json_reply_data);
    handler_->set_expected_url(log_url_.Resolve("ct/v1/get-sth"));
  }

  void RunFetcherWithCallback(RecordFetchCallbackInvocations* callback) {
    fetcher_->FetchSignedTreeHead(
        log_url_, kLogID,
        base::Bind(&RecordFetchCallbackInvocations::STHFetched,
                   base::Unretained(callback), run_loop_.QuitClosure()),
        base::Bind(&RecordFetchCallbackInvocations::FetchingFailed,
                   base::Unretained(callback), run_loop_.QuitClosure()));
    run_loop_.Run();
  }

  void RunGetConsistencyFetcherWithCallback(
      RecordFetchCallbackInvocations* callback) {
    const uint64_t kOldTree = 5;
    const uint64_t kNewTree = 8;
    handler_->set_expected_url(log_url_.Resolve(base::StringPrintf(
        "ct/v1/get-sth-consistency?first=%" PRIu64 "&second=%" PRIu64, kOldTree,
        kNewTree)));
    fetcher_->FetchConsistencyProof(
        log_url_, kLogID, kOldTree, kNewTree,
        base::Bind(&RecordFetchCallbackInvocations::ConsistencyProofFetched,
                   base::Unretained(callback), run_loop_.QuitClosure()),
        base::Bind(&RecordFetchCallbackInvocations::FetchingFailed,
                   base::Unretained(callback), run_loop_.QuitClosure()));
    run_loop_.Run();
  }

  void VerifyReceivedSTH(const std::string& log_id,
                         const net::ct::SignedTreeHead& sth) {
    net::ct::SignedTreeHead expected_sth;
    net::ct::GetSampleSignedTreeHead(&expected_sth);

    EXPECT_EQ(kLogID, log_id);
    EXPECT_EQ(expected_sth.version, sth.version);
    EXPECT_EQ(expected_sth.timestamp, sth.timestamp);
    EXPECT_EQ(expected_sth.tree_size, sth.tree_size);
    EXPECT_STREQ(expected_sth.sha256_root_hash, sth.sha256_root_hash);
    EXPECT_EQ(expected_sth.signature.hash_algorithm,
              sth.signature.hash_algorithm);
    EXPECT_EQ(expected_sth.signature.signature_algorithm,
              sth.signature.signature_algorithm);
    EXPECT_EQ(expected_sth.signature.signature_data,
              sth.signature.signature_data);
    EXPECT_EQ(kLogID, sth.log_id);
  }

  void VerifyConsistencyProof(
      const std::string& log_id,
      const std::vector<std::string>& consistency_proof) {
    EXPECT_EQ(kLogID, log_id);
    EXPECT_EQ(kDummyConsistencyProofNumNodes, consistency_proof.size());
    for (uint64_t i = 0; i < kDummyConsistencyProofNumNodes; ++i) {
      EXPECT_EQ(GetDummyConsistencyProofNode(i), consistency_proof[i])
          << " node: " << i;
    }
  }

  // The |message_loop_|, while seemingly unused, is necessary
  // for URL request interception. That is the message loop that
  // will be used by the RunLoop.
  base::MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;
  net::TestURLRequestContext context_;
  safe_json::TestingJsonParser::ScopedFactoryOverride factory_override_;
  std::unique_ptr<LogProofFetcher> fetcher_;
  const GURL log_url_;
  LogGetResponseHandler* handler_;
};

TEST_F(LogProofFetcherTest, TestValidGetReply) {
  SetValidSTHJSONResponse();

  RecordFetchCallbackInvocations callback(true);

  RunFetcherWithCallback(&callback);

  ASSERT_EQ(STH_FETCH, callback.intercepted_result_type());
  VerifyReceivedSTH(callback.intercepted_log_id(), callback.intercepted_sth());
}

TEST_F(LogProofFetcherTest, TestValidGetReplyAsyncIO) {
  SetValidSTHJSONResponse();
  handler_->set_async_io(true);

  RecordFetchCallbackInvocations callback(true);
  RunFetcherWithCallback(&callback);

  ASSERT_EQ(STH_FETCH, callback.intercepted_result_type());
  VerifyReceivedSTH(callback.intercepted_log_id(), callback.intercepted_sth());
}

TEST_F(LogProofFetcherTest, TestInvalidGetReplyIncompleteJSON) {
  std::string sth_json_reply_data = net::ct::CreateSignedTreeHeadJsonString(
      21 /* tree_size */, 123456u /* timestamp */, std::string(),
      std::string());
  handler_->set_response_body(sth_json_reply_data);
  handler_->set_expected_url(log_url_.Resolve("ct/v1/get-sth"));

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_EQ(FAILURE, callback.intercepted_result_type());
  EXPECT_EQ(net::ERR_CT_STH_INCOMPLETE, callback.net_error());
  EXPECT_EQ(net::HTTP_OK, callback.http_response_code());
}

TEST_F(LogProofFetcherTest, TestInvalidGetReplyInvalidJSON) {
  std::string sth_json_reply_data = "{\"tree_size\":21,\"timestamp\":}";
  handler_->set_response_body(sth_json_reply_data);
  handler_->set_expected_url(log_url_.Resolve("ct/v1/get-sth"));

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_EQ(FAILURE, callback.intercepted_result_type());
  EXPECT_EQ(net::ERR_CT_STH_PARSING_FAILED, callback.net_error());
  EXPECT_EQ(net::HTTP_OK, callback.http_response_code());
}

TEST_F(LogProofFetcherTest, TestLogReplyIsTooLong) {
  std::string sth_json_reply_data = net::ct::GetSampleSTHAsJson();
  // Add kMaxLogResponseSizeInBytes to make sure the response is too big.
  sth_json_reply_data.append(
      std::string(LogProofFetcher::kMaxLogResponseSizeInBytes, ' '));
  handler_->set_response_body(sth_json_reply_data);
  handler_->set_expected_url(log_url_.Resolve("ct/v1/get-sth"));

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_EQ(FAILURE, callback.intercepted_result_type());
  EXPECT_EQ(net::ERR_FILE_TOO_BIG, callback.net_error());
  EXPECT_EQ(net::HTTP_OK, callback.http_response_code());
}

TEST_F(LogProofFetcherTest, TestLogReplyIsExactlyMaxSize) {
  std::string sth_json_reply_data = net::ct::GetSampleSTHAsJson();
  // Extend the reply to be exactly kMaxLogResponseSizeInBytes.
  sth_json_reply_data.append(std::string(
      LogProofFetcher::kMaxLogResponseSizeInBytes - sth_json_reply_data.size(),
      ' '));
  handler_->set_response_body(sth_json_reply_data);
  handler_->set_expected_url(log_url_.Resolve("ct/v1/get-sth"));

  RecordFetchCallbackInvocations callback(true);
  RunFetcherWithCallback(&callback);

  ASSERT_EQ(STH_FETCH, callback.intercepted_result_type());
  VerifyReceivedSTH(callback.intercepted_log_id(), callback.intercepted_sth());
}

TEST_F(LogProofFetcherTest, TestLogRepliesWithHttpError) {
  handler_->set_response_headers(std::string(
      kGetResponseNotFoundHeaders, arraysize(kGetResponseNotFoundHeaders)));
  handler_->set_expected_url(log_url_.Resolve("ct/v1/get-sth"));

  RecordFetchCallbackInvocations callback(false);
  RunFetcherWithCallback(&callback);

  ASSERT_EQ(FAILURE, callback.intercepted_result_type());
  EXPECT_EQ(net::OK, callback.net_error());
  EXPECT_EQ(net::HTTP_NOT_FOUND, callback.http_response_code());
}

TEST_F(LogProofFetcherTest, TestValidGetConsistencyValidReply) {
  std::vector<std::string> proof;
  for (uint64_t i = 0; i < kDummyConsistencyProofNumNodes; ++i)
    proof.push_back(GetDummyConsistencyProofNode(i));

  std::string consistency_proof_reply_data =
      net::ct::CreateConsistencyProofJsonString(proof);
  handler_->set_response_body(consistency_proof_reply_data);

  RecordFetchCallbackInvocations callback(true);
  RunGetConsistencyFetcherWithCallback(&callback);

  ASSERT_EQ(CONSISTENCY_PROOF_FETCH, callback.intercepted_result_type());
  VerifyConsistencyProof(callback.intercepted_log_id(),
                         callback.intercepted_proof());
}

TEST_F(LogProofFetcherTest, TestInvalidGetConsistencyReplyInvalidJSON) {
  std::string consistency_proof_reply_data = "{\"consistency\": [1,2]}";
  handler_->set_response_body(consistency_proof_reply_data);

  RecordFetchCallbackInvocations callback(false);
  RunGetConsistencyFetcherWithCallback(&callback);

  ASSERT_EQ(FAILURE, callback.intercepted_result_type());
  EXPECT_EQ(net::ERR_CT_CONSISTENCY_PROOF_PARSING_FAILED, callback.net_error());
  EXPECT_EQ(net::HTTP_OK, callback.http_response_code());
}

}  // namespace

}  // namespace certificate_transparency
