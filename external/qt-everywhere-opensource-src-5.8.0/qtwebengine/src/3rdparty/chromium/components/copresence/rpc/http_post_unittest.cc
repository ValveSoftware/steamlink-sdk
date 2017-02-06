// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/http_post.h"

#include "base/test/test_simple_task_runner.h"
#include "components/copresence/proto/data.pb.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kFakeServerHost[] = "test.server.google.com";
const char kRPCName[] = "testRpc";
const char kTracingToken[] = "trace me!";
const char kApiKey[] = "unlock ALL the APIz";
const char kAuthToken[] = "oogabooga";

}  // namespace

using google::protobuf::MessageLite;

namespace copresence {

class HttpPostTest : public testing::Test {
 public:
  HttpPostTest() {
    context_getter_ = new net::TestURLRequestContextGetter(
        make_scoped_refptr(new base::TestSimpleTaskRunner));
    proto_.set_client("test_client");
    proto_.set_version_code(123);
  }
  ~HttpPostTest() override {}

  // Check that the correct response was sent.
  void TestResponseCallback(int expected_response_code,
                            const std::string& expected_response,
                            int actual_response_code,
                            const std::string& actual_response) {
    CHECK_EQ(expected_response_code, actual_response_code);
    CHECK_EQ(expected_response, actual_response);
  }

 protected:
  void CheckPassthrough(int response_code, const std::string& response) {
    HttpPost* post = new HttpPost(
        context_getter_.get(), std::string("http://") + kFakeServerHost,
        kRPCName, kApiKey,
        "",  // auth token
        "",  // tracing token
        proto_);
    post->Start(base::Bind(&HttpPostTest::TestResponseCallback,
                           base::Unretained(this),
                           response_code,
                           response));

    net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(
        HttpPost::kUrlFetcherId);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  net::TestURLFetcher* GetFetcher() {
    return fetcher_factory_.GetFetcherByID(HttpPost::kUrlFetcherId);
  }

  const std::string GetApiKeySent() {
    std::string api_key_sent;
    net::GetValueForKeyInQuery(GetFetcher()->GetOriginalURL(),
                               HttpPost::kApiKeyField,
                               &api_key_sent);
    return api_key_sent;
  }

  const std::string GetAuthHeaderSent() {
    net::HttpRequestHeaders headers;
    std::string header;
    GetFetcher()->GetExtraRequestHeaders(&headers);
    return headers.GetHeader("Authorization", &header) ? header : "";
  }

  const std::string GetTracingTokenSent() {
    std::string tracing_token_sent;
    net::GetValueForKeyInQuery(GetFetcher()->GetOriginalURL(),
                               HttpPost::kTracingField,
                               &tracing_token_sent);
    return tracing_token_sent;
  }

  net::TestURLFetcherFactory fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> context_getter_;

  ClientVersion proto_;
};

TEST_F(HttpPostTest, OKResponse) {
  // "Send" the proto to the "server".
  HttpPost* post = new HttpPost(context_getter_.get(),
                                std::string("http://") + kFakeServerHost,
                                kRPCName,
                                kApiKey,
                                kAuthToken,
                                kTracingToken,
                                proto_);
  post->Start(base::Bind(&HttpPostTest::TestResponseCallback,
                         base::Unretained(this),
                         net::HTTP_OK,
                         "Hello World!"));

  // Verify that the data was sent to the right place.
  GURL requested_url = GetFetcher()->GetOriginalURL();
  EXPECT_EQ(kFakeServerHost, requested_url.host());
  EXPECT_EQ(std::string("/") + kRPCName, requested_url.path());

  // Check parameters.
  EXPECT_EQ("", GetApiKeySent());  // No API key when using an auth token.
  EXPECT_EQ(std::string("Bearer ") + kAuthToken, GetAuthHeaderSent());
  EXPECT_EQ(std::string("token:") + kTracingToken, GetTracingTokenSent());

  // Verify that the right data was sent.
  std::string upload_data;
  ASSERT_TRUE(proto_.SerializeToString(&upload_data));
  EXPECT_EQ(upload_data, GetFetcher()->upload_data());

  // Send a response and check that it's passed along correctly.
  GetFetcher()->set_response_code(net::HTTP_OK);
  GetFetcher()->SetResponseString("Hello World!");
  GetFetcher()->delegate()->OnURLFetchComplete(GetFetcher());
}

TEST_F(HttpPostTest, ErrorResponse) {
  CheckPassthrough(net::HTTP_BAD_REQUEST, "Bad client. Shame on you.");
  CheckPassthrough(net::HTTP_INTERNAL_SERVER_ERROR, "I'm dying. Forgive me.");
  CheckPassthrough(-1, "");
}

}  // namespace copresence
