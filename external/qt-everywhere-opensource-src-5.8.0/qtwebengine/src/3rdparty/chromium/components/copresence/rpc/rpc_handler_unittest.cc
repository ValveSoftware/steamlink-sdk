// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "components/audio_modem/public/modem.h"
#include "components/audio_modem/test/stub_whispernet_client.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/test/fake_directive_handler.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"

using google::protobuf::MessageLite;
using google::protobuf::RepeatedPtrField;

using testing::ElementsAre;
using testing::Property;
using testing::SizeIs;

using audio_modem::AudioToken;
using audio_modem::WhispernetClient;

namespace copresence {

namespace {

const char kChromeVersion[] = "Chrome Version String";

void IgnoreMessages(
    const RepeatedPtrField<SubscribedMessage>& /* messages */) {}

}  // namespace

class RpcHandlerTest : public testing::Test, public CopresenceDelegate {
 public:
  RpcHandlerTest()
      : whispernet_client_(new audio_modem::StubWhispernetClient),
        // TODO(ckehoe): Use a FakeCopresenceState here
        // and test that it gets called correctly.
        rpc_handler_(this,
                     &directive_handler_,
                     nullptr,
                     nullptr,
                     base::Bind(&IgnoreMessages),
                     base::Bind(&RpcHandlerTest::CaptureHttpPost,
                                base::Unretained(this))),
        status_(SUCCESS) {}

  // CopresenceDelegate implementation

  void HandleMessages(const std::string& /* app_id */,
                      const std::string& subscription_id,
                      const std::vector<Message>& messages) override {
    NOTREACHED();
  }

  void HandleStatusUpdate(CopresenceStatus /* status */) override {
    NOTREACHED();
  }

  net::URLRequestContextGetter* GetRequestContext() const override {
    return nullptr;
  }

  std::string GetPlatformVersionString() const override {
    return kChromeVersion;
  }

  std::string GetAPIKey(const std::string& app_id) const override {
    return app_id + " API Key";
  }

  WhispernetClient* GetWhispernetClient() override {
    return whispernet_client_.get();
  }

  // TODO(ckehoe): Add GCM tests.
  gcm::GCMDriver* GetGCMDriver() override {
    return nullptr;
  }

  std::string GetDeviceId(bool authenticated) override {
    return device_id_by_auth_state_[authenticated];
  }

  void SaveDeviceId(bool authenticated, const std::string& device_id) override {
    device_id_by_auth_state_[authenticated] = device_id;
  }

 protected:

  // Send test input to RpcHandler

  void RegisterDevice(bool authenticated) {
    rpc_handler_.RegisterDevice(authenticated);
  }

  void SendRegisterResponse(bool authenticated,
                            const std::string& device_id) {
    RegisterDeviceResponse response;
    response.set_registered_device_id(device_id);
    response.mutable_header()->mutable_status()->set_code(OK);

    std::string serialized_response;
    response.SerializeToString(&serialized_response);
    rpc_handler_.RegisterResponseHandler(
        authenticated, false, nullptr, net::HTTP_OK, serialized_response);
  }

  void SendReport(std::unique_ptr<ReportRequest> request,
                  const std::string& app_id,
                  const std::string& auth_token) {
    rpc_handler_.SendReportRequest(std::move(request), app_id, auth_token,
                                   StatusCallback());
  }

  void SendReportResponse(int status_code,
                          std::unique_ptr<ReportResponse> response) {
    response->mutable_header()->mutable_status()->set_code(OK);

    std::string serialized_response;
    response->SerializeToString(&serialized_response);
    rpc_handler_.ReportResponseHandler(
        base::Bind(&RpcHandlerTest::CaptureStatus, base::Unretained(this)),
        nullptr,
        status_code,
        serialized_response);
  }

  // Read and modify RpcHandler state

  void SetAuthToken(const std::string& auth_token) {
    rpc_handler_.auth_token_ = auth_token;
  }

  const ScopedVector<RpcHandler::PendingRequest>& request_queue() const {
    return rpc_handler_.pending_requests_queue_;
  }

  void AddInvalidToken(const std::string& token) {
    rpc_handler_.invalid_audio_token_cache_.Add(token, true);
  }

  bool TokenIsInvalid(const std::string& token) {
    return rpc_handler_.invalid_audio_token_cache_.HasKey(token);
  }

  // For rpc_handler_.invalid_audio_token_cache_
  base::MessageLoop message_loop_;

  std::unique_ptr<WhispernetClient> whispernet_client_;
  FakeDirectiveHandler directive_handler_;
  RpcHandler rpc_handler_;

  std::map<bool, std::string> device_id_by_auth_state_;

  CopresenceStatus status_;
  std::string rpc_name_;
  std::string api_key_;
  std::string auth_token_;
  ScopedVector<MessageLite> request_protos_;

 private:
  void CaptureHttpPost(
      net::URLRequestContextGetter* url_context_getter,
      const std::string& rpc_name,
      const std::string& api_key,
      const std::string& auth_token,
      std::unique_ptr<MessageLite> request_proto,
      const RpcHandler::PostCleanupCallback& response_callback) {
    rpc_name_ = rpc_name;
    api_key_ = api_key;
    auth_token_ = auth_token;
    request_protos_.push_back(request_proto.release());
  }

  void CaptureStatus(CopresenceStatus status) {
    status_ = status;
  }
};

TEST_F(RpcHandlerTest, RegisterDevice) {
  RegisterDevice(false);
  EXPECT_THAT(request_protos_, SizeIs(1));
  const RegisterDeviceRequest* registration =
      static_cast<RegisterDeviceRequest*>(request_protos_[0]);
  EXPECT_EQ(CHROME, registration->device_identifiers().registrant().type());

  SetAuthToken("Register auth");
  RegisterDevice(true);
  EXPECT_THAT(request_protos_, SizeIs(2));
  registration = static_cast<RegisterDeviceRequest*>(request_protos_[1]);
  EXPECT_FALSE(registration->has_device_identifiers());
}

TEST_F(RpcHandlerTest, RequestQueuing) {
  // Send a report.
  ReportRequest* report = new ReportRequest;
  report->mutable_manage_messages_request()->add_id_to_unpublish("unpublish");
  SendReport(base::WrapUnique(report), "Q App ID", "Q Auth Token");
  EXPECT_THAT(request_queue(), SizeIs(1));
  EXPECT_TRUE(request_queue()[0]->authenticated);

  // Check for registration request.
  EXPECT_THAT(request_protos_, SizeIs(1));
  const RegisterDeviceRequest* registration =
      static_cast<RegisterDeviceRequest*>(request_protos_[0]);
  EXPECT_FALSE(registration->device_identifiers().has_registrant());
  EXPECT_EQ("Q Auth Token", auth_token_);

  // Send a second report.
  report = new ReportRequest;
  report->mutable_manage_subscriptions_request()->add_id_to_unsubscribe(
      "unsubscribe");
  SendReport(base::WrapUnique(report), "Q App ID", "Q Auth Token");
  EXPECT_THAT(request_protos_, SizeIs(1));
  EXPECT_THAT(request_queue(), SizeIs(2));
  EXPECT_TRUE(request_queue()[1]->authenticated);

  // Send an anonymous report.
  report = new ReportRequest;
  report->mutable_update_signals_request()->add_token_observation()
      ->set_token_id("Q Audio Token");
  SendReport(base::WrapUnique(report), "Q App ID", "");
  EXPECT_THAT(request_queue(), SizeIs(3));
  EXPECT_FALSE(request_queue()[2]->authenticated);

  // Check for another registration request.
  EXPECT_THAT(request_protos_, SizeIs(2));
  registration = static_cast<RegisterDeviceRequest*>(request_protos_[1]);
  EXPECT_TRUE(registration->device_identifiers().has_registrant());
  EXPECT_EQ("", auth_token_);

  // Respond to the first registration.
  SendRegisterResponse(true, "Q Auth Device ID");
  EXPECT_EQ("Q Auth Device ID", device_id_by_auth_state_[true]);

  // Check that queued reports are sent.
  EXPECT_THAT(request_protos_, SizeIs(4));
  EXPECT_THAT(request_queue(), SizeIs(1));
  EXPECT_THAT(directive_handler_.removed_directives(),
              ElementsAre("unpublish", "unsubscribe"));
  report = static_cast<ReportRequest*>(request_protos_[2]);
  EXPECT_EQ("unpublish", report->manage_messages_request().id_to_unpublish(0));
  report = static_cast<ReportRequest*>(request_protos_[3]);
  EXPECT_EQ("unsubscribe",
            report->manage_subscriptions_request().id_to_unsubscribe(0));

  // Respond to the second registration.
  SendRegisterResponse(false, "Q Anonymous Device ID");
  EXPECT_EQ("Q Anonymous Device ID", device_id_by_auth_state_[false]);

  // Check for last report.
  EXPECT_THAT(request_protos_, SizeIs(5));
  EXPECT_TRUE(request_queue().empty());
  report = static_cast<ReportRequest*>(request_protos_[4]);
  EXPECT_EQ("Q Audio Token",
            report->update_signals_request().token_observation(0).token_id());
}

TEST_F(RpcHandlerTest, CreateRequestHeader) {
  device_id_by_auth_state_[true] = "CreateRequestHeader Device ID";
  SendReport(base::WrapUnique(new ReportRequest), "CreateRequestHeader App",
             "CreateRequestHeader Auth Token");

  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  EXPECT_EQ("CreateRequestHeader App API Key", api_key_);
  EXPECT_EQ("CreateRequestHeader Auth Token", auth_token_);
  const ReportRequest* report = static_cast<ReportRequest*>(request_protos_[0]);
  EXPECT_EQ(kChromeVersion,
            report->header().framework_version().version_name());
  EXPECT_EQ("CreateRequestHeader App",
            report->header().client_version().client());
  EXPECT_EQ("CreateRequestHeader Device ID",
            report->header().registered_device_id());
  EXPECT_EQ(CHROME_PLATFORM_TYPE,
            report->header().device_fingerprint().type());
}

TEST_F(RpcHandlerTest, ReportTokens) {
  std::vector<AudioToken> test_tokens;
  test_tokens.push_back(AudioToken("token 1", false));
  test_tokens.push_back(AudioToken("token 2", false));
  test_tokens.push_back(AudioToken("token 3", true));
  AddInvalidToken("token 2");

  device_id_by_auth_state_[false] = "ReportTokens Anonymous Device";
  device_id_by_auth_state_[true] = "ReportTokens Auth Device";
  SetAuthToken("ReportTokens Auth");

  rpc_handler_.ReportTokens(test_tokens);
  EXPECT_EQ(RpcHandler::kReportRequestRpcName, rpc_name_);
  EXPECT_EQ(" API Key", api_key_);
  EXPECT_THAT(request_protos_, SizeIs(2));
  const ReportRequest* report = static_cast<ReportRequest*>(request_protos_[0]);
  RepeatedPtrField<TokenObservation> tokens_sent =
      report->update_signals_request().token_observation();
  EXPECT_THAT(tokens_sent, ElementsAre(
      Property(&TokenObservation::token_id, "token 1"),
      Property(&TokenObservation::token_id, "token 3"),
      Property(&TokenObservation::token_id, "current audible"),
      Property(&TokenObservation::token_id, "current inaudible")));
}

TEST_F(RpcHandlerTest, ReportResponseHandler) {
  // Fail on HTTP status != 200.
  std::unique_ptr<ReportResponse> response(new ReportResponse);
  status_ = SUCCESS;
  SendReportResponse(net::HTTP_BAD_REQUEST, std::move(response));
  EXPECT_EQ(FAIL, status_);

  // Construct a test ReportResponse.
  response.reset(new ReportResponse);
  response->mutable_header()->mutable_status()->set_code(OK);
  UpdateSignalsResponse* update_response =
      response->mutable_update_signals_response();
  update_response->set_status(util::error::OK);
  Token* invalid_token = update_response->add_token();
  invalid_token->set_id("bad token");
  invalid_token->set_status(INVALID);
  update_response->add_directive()->set_subscription_id("Subscription 1");
  update_response->add_directive()->set_subscription_id("Subscription 2");

  // Check processing.
  status_ = FAIL;
  SendReportResponse(net::HTTP_OK, std::move(response));
  EXPECT_EQ(SUCCESS, status_);
  EXPECT_TRUE(TokenIsInvalid("bad token"));
  EXPECT_THAT(directive_handler_.added_directives(),
              ElementsAre("Subscription 1", "Subscription 2"));
}

}  // namespace copresence
