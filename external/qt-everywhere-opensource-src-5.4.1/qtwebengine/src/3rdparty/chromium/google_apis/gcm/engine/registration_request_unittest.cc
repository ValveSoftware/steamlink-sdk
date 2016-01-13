// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {
const uint64 kAndroidId = 42UL;
const char kAppId[] = "TestAppId";
const char kDeveloperId[] = "Project1";
const char kLoginHeader[] = "AidLogin";
const char kRegistrationURL[] = "http://foo.bar/register";
const uint64 kSecurityToken = 77UL;

// Backoff policy for testing registration request.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  // Explicitly set to 2 to skip the delay on the first retry, as we are not
  // trying to test the backoff itself, but rather the fact that retry happens.
  2,

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

class RegistrationRequestTest : public testing::Test {
 public:
  RegistrationRequestTest();
  virtual ~RegistrationRequestTest();

  void RegistrationCallback(RegistrationRequest::Status status,
                            const std::string& registration_id);

  void CreateRequest(const std::string& sender_ids);
  void SetResponseStatusAndString(net::HttpStatusCode status_code,
                                  const std::string& response_body);
  void CompleteFetch();
  void set_max_retry_count(int max_retry_count) {
    max_retry_count_ = max_retry_count;
  }

 protected:
  int max_retry_count_;
  RegistrationRequest::Status status_;
  std::string registration_id_;
  bool callback_called_;
  std::map<std::string, std::string> extras_;
  scoped_ptr<RegistrationRequest> request_;
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  FakeGCMStatsRecorder recorder_;
};

RegistrationRequestTest::RegistrationRequestTest()
    : max_retry_count_(2),
      status_(RegistrationRequest::SUCCESS),
      callback_called_(false),
      url_request_context_getter_(new net::TestURLRequestContextGetter(
          message_loop_.message_loop_proxy())) {}

RegistrationRequestTest::~RegistrationRequestTest() {}

void RegistrationRequestTest::RegistrationCallback(
    RegistrationRequest::Status status,
    const std::string& registration_id) {
  status_ = status;
  registration_id_ = registration_id;
  callback_called_ = true;
}

void RegistrationRequestTest::CreateRequest(const std::string& sender_ids) {
  std::vector<std::string> senders;
  base::StringTokenizer tokenizer(sender_ids, ",");
  while (tokenizer.GetNext())
    senders.push_back(tokenizer.token());

  request_.reset(new RegistrationRequest(
      GURL(kRegistrationURL),
      RegistrationRequest::RequestInfo(kAndroidId,
                                       kSecurityToken,
                                       kAppId,
                                       senders),
      kDefaultBackoffPolicy,
      base::Bind(&RegistrationRequestTest::RegistrationCallback,
                 base::Unretained(this)),
      max_retry_count_,
      url_request_context_getter_.get(),
      &recorder_));
}

void RegistrationRequestTest::SetResponseStatusAndString(
    net::HttpStatusCode status_code,
    const std::string& response_body) {
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(status_code);
  fetcher->SetResponseString(response_body);
}

void RegistrationRequestTest::CompleteFetch() {
  registration_id_.clear();
  status_ = RegistrationRequest::SUCCESS;
  callback_called_ = false;

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(RegistrationRequestTest, RequestSuccessful) {
  set_max_retry_count(0);
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, RequestDataAndURL) {
  CreateRequest(kDeveloperId);
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

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["device"] = base::Uint64ToString(kAndroidId);

  // Verify data was formatted properly.
  std::string upload_data = fetcher->upload_data();
  base::StringTokenizer data_tokenizer(upload_data, "&=");
  while (data_tokenizer.GetNext()) {
    std::map<std::string, std::string>::iterator iter =
        expected_pairs.find(data_tokenizer.token());
    ASSERT_TRUE(iter != expected_pairs.end());
    ASSERT_TRUE(data_tokenizer.GetNext());
    EXPECT_EQ(iter->second, data_tokenizer.token());
    // Ensure that none of the keys appears twice.
    expected_pairs.erase(iter);
  }

  EXPECT_EQ(0UL, expected_pairs.size());
}

TEST_F(RegistrationRequestTest, RequestRegistrationWithMultipleSenderIds) {
  CreateRequest("sender1,sender2@gmail.com");
  request_->Start();

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Verify data was formatted properly.
  std::string upload_data = fetcher->upload_data();
  base::StringTokenizer data_tokenizer(upload_data, "&=");

  // Skip all tokens until you hit entry for senders.
  while (data_tokenizer.GetNext() && data_tokenizer.token() != "sender")
    continue;

  ASSERT_TRUE(data_tokenizer.GetNext());
  std::string senders(net::UnescapeURLComponent(data_tokenizer.token(),
      net::UnescapeRule::URL_SPECIAL_CHARS));
  base::StringTokenizer sender_tokenizer(senders, ",");
  ASSERT_TRUE(sender_tokenizer.GetNext());
  EXPECT_EQ("sender1", sender_tokenizer.token());
  ASSERT_TRUE(sender_tokenizer.GetNext());
  EXPECT_EQ("sender2@gmail.com", sender_tokenizer.token());
}

TEST_F(RegistrationRequestTest, ResponseParsing) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED, "token=2501");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseMissingRegistrationId) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_OK, "some error in response");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseDeviceRegistrationError) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "Error=PHONE_REGISTRATION_ERROR");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseAuthenticationError) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED,
                             "Error=AUTHENTICATION_FAILED");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeds.
  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseInvalidParameters) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "Error=INVALID_PARAMETERS");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::INVALID_PARAMETERS, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseInvalidSender) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "Error=INVALID_SENDER");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::INVALID_SENDER, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseInvalidSenderBadRequest) {
  CreateRequest("sender1");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_BAD_REQUEST, "Error=INVALID_SENDER");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::INVALID_SENDER, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(RegistrationRequestTest, RequestNotSuccessful) {
  CreateRequest("sender1,sender2");
  request_->Start();

  net::URLRequestStatus request_status(net::URLRequestStatus::FAILED, 1);
  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_status(request_status);

  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeded.
  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseHttpNotOk) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_GATEWAY_TIMEOUT, "token=2501");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  // Ensuring a retry happened and succeeded.
  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::SUCCESS, status_);
  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, MaximumAttemptsReachedWithZeroRetries) {
  set_max_retry_count(0);
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_GATEWAY_TIMEOUT, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::REACHED_MAX_RETRIES, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

TEST_F(RegistrationRequestTest, MaximumAttemptsReached) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_GATEWAY_TIMEOUT, "token=2501");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_GATEWAY_TIMEOUT, "token=2501");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponseStatusAndString(net::HTTP_GATEWAY_TIMEOUT, "token=2501");
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(RegistrationRequest::REACHED_MAX_RETRIES, status_);
  EXPECT_EQ(std::string(), registration_id_);
}

}  // namespace gcm
