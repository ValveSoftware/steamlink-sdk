// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/generic_url_request_job.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "headless/public/util/expedited_dispatcher.h"
#include "headless/public/util/testing/generic_url_request_mocks.h"
#include "headless/public/util/url_fetcher.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

std::ostream& operator<<(std::ostream& os, const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  os << json;
  return os;
}

MATCHER_P(MatchesJson, json, json.c_str()) {
  std::unique_ptr<base::Value> expected(
      base::JSONReader::Read(json, base::JSON_PARSE_RFC));
  return arg.Equals(expected.get());
}

namespace headless {

namespace {

class MockFetcher : public URLFetcher {
 public:
  MockFetcher(base::DictionaryValue* fetch_request,
              const std::string& json_reply)
      : fetch_reply_(base::JSONReader::Read(json_reply, base::JSON_PARSE_RFC)),
        fetch_request_(fetch_request) {
    CHECK(fetch_reply_) << "Invalid json: " << json_reply;
  }

  ~MockFetcher() override {}

  void StartFetch(const GURL& url,
                  const std::string& method,
                  const net::HttpRequestHeaders& request_headers,
                  ResultListener* result_listener) override {
    // Record the request.
    fetch_request_->SetString("url", url.spec());
    fetch_request_->SetString("method", method);
    std::unique_ptr<base::DictionaryValue> headers(new base::DictionaryValue);
    for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();) {
      headers->SetString(it.name(), it.value());
    }
    fetch_request_->Set("headers", std::move(headers));

    // Return the canned response.
    base::DictionaryValue* reply_dictionary;
    ASSERT_TRUE(fetch_reply_->GetAsDictionary(&reply_dictionary));
    std::string final_url;
    ASSERT_TRUE(reply_dictionary->GetString("url", &final_url));
    int http_response_code;
    ASSERT_TRUE(reply_dictionary->GetInteger("http_response_code",
                                             &http_response_code));
    ASSERT_TRUE(reply_dictionary->GetString("data", &response_data_));
    base::DictionaryValue* reply_headers_dictionary;
    ASSERT_TRUE(
        reply_dictionary->GetDictionary("headers", &reply_headers_dictionary));
    scoped_refptr<net::HttpResponseHeaders> response_headers(
        new net::HttpResponseHeaders(""));
    for (base::DictionaryValue::Iterator it(*reply_headers_dictionary);
         !it.IsAtEnd(); it.Advance()) {
      std::string value;
      ASSERT_TRUE(it.value().GetAsString(&value));
      response_headers->AddHeader(
          base::StringPrintf("%s: %s", it.key().c_str(), value.c_str()));
    }

    result_listener->OnFetchComplete(
        GURL(final_url), http_response_code, std::move(response_headers),
        response_data_.c_str(), response_data_.size());
  }

 private:
  std::unique_ptr<base::Value> fetch_reply_;
  base::DictionaryValue* fetch_request_;  // NOT OWNED
  std::string response_data_;  // Here to ensure the required lifetime.
};

class MockProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  // Details of the fetch will be stored in |fetch_request|.
  // The fetch response will be created from parsing |json_fetch_reply_|.
  MockProtocolHandler(base::DictionaryValue* fetch_request,
                      std::string* json_fetch_reply,
                      URLRequestDispatcher* dispatcher,
                      GenericURLRequestJob::Delegate* job_delegate)
      : fetch_request_(fetch_request),
        json_fetch_reply_(json_fetch_reply),
        job_delegate_(job_delegate),
        dispatcher_(dispatcher) {}

  // net::URLRequestJobFactory::ProtocolHandler override.
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new GenericURLRequestJob(
        request, network_delegate, dispatcher_,
        base::MakeUnique<MockFetcher>(fetch_request_, *json_fetch_reply_),
        job_delegate_);
  }

 private:
  base::DictionaryValue* fetch_request_;          // NOT OWNED
  std::string* json_fetch_reply_;                 // NOT OWNED
  GenericURLRequestJob::Delegate* job_delegate_;  // NOT OWNED
  URLRequestDispatcher* dispatcher_;              // NOT OWNED
};

}  // namespace

class GenericURLRequestJobTest : public testing::Test {
 public:
  GenericURLRequestJobTest() : dispatcher_(message_loop_.task_runner()) {
    url_request_job_factory_.SetProtocolHandler(
        "https", base::WrapUnique(new MockProtocolHandler(
                     &fetch_request_, &json_fetch_reply_, &dispatcher_,
                     &job_delegate_)));
    url_request_context_.set_job_factory(&url_request_job_factory_);
    url_request_context_.set_cookie_store(&cookie_store_);
  }

  std::unique_ptr<net::URLRequest> CreateAndCompleteJob(
      const GURL& url,
      const std::string& json_reply) {
    json_fetch_reply_ = json_reply;

    std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
        url, net::DEFAULT_PRIORITY, &request_delegate_));
    request->Start();
    base::RunLoop().RunUntilIdle();
    return request;
  }

 protected:
  base::MessageLoop message_loop_;
  ExpeditedDispatcher dispatcher_;

  net::URLRequestJobFactoryImpl url_request_job_factory_;
  net::URLRequestContext url_request_context_;
  MockCookieStore cookie_store_;

  MockURLRequestDelegate request_delegate_;
  base::DictionaryValue fetch_request_;  // The request sent to MockFetcher.
  std::string json_fetch_reply_;         // The reply to be sent by MockFetcher.
  MockGenericURLRequestJobDelegate job_delegate_;
};

TEST_F(GenericURLRequestJobTest, BasicRequestParams) {
  // TODO(alexclarke): Lobby for raw string literals and use them here!
  json_fetch_reply_ =
      "{\"url\":\"https://example.com\","
      " \"http_response_code\":200,"
      " \"data\":\"Reply\","
      " \"headers\":{\"Content-Type\":\"text/plain\"}}";

  std::unique_ptr<net::URLRequest> request(url_request_context_.CreateRequest(
      GURL("https://example.com"), net::DEFAULT_PRIORITY, &request_delegate_));
  request->SetReferrer("https://referrer.example.com");
  request->SetExtraRequestHeaderByName("Extra-Header", "Value", true);
  request->SetExtraRequestHeaderByName("User-Agent", "TestBrowser", true);
  request->SetExtraRequestHeaderByName("Accept", "text/plain", true);
  request->Start();
  base::RunLoop().RunUntilIdle();

  std::string expected_request_json =
      "{\"url\": \"https://example.com/\","
      " \"method\": \"GET\","
      " \"headers\": {"
      "   \"Accept\": \"text/plain\","
      "   \"Cookie\": \"\","
      "   \"Extra-Header\": \"Value\","
      "   \"Referer\": \"https://referrer.example.com/\","
      "   \"User-Agent\": \"TestBrowser\""
      " }"
      "}";

  EXPECT_THAT(fetch_request_, MatchesJson(expected_request_json));
}

TEST_F(GenericURLRequestJobTest, BasicRequestProperties) {
  std::string reply =
      "{\"url\":\"https://example.com\","
      " \"http_response_code\":200,"
      " \"data\":\"Reply\","
      " \"headers\":{\"Content-Type\":\"text/html; charset=UTF-8\"}}";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteJob(GURL("https://example.com"), reply));

  EXPECT_EQ(200, request->GetResponseCode());

  std::string mime_type;
  request->GetMimeType(&mime_type);
  EXPECT_EQ("text/html", mime_type);

  std::string charset;
  request->GetCharset(&charset);
  EXPECT_EQ("utf-8", charset);

  std::string content_type;
  EXPECT_TRUE(request->response_info().headers->GetNormalizedHeader(
      "Content-Type", &content_type));
  EXPECT_EQ("text/html; charset=UTF-8", content_type);
}

TEST_F(GenericURLRequestJobTest, BasicRequestContents) {
  std::string reply =
      "{\"url\":\"https://example.com\","
      " \"http_response_code\":200,"
      " \"data\":\"Reply\","
      " \"headers\":{\"Content-Type\":\"text/html; charset=UTF-8\"}}";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteJob(GURL("https://example.com"), reply));

  const int kBufferSize = 256;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read;
  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(5, bytes_read);
  EXPECT_EQ("Reply", std::string(buffer->data(), 5));

  net::LoadTimingInfo load_timing_info;
  request->GetLoadTimingInfo(&load_timing_info);
  EXPECT_FALSE(load_timing_info.receive_headers_end.is_null());
}

TEST_F(GenericURLRequestJobTest, ReadInParts) {
  std::string reply =
      "{\"url\":\"https://example.com\","
      " \"http_response_code\":200,"
      " \"data\":\"Reply\","
      " \"headers\":{\"Content-Type\":\"text/html; charset=UTF-8\"}}";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteJob(GURL("https://example.com"), reply));

  const int kBufferSize = 3;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kBufferSize));
  int bytes_read;
  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(3, bytes_read);
  EXPECT_EQ("Rep", std::string(buffer->data(), bytes_read));

  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(2, bytes_read);
  EXPECT_EQ("ly", std::string(buffer->data(), bytes_read));

  EXPECT_TRUE(request->Read(buffer.get(), kBufferSize, &bytes_read));
  EXPECT_EQ(0, bytes_read);
}

TEST_F(GenericURLRequestJobTest, RequestWithCookies) {
  net::CookieList* cookies = cookie_store_.cookies();

  // Basic matching cookie.
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://example.com"), "basic_cookie", "1", "example.com", "/",
      base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ false, net::COOKIE_PRIORITY_DEFAULT));

  // Matching secure cookie.
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://example.com"), "secure_cookie", "2", "example.com", "/",
      base::Time(), base::Time(),
      /* secure */ true,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ true, net::COOKIE_PRIORITY_DEFAULT));

  // Matching http-only cookie.
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://example.com"), "http_only_cookie", "3", "example.com", "/",
      base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ true, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ false, net::COOKIE_PRIORITY_DEFAULT));

  // Matching cookie with path.
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://example.com"), "cookie_with_path", "4", "example.com",
      "/widgets", base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ false, net::COOKIE_PRIORITY_DEFAULT));

  // Matching cookie with subdomain.
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://cdn.example.com"), "bad_subdomain_cookie", "5",
      "cdn.example.com", "/", base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ false, net::COOKIE_PRIORITY_DEFAULT));

  // Non-matching cookie (different site).
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://zombo.com"), "bad_site_cookie", "6", "zombo.com", "/",
      base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ false, net::COOKIE_PRIORITY_DEFAULT));

  // Non-matching cookie (different path).
  cookies->push_back(*net::CanonicalCookie::Create(
      GURL("https://example.com"), "bad_path_cookie", "7", "example.com",
      "/gadgets", base::Time(), base::Time(),
      /* secure */ false,
      /* http_only */ false, net::CookieSameSite::NO_RESTRICTION,
      /* enforce_strict_secure */ false, net::COOKIE_PRIORITY_DEFAULT));

  std::string reply =
      "{\"url\":\"https://example.com\","
      " \"http_response_code\":200,"
      " \"data\":\"Reply\","
      " \"headers\":{\"Content-Type\":\"text/html; charset=UTF-8\"}}";

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteJob(GURL("https://example.com"), reply));

  std::string expected_request_json =
      "{\"url\": \"https://example.com/\","
      " \"method\": \"GET\","
      " \"headers\": {"
      "   \"Cookie\": \"basic_cookie=1; secure_cookie=2; http_only_cookie=3\","
      "   \"Referer\": \"\""
      " }"
      "}";

  EXPECT_THAT(fetch_request_, MatchesJson(expected_request_json));
}

TEST_F(GenericURLRequestJobTest, DelegateBlocksLoading) {
  std::string reply =
      "{\"url\":\"https://example.com\","
      " \"http_response_code\":200,"
      " \"data\":\"Reply\","
      " \"headers\":{\"Content-Type\":\"text/html; charset=UTF-8\"}}";

  job_delegate_.SetShouldBlock(true);

  std::unique_ptr<net::URLRequest> request(
      CreateAndCompleteJob(GURL("https://example.com"), reply));

  EXPECT_EQ(net::URLRequestStatus::FAILED, request->status().status());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request->status().error());
}

}  // namespace headless
