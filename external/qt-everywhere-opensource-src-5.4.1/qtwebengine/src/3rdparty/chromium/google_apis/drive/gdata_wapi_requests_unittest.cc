// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "google_apis/drive/dummy_auth_service.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/gdata_wapi_requests.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"
#include "google_apis/drive/request_sender.h"
#include "google_apis/drive/test_util.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestUserAgent[] = "test-user-agent";

class GDataWapiRequestsTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        message_loop_.message_loop_proxy());

    request_sender_.reset(new RequestSender(new DummyAuthService,
                                            request_context_getter_.get(),
                                            message_loop_.message_loop_proxy(),
                                            kTestUserAgent));

    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
    test_server_.RegisterRequestHandler(
        base::Bind(&GDataWapiRequestsTest::HandleResourceFeedRequest,
                   base::Unretained(this)));

    GURL test_base_url = test_util::GetBaseUrlForTesting(test_server_.port());
    url_generator_.reset(new GDataWapiUrlGenerator(test_base_url));
  }

 protected:
  // Handles a request for fetching a resource feed.
  scoped_ptr<net::test_server::HttpResponse> HandleResourceFeedRequest(
      const net::test_server::HttpRequest& request) {
    http_request_ = request;

    const GURL absolute_url = test_server_.GetURL(request.relative_url);
    std::string remaining_path;

    if (!test_util::RemovePrefix(absolute_url.path(),
                                 "/feeds/default/private/full",
                                 &remaining_path)) {
      return scoped_ptr<net::test_server::HttpResponse>();
    }

    // Process a feed for a single resource ID.
    const std::string resource_id = net::UnescapeURLComponent(
        remaining_path.substr(1), net::UnescapeRule::URL_SPECIAL_CHARS);
    if (resource_id == "file:2_file_resource_id") {
      scoped_ptr<net::test_server::BasicHttpResponse> result(
          test_util::CreateHttpResponseFromFile(
              test_util::GetTestFilePath("gdata/file_entry.json")));
      return result.PassAs<net::test_server::HttpResponse>();
    } else if (resource_id == "invalid_resource_id") {
      // Check if this is an authorization request for an app.
      // This emulates to return invalid formatted result from the server.
      if (request.method == net::test_server::METHOD_PUT &&
          request.content.find("<docs:authorizedApp>") != std::string::npos) {
        scoped_ptr<net::test_server::BasicHttpResponse> result(
            test_util::CreateHttpResponseFromFile(
                test_util::GetTestFilePath("gdata/testfile.txt")));
        return result.PassAs<net::test_server::HttpResponse>();
      }
    }

    return scoped_ptr<net::test_server::HttpResponse>();
  }

  base::MessageLoopForIO message_loop_;  // Test server needs IO thread.
  net::test_server::EmbeddedTestServer test_server_;
  scoped_ptr<RequestSender> request_sender_;
  scoped_ptr<GDataWapiUrlGenerator> url_generator_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;

  // The incoming HTTP request is saved so tests can verify the request
  // parameters like HTTP method (ex. some requests should use DELETE
  // instead of GET).
  net::test_server::HttpRequest http_request_;
};

}  // namespace

TEST_F(GDataWapiRequestsTest, GetResourceEntryRequest_ValidResourceId) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  {
    base::RunLoop run_loop;
    GetResourceEntryRequest* request = new GetResourceEntryRequest(
        request_sender_.get(),
        *url_generator_,
        "file:2_file_resource_id",  // resource ID
        GURL(),  // embed origin
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_SUCCESS, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/file%3A2_file_resource_id"
            "?v=3&alt=json&showroot=true",
            http_request_.relative_url);
  scoped_ptr<base::Value> expected_json =
      test_util::LoadJSONFile("gdata/file_entry.json");
  ASSERT_TRUE(expected_json);
  EXPECT_TRUE(result_data);
  EXPECT_TRUE(base::Value::Equals(expected_json.get(), result_data.get()));
}

TEST_F(GDataWapiRequestsTest, GetResourceEntryRequest_InvalidResourceId) {
  GDataErrorCode result_code = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> result_data;

  {
    base::RunLoop run_loop;
    GetResourceEntryRequest* request = new GetResourceEntryRequest(
        request_sender_.get(),
        *url_generator_,
        "<invalid>",  // resource ID
        GURL(),  // embed origin
        test_util::CreateQuitCallback(
            &run_loop,
            test_util::CreateCopyResultCallback(&result_code, &result_data)));
    request_sender_->StartRequestWithRetry(request);
    run_loop.Run();
  }

  EXPECT_EQ(HTTP_NOT_FOUND, result_code);
  EXPECT_EQ(net::test_server::METHOD_GET, http_request_.method);
  EXPECT_EQ("/feeds/default/private/full/%3Cinvalid%3E?v=3&alt=json"
            "&showroot=true",
            http_request_.relative_url);
  ASSERT_FALSE(result_data);
}

}  // namespace google_apis
