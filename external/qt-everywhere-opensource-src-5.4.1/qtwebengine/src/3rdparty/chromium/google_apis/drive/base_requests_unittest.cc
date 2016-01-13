// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/base_requests.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/drive/test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeUrlFetchRequest : public UrlFetchRequestBase {
 public:
  explicit FakeUrlFetchRequest(RequestSender* sender,
                               const EntryActionCallback& callback,
                               const GURL& url)
      : UrlFetchRequestBase(sender),
        callback_(callback),
        url_(url) {
  }

  virtual ~FakeUrlFetchRequest() {
  }

 protected:
  virtual GURL GetURL() const OVERRIDE { return url_; }
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE {
    callback_.Run(GetErrorCode());
  }
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE {
    callback_.Run(code);
  }

  EntryActionCallback callback_;
  GURL url_;
};

class FakeGetDataRequest : public GetDataRequest {
 public:
  explicit FakeGetDataRequest(RequestSender* sender,
                              const GetDataCallback& callback,
                              const GURL& url)
      : GetDataRequest(sender, callback),
        url_(url) {
  }

  virtual ~FakeGetDataRequest() {
  }

 protected:
  virtual GURL GetURL() const OVERRIDE { return url_; }

  GURL url_;
};

}  // namespace

class BaseRequestsTest : public testing::Test {
 public:
  BaseRequestsTest() : response_code_(net::HTTP_OK) {}

  virtual void SetUp() OVERRIDE {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.message_loop_proxy());

    sender_.reset(new RequestSender(new DummyAuthService,
                                    request_context_getter_.get(),
                                    message_loop_.message_loop_proxy(),
                                    std::string() /* custom user agent */));

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&BaseRequestsTest::HandleRequest, base::Unretained(this)));
  }

  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(response_code_);
    response->set_content(response_body_);
    response->set_content_type("application/json");
    return response.PassAs<net::test_server::HttpResponse>();
  }

  base::MessageLoopForIO message_loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  scoped_ptr<RequestSender> sender_;
  net::test_server::EmbeddedTestServer test_server_;

  net::HttpStatusCode response_code_;
  std::string response_body_;
};

TEST_F(BaseRequestsTest, ParseValidJson) {
  scoped_ptr<base::Value> json;
  ParseJson(message_loop_.message_loop_proxy(),
            kValidJsonString,
            base::Bind(test_util::CreateCopyResultCallback(&json)));
  base::RunLoop().RunUntilIdle();

  base::DictionaryValue* root_dict = NULL;
  ASSERT_TRUE(json);
  ASSERT_TRUE(json->GetAsDictionary(&root_dict));

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("test", &int_value));
  EXPECT_EQ(123, int_value);
}

TEST_F(BaseRequestsTest, ParseInvalidJson) {
  // Initialize with a valid pointer to verify that null is indeed assigned.
  scoped_ptr<base::Value> json(base::Value::CreateNullValue());
  ParseJson(message_loop_.message_loop_proxy(),
            kInvalidJsonString,
            base::Bind(test_util::CreateCopyResultCallback(&json)));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(json);
}

TEST_F(BaseRequestsTest, UrlFetchRequestBaseResponseCodeOverride) {
  response_code_ = net::HTTP_FORBIDDEN;
  response_body_ =
      "{\"error\": {\n"
      "  \"errors\": [\n"
      "   {\n"
      "    \"domain\": \"usageLimits\",\n"
      "    \"reason\": \"rateLimitExceeded\",\n"
      "    \"message\": \"Rate Limit Exceeded\"\n"
      "   }\n"
      "  ],\n"
      "  \"code\": 403,\n"
      "  \"message\": \"Rate Limit Exceeded\"\n"
      " }\n"
      "}\n";

  GDataErrorCode error = GDATA_OTHER_ERROR;
  base::RunLoop run_loop;
  sender_->StartRequestWithRetry(
      new FakeUrlFetchRequest(
          sender_.get(),
          test_util::CreateQuitCallback(
              &run_loop, test_util::CreateCopyResultCallback(&error)),
          test_server_.base_url()));
  run_loop.Run();

  // HTTP_FORBIDDEN (403) is overridden by the error reason.
  EXPECT_EQ(HTTP_SERVICE_UNAVAILABLE, error);
}

TEST_F(BaseRequestsTest, GetDataRequestParseValidResponse) {
  response_body_ = kValidJsonString;

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;
  base::RunLoop run_loop;
  sender_->StartRequestWithRetry(
      new FakeGetDataRequest(
          sender_.get(),
          test_util::CreateQuitCallback(
              &run_loop, test_util::CreateCopyResultCallback(&error, &value)),
          test_server_.base_url()));
  run_loop.Run();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(value);
}

TEST_F(BaseRequestsTest, GetDataRequestParseInvalidResponse) {
  response_body_ = kInvalidJsonString;

  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;
  base::RunLoop run_loop;
  sender_->StartRequestWithRetry(
      new FakeGetDataRequest(
          sender_.get(),
          test_util::CreateQuitCallback(
              &run_loop, test_util::CreateCopyResultCallback(&error, &value)),
          test_server_.base_url()));
  run_loop.Run();

  EXPECT_EQ(GDATA_PARSE_ERROR, error);
  EXPECT_FALSE(value);
}

}  // namespace google_apis
