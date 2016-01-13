// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A complete set of unit tests for OAuth2MintTokenFlow.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_fetcher_factory.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::HttpRequestHeaders;
using net::ScopedURLFetcherFactory;
using net::TestURLFetcher;
using net::URLFetcher;
using net::URLFetcherDelegate;
using net::URLFetcherFactory;
using net::URLRequestStatus;
using testing::_;
using testing::Return;

namespace {

static std::string CreateBody() {
  return "some body";
}

static GURL CreateApiUrl() {
  return GURL("https://www.googleapis.com/someapi");
}

static std::vector<std::string> CreateTestScopes() {
  std::vector<std::string> scopes;
  scopes.push_back("scope1");
  scopes.push_back("scope2");
  return scopes;
}

class MockUrlFetcherFactory : public ScopedURLFetcherFactory,
                              public URLFetcherFactory {
 public:
  MockUrlFetcherFactory()
      : ScopedURLFetcherFactory(this) {
  }
  virtual ~MockUrlFetcherFactory() {}

  MOCK_METHOD4(
      CreateURLFetcher,
      URLFetcher* (int id,
                   const GURL& url,
                   URLFetcher::RequestType request_type,
                   URLFetcherDelegate* d));
};

class MockAccessTokenFetcher : public OAuth2AccessTokenFetcherImpl {
 public:
  MockAccessTokenFetcher(OAuth2AccessTokenConsumer* consumer,
                         net::URLRequestContextGetter* getter,
                         const std::string& refresh_token)
      : OAuth2AccessTokenFetcherImpl(consumer, getter, refresh_token) {}
  ~MockAccessTokenFetcher() {}

  MOCK_METHOD3(Start,
               void(const std::string& client_id,
                    const std::string& client_secret,
                    const std::vector<std::string>& scopes));
};

class MockApiCallFlow : public OAuth2ApiCallFlow {
 public:
  MockApiCallFlow(net::URLRequestContextGetter* context,
                  const std::string& refresh_token,
                  const std::string& access_token,
                  const std::vector<std::string>& scopes)
      : OAuth2ApiCallFlow(context, refresh_token, access_token, scopes) {}
  ~MockApiCallFlow() {}

  MOCK_METHOD0(CreateApiCallUrl, GURL ());
  MOCK_METHOD0(CreateApiCallBody, std::string ());
  MOCK_METHOD1(ProcessApiCallSuccess,
      void (const URLFetcher* source));
  MOCK_METHOD1(ProcessApiCallFailure,
      void (const URLFetcher* source));
  MOCK_METHOD1(ProcessNewAccessToken,
      void (const std::string& access_token));
  MOCK_METHOD1(ProcessMintAccessTokenFailure,
      void (const GoogleServiceAuthError& error));
  MOCK_METHOD0(CreateAccessTokenFetcher, OAuth2AccessTokenFetcher* ());
};

}  // namespace

class OAuth2ApiCallFlowTest : public testing::Test {
 protected:
  void SetupAccessTokenFetcher(const std::vector<std::string>& scopes) {
    EXPECT_CALL(*access_token_fetcher_,
                Start(GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
                      scopes)).Times(1);
    EXPECT_CALL(*flow_, CreateAccessTokenFetcher())
        .WillOnce(Return(access_token_fetcher_.release()));
  }

  TestURLFetcher* CreateURLFetcher(
      const GURL& url, bool fetch_succeeds,
      int response_code, const std::string& body) {
    TestURLFetcher* url_fetcher = new TestURLFetcher(0, url, flow_.get());
    URLRequestStatus::Status status =
        fetch_succeeds ? URLRequestStatus::SUCCESS : URLRequestStatus::FAILED;
    url_fetcher->set_status(URLRequestStatus(status, 0));

    if (response_code != 0)
      url_fetcher->set_response_code(response_code);

    if (!body.empty())
      url_fetcher->SetResponseString(body);

    return url_fetcher;
  }

  void CreateFlow(const std::string& refresh_token,
                  const std::string& access_token,
                  const std::vector<std::string>& scopes) {
    scoped_refptr<net::TestURLRequestContextGetter> request_context_getter =
        new net::TestURLRequestContextGetter(
            message_loop_.message_loop_proxy());
    flow_.reset(new MockApiCallFlow(
        request_context_getter, refresh_token, access_token, scopes));
    access_token_fetcher_.reset(new MockAccessTokenFetcher(
        flow_.get(), request_context_getter, refresh_token));
  }

  TestURLFetcher* SetupApiCall(bool succeeds, net::HttpStatusCode status) {
    std::string body(CreateBody());
    GURL url(CreateApiUrl());
    EXPECT_CALL(*flow_, CreateApiCallBody()).WillOnce(Return(body));
    EXPECT_CALL(*flow_, CreateApiCallUrl()).WillOnce(Return(url));
    TestURLFetcher* url_fetcher =
        CreateURLFetcher(url, succeeds, status, std::string());
    EXPECT_CALL(factory_, CreateURLFetcher(_, url, _, _))
        .WillOnce(Return(url_fetcher));
    return url_fetcher;
  }

  MockUrlFetcherFactory factory_;
  scoped_ptr<MockApiCallFlow> flow_;
  scoped_ptr<MockAccessTokenFetcher> access_token_fetcher_;
  base::MessageLoop message_loop_;
};

TEST_F(OAuth2ApiCallFlowTest, FirstApiCallSucceeds) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(*flow_, ProcessApiCallSuccess(url_fetcher));
  flow_->Start();
  flow_->OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2ApiCallFlowTest, SecondApiCallSucceeds) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher1 = SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  flow_->Start();
  SetupAccessTokenFetcher(scopes);
  flow_->OnURLFetchComplete(url_fetcher1);
  TestURLFetcher* url_fetcher2 = SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(*flow_, ProcessApiCallSuccess(url_fetcher2));
  flow_->OnGetTokenSuccess(
      at,
      base::Time::Now() + base::TimeDelta::FromMinutes(3600));
  flow_->OnURLFetchComplete(url_fetcher2);
}

TEST_F(OAuth2ApiCallFlowTest, SecondApiCallFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher1 = SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  flow_->Start();
  SetupAccessTokenFetcher(scopes);
  flow_->OnURLFetchComplete(url_fetcher1);
  TestURLFetcher* url_fetcher2 = SetupApiCall(false, net::HTTP_UNAUTHORIZED);
  EXPECT_CALL(*flow_, ProcessApiCallFailure(url_fetcher2));
  flow_->OnGetTokenSuccess(
      at,
      base::Time::Now() + base::TimeDelta::FromMinutes(3600));
  flow_->OnURLFetchComplete(url_fetcher2);
}

TEST_F(OAuth2ApiCallFlowTest, NewTokenGenerationFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, at, scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(true, net::HTTP_UNAUTHORIZED);
  flow_->Start();
  SetupAccessTokenFetcher(scopes);
  flow_->OnURLFetchComplete(url_fetcher);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*flow_, ProcessMintAccessTokenFailure(error));
  flow_->OnGetTokenFailure(error);
}

TEST_F(OAuth2ApiCallFlowTest, EmptyAccessTokenFirstApiCallSucceeds) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, std::string(), scopes);
  SetupAccessTokenFetcher(scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(true, net::HTTP_OK);
  EXPECT_CALL(*flow_, ProcessApiCallSuccess(url_fetcher));
  flow_->Start();
  flow_->OnGetTokenSuccess(
      at,
      base::Time::Now() + base::TimeDelta::FromMinutes(3600));
  flow_->OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2ApiCallFlowTest, EmptyAccessTokenApiCallFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, std::string(), scopes);
  SetupAccessTokenFetcher(scopes);
  TestURLFetcher* url_fetcher = SetupApiCall(false, net::HTTP_BAD_GATEWAY);
  EXPECT_CALL(*flow_, ProcessApiCallFailure(url_fetcher));
  flow_->Start();
  flow_->OnGetTokenSuccess(
      at,
      base::Time::Now() + base::TimeDelta::FromMinutes(3600));
  flow_->OnURLFetchComplete(url_fetcher);
}

TEST_F(OAuth2ApiCallFlowTest, EmptyAccessTokenNewTokenGenerationFails) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());

  CreateFlow(rt, std::string(), scopes);
  SetupAccessTokenFetcher(scopes);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*flow_, ProcessMintAccessTokenFailure(error));
  flow_->Start();
  flow_->OnGetTokenFailure(error);
}

TEST_F(OAuth2ApiCallFlowTest, CreateURLFetcher) {
  std::string rt = "refresh_token";
  std::string at = "access_token";
  std::vector<std::string> scopes(CreateTestScopes());
  std::string body = CreateBody();
  GURL url(CreateApiUrl());

  CreateFlow(rt, at, scopes);
  scoped_ptr<TestURLFetcher> url_fetcher(SetupApiCall(true, net::HTTP_OK));
  flow_->CreateURLFetcher();
  HttpRequestHeaders headers;
  url_fetcher->GetExtraRequestHeaders(&headers);
  std::string auth_header;
  EXPECT_TRUE(headers.GetHeader("Authorization", &auth_header));
  EXPECT_EQ("Bearer access_token", auth_header);
  EXPECT_EQ(url, url_fetcher->GetOriginalURL());
  EXPECT_EQ(body, url_fetcher->upload_data());
}
