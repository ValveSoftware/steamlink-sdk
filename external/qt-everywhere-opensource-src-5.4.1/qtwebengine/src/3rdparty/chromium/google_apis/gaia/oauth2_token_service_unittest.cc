// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// A testing consumer that retries on error.
class RetryingTestingOAuth2TokenServiceConsumer
    : public TestingOAuth2TokenServiceConsumer {
 public:
  RetryingTestingOAuth2TokenServiceConsumer(
      OAuth2TokenService* oauth2_service,
      const std::string& account_id)
      : oauth2_service_(oauth2_service),
        account_id_(account_id) {}
  virtual ~RetryingTestingOAuth2TokenServiceConsumer() {}

  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE {
    TestingOAuth2TokenServiceConsumer::OnGetTokenFailure(request, error);
    request_.reset(oauth2_service_->StartRequest(
        account_id_, OAuth2TokenService::ScopeSet(), this).release());
  }

  OAuth2TokenService* oauth2_service_;
  std::string account_id_;
  scoped_ptr<OAuth2TokenService::Request> request_;
};

class TestOAuth2TokenService : public OAuth2TokenService {
 public:
  explicit TestOAuth2TokenService(net::TestURLRequestContextGetter* getter)
      : request_context_getter_(getter) {
  }

  void CancelAllRequestsForTest() { CancelAllRequests(); }

  void CancelRequestsForAccountForTest(const std::string& account_id) {
    CancelRequestsForAccount(account_id);
  }

  // For testing: set the refresh token to be used.
  void set_refresh_token(const std::string& account_id,
                         const std::string& refresh_token) {
    if (refresh_token.empty())
      refresh_tokens_.erase(account_id);
    else
      refresh_tokens_[account_id] = refresh_token;
  }

  virtual bool RefreshTokenIsAvailable(const std::string& account_id) const
      OVERRIDE {
    std::map<std::string, std::string>::const_iterator it =
        refresh_tokens_.find(account_id);

    return it != refresh_tokens_.end();
  };

 private:
  // OAuth2TokenService implementation.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE {
    return request_context_getter_.get();
  }

  virtual OAuth2AccessTokenFetcher* CreateAccessTokenFetcher(
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      OAuth2AccessTokenConsumer* consumer) OVERRIDE {
    std::map<std::string, std::string>::const_iterator it =
        refresh_tokens_.find(account_id);
    DCHECK(it != refresh_tokens_.end());
    std::string refresh_token(it->second);
    return new OAuth2AccessTokenFetcherImpl(consumer, getter, refresh_token);
  };

  std::map<std::string, std::string> refresh_tokens_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
};

class OAuth2TokenServiceTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    oauth2_service_.reset(
        new TestOAuth2TokenService(new net::TestURLRequestContextGetter(
            message_loop_.message_loop_proxy())));
    account_id_ = "test_user@gmail.com";
  }

  virtual void TearDown() OVERRIDE {
    // Makes sure that all the clean up tasks are run.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::MessageLoopForIO message_loop_;  // net:: stuff needs IO message loop.
  net::TestURLFetcherFactory factory_;
  scoped_ptr<TestOAuth2TokenService> oauth2_service_;
  std::string account_id_;
  TestingOAuth2TokenServiceConsumer consumer_;
};

TEST_F(OAuth2TokenServiceTest, NoOAuth2RefreshToken) {
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_, OAuth2TokenService::ScopeSet(),
          &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, FailureShouldNotRetry) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
  EXPECT_EQ(fetcher, factory_.GetFetcherByID(0));
}

TEST_F(OAuth2TokenServiceTest, SuccessWithoutCaching) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, SuccessWithCaching) {
  OAuth2TokenService::ScopeSet scopes1;
  scopes1.insert("s1");
  scopes1.insert("s2");
  OAuth2TokenService::ScopeSet scopes1_same;
  scopes1_same.insert("s2");
  scopes1_same.insert("s1");
  OAuth2TokenService::ScopeSet scopes2;
  scopes2.insert("s3");

  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_, scopes1, &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request to the same set of scopes, should return the same token
  // without needing a network request.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes1_same, &consumer_));
  base::RunLoop().RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Third request to a new set of scopes, should return another token.
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(account_id_, scopes2, &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token2", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndExpirationAndFailure) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  // Network failure.
  fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, SuccessAndExpirationAndSuccess) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request must try to access the network as the token has expired.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("another token", 0));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("another token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, RequestDeletedBeforeCompletion) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_,
                                    OAuth2TokenService::ScopeSet(),
                                    &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  request.reset();

  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, RequestDeletedAfterCompletion) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      account_id_, OAuth2TokenService::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  request.reset();

  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, MultipleRequestsForTheSameScopesWithOneDeleted) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      account_id_, OAuth2TokenService::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_, OAuth2TokenService::ScopeSet(),
          &consumer_));
  base::RunLoop().RunUntilIdle();

  request.reset();

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, ClearedRefreshTokenFailsSubsequentRequests) {
  // We have a valid refresh token; the first request is successful.
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      account_id_, OAuth2TokenService::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // The refresh token is no longer available; subsequent requests fail.
  oauth2_service_->set_refresh_token(account_id_, "");
  request = oauth2_service_->StartRequest(account_id_,
      OAuth2TokenService::ScopeSet(), &consumer_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest,
       ChangedRefreshTokenDoesNotAffectInFlightRequests) {
  oauth2_service_->set_refresh_token(account_id_, "first refreshToken");
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert("s1");
  scopes.insert("s2");
  OAuth2TokenService::ScopeSet scopes1;
  scopes.insert("s3");
  scopes.insert("s4");

  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();
  net::TestURLFetcher* fetcher1 = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher1);

  // Note |request| is still pending when the refresh token changes.
  oauth2_service_->set_refresh_token(account_id_, "second refreshToken");

  // A 2nd request (using the new refresh token) that occurs and completes
  // while the 1st request is in flight is successful.
  TestingOAuth2TokenServiceConsumer consumer2;
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes1, &consumer2));
  base::RunLoop().RunUntilIdle();

  net::TestURLFetcher* fetcher2 = factory_.GetFetcherByID(0);
  fetcher2->set_response_code(net::HTTP_OK);
  fetcher2->SetResponseString(GetValidTokenResponse("second token", 3600));
  fetcher2->delegate()->OnURLFetchComplete(fetcher2);
  EXPECT_EQ(1, consumer2.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer2.number_of_errors_);
  EXPECT_EQ("second token", consumer2.last_token_);

  fetcher1->set_response_code(net::HTTP_OK);
  fetcher1->SetResponseString(GetValidTokenResponse("first token", 3600));
  fetcher1->delegate()->OnURLFetchComplete(fetcher1);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("first token", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, ServiceShutDownBeforeFetchComplete) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      account_id_, OAuth2TokenService::ScopeSet(), &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  // The destructor should cancel all in-flight fetchers.
  oauth2_service_.reset(NULL);

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, RetryingConsumer) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  RetryingTestingOAuth2TokenServiceConsumer consumer(oauth2_service_.get(),
      account_id_);
  scoped_ptr<OAuth2TokenService::Request> request(oauth2_service_->StartRequest(
      account_id_, OAuth2TokenService::ScopeSet(), &consumer));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer.number_of_errors_);

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(1, consumer.number_of_errors_);

  fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_UNAUTHORIZED);
  fetcher->SetResponseString(std::string());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(0, consumer.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, InvalidateToken) {
  OAuth2TokenService::ScopeSet scopes;
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");

  // First request.
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(1, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Second request, should return the same token without needing a network
  // request.
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();

  // No new network fetcher.
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token", consumer_.last_token_);

  // Invalidating the token should return a new token on the next request.
  oauth2_service_->InvalidateToken(account_id_, scopes, consumer_.last_token_);
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(account_id_, scopes, &consumer_));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  fetcher = factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(GetValidTokenResponse("token2", 3600));
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_EQ(3, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);
  EXPECT_EQ("token2", consumer_.last_token_);
}

TEST_F(OAuth2TokenServiceTest, CancelAllRequests) {
  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request(
      oauth2_service_->StartRequest(account_id_, OAuth2TokenService::ScopeSet(),
          &consumer_));

  oauth2_service_->set_refresh_token("account_id_2", "refreshToken2");
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_, OAuth2TokenService::ScopeSet(),
          &consumer_));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  oauth2_service_->CancelAllRequestsForTest();

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, CancelRequestsForAccount) {
  OAuth2TokenService::ScopeSet scope_set_1;
  scope_set_1.insert("scope1");
  scope_set_1.insert("scope2");
  OAuth2TokenService::ScopeSet scope_set_2(scope_set_1.begin(),
                                           scope_set_1.end());
  scope_set_2.insert("scope3");

  oauth2_service_->set_refresh_token(account_id_, "refreshToken");
  scoped_ptr<OAuth2TokenService::Request> request1(
      oauth2_service_->StartRequest(account_id_, scope_set_1, &consumer_));
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequest(account_id_, scope_set_2, &consumer_));

  std::string account_id_2("account_id_2");
  oauth2_service_->set_refresh_token(account_id_2, "refreshToken2");
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequest(account_id_2, scope_set_1, &consumer_));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(0, consumer_.number_of_errors_);

  oauth2_service_->CancelRequestsForAccountForTest(account_id_);

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(2, consumer_.number_of_errors_);

  oauth2_service_->CancelRequestsForAccountForTest(account_id_2);

  EXPECT_EQ(0, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(3, consumer_.number_of_errors_);
}

TEST_F(OAuth2TokenServiceTest, SameScopesRequestedForDifferentClients) {
  std::string client_id_1("client1");
  std::string client_secret_1("secret1");
  std::string client_id_2("client2");
  std::string client_secret_2("secret2");
  std::set<std::string> scope_set;
  scope_set.insert("scope1");
  scope_set.insert("scope2");

  std::string refresh_token("refreshToken");
  oauth2_service_->set_refresh_token(account_id_, refresh_token);

  scoped_ptr<OAuth2TokenService::Request> request1(
      oauth2_service_->StartRequestForClient(account_id_,
                                             client_id_1,
                                             client_secret_1,
                                             scope_set,
                                             &consumer_));
  scoped_ptr<OAuth2TokenService::Request> request2(
      oauth2_service_->StartRequestForClient(account_id_,
                                             client_id_2,
                                             client_secret_2,
                                             scope_set,
                                             &consumer_));
  // Start a request that should be duplicate of |request1|.
  scoped_ptr<OAuth2TokenService::Request> request3(
      oauth2_service_->StartRequestForClient(account_id_,
                                             client_id_1,
                                             client_secret_1,
                                             scope_set,
                                             &consumer_));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2U,
            oauth2_service_->GetNumPendingRequestsForTesting(
                client_id_1,
                account_id_,
                scope_set));
   ASSERT_EQ(1U,
             oauth2_service_->GetNumPendingRequestsForTesting(
                client_id_2,
                account_id_,
                scope_set));
}

TEST_F(OAuth2TokenServiceTest, RequestParametersOrderTest) {
  OAuth2TokenService::ScopeSet set_0;
  OAuth2TokenService::ScopeSet set_1;
  set_1.insert("1");

  OAuth2TokenService::RequestParameters params[] = {
      OAuth2TokenService::RequestParameters("0", "0", set_0),
      OAuth2TokenService::RequestParameters("0", "0", set_1),
      OAuth2TokenService::RequestParameters("0", "1", set_0),
      OAuth2TokenService::RequestParameters("0", "1", set_1),
      OAuth2TokenService::RequestParameters("1", "0", set_0),
      OAuth2TokenService::RequestParameters("1", "0", set_1),
      OAuth2TokenService::RequestParameters("1", "1", set_0),
      OAuth2TokenService::RequestParameters("1", "1", set_1),
  };

  for (size_t i = 0; i < arraysize(params); i++) {
    for (size_t j = 0; j < arraysize(params); j++) {
      if (i == j) {
        EXPECT_FALSE(params[i] < params[j]) << " i=" << i << ", j=" << j;
        EXPECT_FALSE(params[j] < params[i]) << " i=" << i << ", j=" << j;
      } else if (i < j) {
        EXPECT_TRUE(params[i] < params[j]) << " i=" << i << ", j=" << j;
        EXPECT_FALSE(params[j] < params[i]) << " i=" << i << ", j=" << j;
      } else {
        EXPECT_TRUE(params[j] < params[i]) << " i=" << i << ", j=" << j;
        EXPECT_FALSE(params[i] < params[j]) << " i=" << i << ", j=" << j;
      }
    }
  }
}
