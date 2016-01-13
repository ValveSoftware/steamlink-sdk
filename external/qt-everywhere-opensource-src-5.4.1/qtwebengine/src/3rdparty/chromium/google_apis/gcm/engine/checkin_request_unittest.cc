// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  // Explicitly set to 1 to skip the delay of the first Retry, as we are not
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

}

const uint64 kAndroidId = 42UL;
const uint64 kBlankAndroidId = 999999UL;
const uint64 kBlankSecurityToken = 999999UL;
const char kCheckinURL[] = "http://foo.bar/checkin";
const char kChromeVersion[] = "Version String";
const uint64 kSecurityToken = 77;
const char kSettingsDigest[] = "settings_digest";

class CheckinRequestTest : public testing::Test {
 public:
  enum ResponseScenario {
    VALID_RESPONSE,  // Both android_id and security_token set in response.
    MISSING_ANDROID_ID,  // android_id is missing.
    MISSING_SECURITY_TOKEN,  // security_token is missing.
    ANDROID_ID_IS_ZER0,  // android_id is 0.
    SECURITY_TOKEN_IS_ZERO  // security_token is 0.
  };

  CheckinRequestTest();
  virtual ~CheckinRequestTest();

  void FetcherCallback(
      const checkin_proto::AndroidCheckinResponse& response);

  void CreateRequest(uint64 android_id, uint64 security_token);

  void SetResponseStatusAndString(
      net::HttpStatusCode status_code,
      const std::string& response_data);

  void CompleteFetch();

  void SetResponse(ResponseScenario response_scenario);

 protected:
  bool callback_called_;
  uint64 android_id_;
  uint64 security_token_;
  int checkin_device_type_;
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  checkin_proto::ChromeBuildProto chrome_build_proto_;
  scoped_ptr<CheckinRequest> request_;
  FakeGCMStatsRecorder recorder_;
};

CheckinRequestTest::CheckinRequestTest()
    : callback_called_(false),
      android_id_(kBlankAndroidId),
      security_token_(kBlankSecurityToken),
      checkin_device_type_(0),
      url_request_context_getter_(new net::TestURLRequestContextGetter(
          message_loop_.message_loop_proxy())) {
}

CheckinRequestTest::~CheckinRequestTest() {}

void CheckinRequestTest::FetcherCallback(
    const checkin_proto::AndroidCheckinResponse& checkin_response) {
  callback_called_ = true;
  if (checkin_response.has_android_id())
    android_id_ = checkin_response.android_id();
  if (checkin_response.has_security_token())
    security_token_ = checkin_response.security_token();
}

void CheckinRequestTest::CreateRequest(uint64 android_id,
                                       uint64 security_token) {
  // First setup a chrome_build protobuf.
  chrome_build_proto_.set_platform(
      checkin_proto::ChromeBuildProto::PLATFORM_LINUX);
  chrome_build_proto_.set_channel(
      checkin_proto::ChromeBuildProto::CHANNEL_CANARY);
  chrome_build_proto_.set_chrome_version(kChromeVersion);

  CheckinRequest::RequestInfo request_info(
      android_id,
      security_token,
      kSettingsDigest,
      chrome_build_proto_);
  // Then create a request with that protobuf and specified android_id,
  // security_token.
  request_.reset(new CheckinRequest(
      GURL(kCheckinURL),
      request_info,
      kDefaultBackoffPolicy,
      base::Bind(&CheckinRequestTest::FetcherCallback, base::Unretained(this)),
      url_request_context_getter_.get(),
      &recorder_));

  // Setting android_id_ and security_token_ to blank value, not used elsewhere
  // in the tests.
  callback_called_ = false;
  android_id_ = kBlankAndroidId;
  security_token_ = kBlankSecurityToken;
}

void CheckinRequestTest::SetResponseStatusAndString(
    net::HttpStatusCode status_code,
    const std::string& response_data) {
  net::TestURLFetcher* fetcher =
      url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(status_code);
  fetcher->SetResponseString(response_data);
}

void CheckinRequestTest::CompleteFetch() {
  net::TestURLFetcher* fetcher =
      url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void CheckinRequestTest::SetResponse(ResponseScenario response_scenario) {
  checkin_proto::AndroidCheckinResponse response;
  response.set_stats_ok(true);

  uint64 android_id = response_scenario == ANDROID_ID_IS_ZER0 ? 0 : kAndroidId;
  uint64 security_token =
      response_scenario == SECURITY_TOKEN_IS_ZERO ? 0 : kSecurityToken;

  if (response_scenario != MISSING_ANDROID_ID)
    response.set_android_id(android_id);

  if (response_scenario != MISSING_SECURITY_TOKEN)
    response.set_security_token(security_token);

  std::string response_string;
  response.SerializeToString(&response_string);
  SetResponseStatusAndString(net::HTTP_OK, response_string);
}

TEST_F(CheckinRequestTest, FetcherDataAndURL) {
  CreateRequest(kAndroidId, kSecurityToken);
  request_->Start();

  // Get data sent by request.
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kCheckinURL), fetcher->GetOriginalURL());

  checkin_proto::AndroidCheckinRequest request_proto;
  request_proto.ParseFromString(fetcher->upload_data());
  EXPECT_EQ(kAndroidId, static_cast<uint64>(request_proto.id()));
  EXPECT_EQ(kSecurityToken, request_proto.security_token());
  EXPECT_EQ(chrome_build_proto_.platform(),
            request_proto.checkin().chrome_build().platform());
  EXPECT_EQ(chrome_build_proto_.chrome_version(),
            request_proto.checkin().chrome_build().chrome_version());
  EXPECT_EQ(chrome_build_proto_.channel(),
            request_proto.checkin().chrome_build().channel());

#if defined(CHROME_OS)
  EXPECT_EQ(checkin_proto::DEVICE_CHROME_OS, request_proto.checkin().type());
#else
  EXPECT_EQ(checkin_proto::DEVICE_CHROME_BROWSER,
            request_proto.checkin().type());
#endif

  EXPECT_EQ(kSettingsDigest, request_proto.digest());
}

TEST_F(CheckinRequestTest, ResponseBodyEmpty) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, std::string());
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseBodyCorrupted) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "Corrupted response body");
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseHttpStatusUnauthorized) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED, std::string());
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kBlankAndroidId, android_id_);
  EXPECT_EQ(kBlankSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseHttpStatusBadRequest) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseStatusAndString(net::HTTP_BAD_REQUEST, std::string());
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kBlankAndroidId, android_id_);
  EXPECT_EQ(kBlankSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponseStatusAndString(net::HTTP_INTERNAL_SERVER_ERROR, std::string());
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseMissingAndroidId) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponse(MISSING_ANDROID_ID);
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, ResponseMissingSecurityToken) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponse(MISSING_SECURITY_TOKEN);
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, AndroidIdEqualsZeroInResponse) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponse(ANDROID_ID_IS_ZER0);
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, SecurityTokenEqualsZeroInResponse) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponse(SECURITY_TOKEN_IS_ZERO);
  CompleteFetch();

  EXPECT_FALSE(callback_called_);

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, SuccessfulFirstTimeCheckin) {
  CreateRequest(0u, 0u);
  request_->Start();

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

TEST_F(CheckinRequestTest, SuccessfulSubsequentCheckin) {
  CreateRequest(kAndroidId, kSecurityToken);
  request_->Start();

  SetResponse(VALID_RESPONSE);
  CompleteFetch();

  EXPECT_TRUE(callback_called_);
  EXPECT_EQ(kAndroidId, android_id_);
  EXPECT_EQ(kSecurityToken, security_token_);
}

}  // namespace gcm
