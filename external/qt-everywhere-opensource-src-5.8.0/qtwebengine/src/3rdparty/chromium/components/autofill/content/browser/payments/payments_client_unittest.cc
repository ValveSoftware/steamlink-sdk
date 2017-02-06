// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace payments {

class PaymentsClientTest : public testing::Test, public PaymentsClientDelegate {
 public:
  PaymentsClientTest() : result_(AutofillClient::NONE) {}
  ~PaymentsClientTest() override {}

  void SetUp() override {
    // Silence the warning for mismatching sync and Payments servers.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWalletServiceUseSandbox, "0");

    result_ = AutofillClient::NONE;
    real_pan_.clear();
    legal_message_.reset();

    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    token_service_.reset(new FakeOAuth2TokenService());
    identity_provider_.reset(new FakeIdentityProvider(token_service_.get()));
    client_.reset(new PaymentsClient(request_context_.get(), this));
  }

  void TearDown() override { client_.reset(); }

  // PaymentsClientDelegate

  IdentityProvider* GetIdentityProvider() override {
    return identity_provider_.get();
  }

  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan) override {
    result_ = result;
    real_pan_ = real_pan;
  }

  void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::DictionaryValue> legal_message) override {
    result_ = result;
    legal_message_ = std::move(legal_message);
  }

  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result) override {
    result_ = result;
  }

 protected:
  void StartUnmasking() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    PaymentsClient::UnmaskRequestDetails request_details;
    request_details.card = test::GetMaskedServerCard();
    request_details.user_response.cvc = base::ASCIIToUTF16("123");
    request_details.risk_data = "some risk data";
    client_->UnmaskCard(request_details);
  }

  void StartGettingUploadDetails() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    client_->GetUploadDetails("language-LOCALE");
  }

  void StartUploading() {
    token_service_->AddAccount("example@gmail.com");
    identity_provider_->LogIn("example@gmail.com");
    PaymentsClient::UploadRequestDetails request_details;
    request_details.card = test::GetCreditCard();
    request_details.cvc = base::ASCIIToUTF16("123");
    request_details.context_token = base::ASCIIToUTF16("context token");
    request_details.risk_data = "some risk data";
    request_details.app_locale = "language-LOCALE";
    client_->UploadCard(request_details);
  }

  void IssueOAuthToken() {
    token_service_->IssueAllTokensForAccount(
        "example@gmail.com", "totally_real_token",
        base::Time::Now() + base::TimeDelta::FromDays(10));

    // Verify the auth header.
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    net::HttpRequestHeaders request_headers;
    fetcher->GetExtraRequestHeaders(&request_headers);
    std::string auth_header_value;
    EXPECT_TRUE(request_headers.GetHeader(
        net::HttpRequestHeaders::kAuthorization, &auth_header_value))
        << request_headers.ToString();
    EXPECT_EQ("Bearer totally_real_token", auth_header_value);
  }

  void ReturnResponse(net::HttpStatusCode response_code,
                      const std::string& response_body) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  AutofillClient::PaymentsRpcResult result_;
  std::string real_pan_;
  std::unique_ptr<base::DictionaryValue> legal_message_;

  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLFetcherFactory factory_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  std::unique_ptr<FakeOAuth2TokenService> token_service_;
  std::unique_ptr<FakeIdentityProvider> identity_provider_;
  std::unique_ptr<PaymentsClient> client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsClientTest);
};

TEST_F(PaymentsClientTest, OAuthError) {
  StartUnmasking();
  token_service_->IssueErrorForAllPendingRequestsForAccount(
      "example@gmail.com",
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_TRUE(real_pan_.empty());
}

TEST_F(PaymentsClientTest, UnmaskSuccess) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_EQ("1234", real_pan_);
}

TEST_F(PaymentsClientTest, GetDetailsSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
  EXPECT_NE(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, UploadSuccess) {
  StartUploading();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
}

TEST_F(PaymentsClientTest, GetDetailsFollowedByUploadSuccess) {
  StartGettingUploadDetails();
  ReturnResponse(
      net::HTTP_OK,
      "{ \"context_token\": \"some_token\", \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);

  result_ = AutofillClient::NONE;

  StartUploading();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::SUCCESS, result_);
}

TEST_F(PaymentsClientTest, UnmaskMissingPan) {
  StartUnmasking();
  ReturnResponse(net::HTTP_OK, "{}");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingContextToken) {
  StartGettingUploadDetails();
  ReturnResponse(net::HTTP_OK, "{ \"legal_message\": {} }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
}

TEST_F(PaymentsClientTest, GetDetailsMissingLegalMessage) {
  StartGettingUploadDetails();
  ReturnResponse(net::HTTP_OK, "{ \"context_token\": \"some_token\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ(nullptr, legal_message_.get());
}

TEST_F(PaymentsClientTest, RetryFailure) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error\": { \"code\": \"INTERNAL\" } }");
  EXPECT_EQ(AutofillClient::TRY_AGAIN_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, PermanentFailure) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK,
                 "{ \"error\": { \"code\": \"ANYTHING_ELSE\" } }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, MalformedResponse) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_OK, "{ \"error_code\": \"WRONG_JSON_FORMAT\" }");
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, ReauthNeeded) {
  {
    StartUnmasking();
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::NONE, result_);
    EXPECT_EQ("", real_pan_);

    // Second HTTP_UNAUTHORIZED causes permanent failure.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
    EXPECT_EQ("", real_pan_);
  }

  result_ = AutofillClient::NONE;
  real_pan_.clear();

  {
    StartUnmasking();
    IssueOAuthToken();
    ReturnResponse(net::HTTP_UNAUTHORIZED, "");
    // No response yet.
    EXPECT_EQ(AutofillClient::NONE, result_);
    EXPECT_EQ("", real_pan_);

    // HTTP_OK after first HTTP_UNAUTHORIZED results in success.
    IssueOAuthToken();
    ReturnResponse(net::HTTP_OK, "{ \"pan\": \"1234\" }");
    EXPECT_EQ(AutofillClient::SUCCESS, result_);
    EXPECT_EQ("1234", real_pan_);
  }
}

TEST_F(PaymentsClientTest, NetworkError) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_REQUEST_TIMEOUT, std::string());
  EXPECT_EQ(AutofillClient::NETWORK_ERROR, result_);
  EXPECT_EQ("", real_pan_);
}

TEST_F(PaymentsClientTest, OtherError) {
  StartUnmasking();
  IssueOAuthToken();
  ReturnResponse(net::HTTP_FORBIDDEN, std::string());
  EXPECT_EQ(AutofillClient::PERMANENT_FAILURE, result_);
  EXPECT_EQ("", real_pan_);
}

}  // namespace autofill
}  // namespace payments
