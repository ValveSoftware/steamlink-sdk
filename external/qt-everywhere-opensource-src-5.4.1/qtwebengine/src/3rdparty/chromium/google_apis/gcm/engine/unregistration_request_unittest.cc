// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "google_apis/gcm/engine/unregistration_request.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {
const uint64 kAndroidId = 42UL;
const char kLoginHeader[] = "AidLogin";
const char kAppId[] = "TestAppId";
const char kDeletedAppId[] = "deleted=TestAppId";
const char kRegistrationURL[] = "http://foo.bar/register";
const uint64 kSecurityToken = 77UL;

// Backoff policy for testing registration request.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  // Explicitly set to 2 to skip the delay on the first retry, as we are not
  // trying to test the backoff itself, but rather the fact that retry happens.
  1,

  // Initial delay for exponential back-off in ms.
  15000,  // 15 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.5,  // 50%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 60 * 5, // 5 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};
}  // namespace

class UnregistrationRequestTest : public testing::Test {
 public:
  UnregistrationRequestTest();
  virtual ~UnregistrationRequestTest();

  void UnregistrationCallback(UnregistrationRequest::Status status);

  void CreateRequest();
  void SetResponseStatusAndString(net::HttpStatusCode status_code,
                                  const std::string& response_body);
  void CompleteFetch();

 protected:
  bool callback_called_;
  UnregistrationRequest::Status status_;
  scoped_ptr<UnregistrationRequest> request_;
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  FakeGCMStatsRecorder recorder_;
};

UnregistrationRequestTest::UnregistrationRequestTest()
    : callback_called_(false),
      status_(UnregistrationRequest::UNREGISTRATION_STATUS_COUNT),
      url_request_context_getter_(new net::TestURLRequestContextGetter(
          message_loop_.message_loop_proxy())) {}

UnregistrationRequestTest::~UnregistrationRequestTest() {}

void UnregistrationRequestTest::UnregistrationCallback(
    UnregistrationRequest::Status status) {
  callback_called_ = true;
  status_ = status;
}

void UnregistrationRequestTest::CreateRequest() {
  request_.reset(new UnregistrationRequest(
      GURL(kRegistrationURL),
      UnregistrationRequest::RequestInfo(kAndroidId,
                                         kSecurityToken,
                                         kAppId),
      kDefaultBackoffPolicy,
      base::Bind(&UnregistrationRequestTest::UnregistrationCallback,
                 base::Unretained(this)),
      url_request_context_getter_.get(),
      &recorder_));
}

void UnregistrationRequestTest::SetResponseStatusAndString(
    net::HttpStatusCode status_code,
    const std::string& response_body) {
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(status_code);
  fetcher->SetResponseString(response_body);
}

void UnregistrationRequestTest::CompleteFetch() {
  status_ = UnregistrationRequest::UNREGISTRATION_STATUS_COUNT;
  callback_called_ = false;
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(UnregistrationRequestTest, RequestDataPassedToFetcher) {
  CreateRequest();
  request_->Start();

  // Get data sent by request.
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  EXPECT_EQ(GURL(kRegistrationURL), fetcher->GetOriginalURL());

  // Verify that authorization header was put together properly.
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string auth_header;
  headers.GetHeader(net::HttpRequestHeaders::kAuthorization, &auth_header);
  base::StringTokenizer auth_tokenizer(auth_header, " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::Uint64ToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::Uint64ToString(kSecurityToken), auth_tokenizer.token());
  std::string app_id_header;
  headers.GetHeader("app", &app_id_header);
  EXPECT_EQ(kAppId, app_id_header);

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["app"] = kAppId;
  expected_pairs["device"] = base::Uint64ToString(kAndroidId);
  expected_pairs["delete"] = "true";
  expected_pairs["gcm_unreg_caller"] = "false";

  // Verify data was formatted properly.
  std::string upload_data = fetcher->upload_data();
  base::StringTokenizer data_tokenizer(upload_data, "&=");
  while (data_tokenizer.GetNext()) {
    std::map<std::string, std::string>::iterator iter =
        expected_pairs.find(data_tokenizer.token());
    ASSERT_TRUE(iter != expected_pairs.end()) << data_tokenizer.token();
    ASSERT_TRUE(data_tokenizer.GetNext()) << data_tokenizer.token();
    EXPECT_EQ(iter->second, data_tokenizer.token());
    // Ensure that none of the keys appears twice.
    expected_pairs.erase(iter);
  }

  EXPECT_EQ(0UL, expected_pairs.size());
}

TEST_F(UnregistrationRequestTest, SuccessfulUnregistration) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, kDeletedAppId);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(UnregistrationRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED, "");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::HTTP_NOT_OK, status_);
}

TEST_F(UnregistrationRequestTest, ResponseEmpty) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, kDeletedAppId);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(UnregistrationRequestTest, InvalidParametersError) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "Error=INVALID_PARAMETERS");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::INVALID_PARAMETERS, status_);
}

TEST_F(UnregistrationRequestTest, UnkwnownError) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "Error=XXX");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::UNKNOWN_ERROR, status_);
}

TEST_F(UnregistrationRequestTest, ServiceUnavailable) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_SERVICE_UNAVAILABLE, "");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, kDeletedAppId);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(UnregistrationRequestTest, InternalServerError) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_INTERNAL_SERVER_ERROR, "");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, kDeletedAppId);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(UnregistrationRequestTest, IncorrectAppId) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "deleted=OtherTestAppId");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, kDeletedAppId);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

TEST_F(UnregistrationRequestTest, ResponseParsingFailed) {
  CreateRequest();
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "some malformed response");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, kDeletedAppId);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(UnregistrationRequest::SUCCESS, status_);
}

}  // namespace gcm
