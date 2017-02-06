// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "content/renderer/media/webrtc_identity_service.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const char FAKE_URL[] = "http://fake.com";
static const char FAKE_IDENTITY_NAME[] = "fake identity";
static const char FAKE_COMMON_NAME[] = "fake common name";
static const char FAKE_CERTIFICATE[] = "fake cert";
static const char FAKE_PRIVATE_KEY[] = "fake private key";
static const int FAKE_ERROR = 100;

class WebRtcIdentityServiceForTest : public WebRTCIdentityService {
 public:
  bool Send(IPC::Message* message) override {
    messages_.push_back(*message);
    delete message;
    return true;
  }

  bool OnControlMessageReceived(const IPC::Message& message) override {
    return WebRTCIdentityService::OnControlMessageReceived(message);
  }

  IPC::Message GetLastMessage() { return messages_.back(); }

  int GetNumberOfMessages() { return messages_.size(); }

  void ClearMessages() { messages_.clear(); }

 private:
  std::deque<IPC::Message> messages_;
};

class WebRtcIdentityServiceTest : public ::testing::Test {
 public:
  WebRtcIdentityServiceTest()
      : service_(new WebRtcIdentityServiceForTest()), last_error_(0) {}

 protected:
  void OnIdentityReady(const std::string& cert, const std::string& key) {
    last_certificate_ = cert;
    last_private_key_ = key;
  }

  void OnRequestFailed(int error) { last_error_ = error; }

  void ResetRequestResult() {
    last_certificate_ = "";
    last_private_key_ = "";
    last_error_ = 0;
  }

  int RequestIdentity() {
    return service_->RequestIdentity(
        GURL(FAKE_URL), GURL(FAKE_URL), FAKE_IDENTITY_NAME, FAKE_COMMON_NAME,
        base::Bind(&WebRtcIdentityServiceTest::OnIdentityReady,
                   base::Unretained(this)),
        base::Bind(&WebRtcIdentityServiceTest::OnRequestFailed,
                   base::Unretained(this)));
  }

  std::unique_ptr<WebRtcIdentityServiceForTest> service_;
  std::string last_certificate_;
  std::string last_private_key_;
  int last_error_;
};

}  // namespace

TEST_F(WebRtcIdentityServiceTest, TestSendRequest) {
  RequestIdentity();

  IPC::Message ipc = service_->GetLastMessage();
  EXPECT_EQ(ipc.type(), WebRTCIdentityMsg_RequestIdentity::ID);
}

TEST_F(WebRtcIdentityServiceTest, TestSuccessCallback) {
  int id = RequestIdentity();

  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      id, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));
  EXPECT_EQ(FAKE_CERTIFICATE, last_certificate_);
  EXPECT_EQ(FAKE_PRIVATE_KEY, last_private_key_);
}

TEST_F(WebRtcIdentityServiceTest, TestFailureCallback) {
  int id = RequestIdentity();

  service_->OnControlMessageReceived(
      WebRTCIdentityHostMsg_RequestFailed(id, FAKE_ERROR));
  EXPECT_EQ(FAKE_ERROR, last_error_);
}

TEST_F(WebRtcIdentityServiceTest, TestCancelRequest) {
  int request_id = RequestIdentity();
  service_->ClearMessages();

  service_->CancelRequest(request_id);

  IPC::Message ipc = service_->GetLastMessage();
  EXPECT_EQ(ipc.type(), WebRTCIdentityMsg_CancelRequest::ID);
}

TEST_F(WebRtcIdentityServiceTest, TestQueuedRequestSentAfterSuccess) {
  int id = RequestIdentity();
  RequestIdentity();
  EXPECT_EQ(1, service_->GetNumberOfMessages());
  service_->ClearMessages();

  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      id, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));

  IPC::Message ipc = service_->GetLastMessage();
  EXPECT_EQ(ipc.type(), WebRTCIdentityMsg_RequestIdentity::ID);
}

TEST_F(WebRtcIdentityServiceTest, TestQueuedRequestSentAfterFailure) {
  int id = RequestIdentity();
  RequestIdentity();
  EXPECT_EQ(1, service_->GetNumberOfMessages());
  service_->ClearMessages();

  service_->OnControlMessageReceived(
      WebRTCIdentityHostMsg_RequestFailed(id, FAKE_ERROR));

  IPC::Message ipc = service_->GetLastMessage();
  EXPECT_EQ(ipc.type(), WebRTCIdentityMsg_RequestIdentity::ID);
}

TEST_F(WebRtcIdentityServiceTest, TestQueuedRequestSentAfterCancelOutstanding) {
  int outstand_request_id = RequestIdentity();
  RequestIdentity();

  EXPECT_EQ(1, service_->GetNumberOfMessages());
  service_->ClearMessages();

  service_->CancelRequest(outstand_request_id);

  // Should have two messages sent: one for cancelling the outstanding request,
  // one for requesting the queued request.
  EXPECT_EQ(2, service_->GetNumberOfMessages());
  IPC::Message ipc = service_->GetLastMessage();
  EXPECT_EQ(ipc.type(), WebRTCIdentityMsg_RequestIdentity::ID);
}

TEST_F(WebRtcIdentityServiceTest, TestCancelQueuedRequest) {
  int sent_id = RequestIdentity();
  int queued_request_id = RequestIdentity();
  EXPECT_EQ(1, service_->GetNumberOfMessages());
  service_->ClearMessages();

  service_->CancelRequest(queued_request_id);

  // Verifies that the queued request is not sent after the outstanding request
  // returns.
  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      sent_id, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));

  EXPECT_EQ(0, service_->GetNumberOfMessages());
}

TEST_F(WebRtcIdentityServiceTest, TestQueuedRequestSuccessCallback) {
  int id1 = RequestIdentity();
  int id2 = RequestIdentity();

  // Completes the outstanding request.
  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      id1, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));
  EXPECT_EQ(FAKE_CERTIFICATE, last_certificate_);
  EXPECT_EQ(FAKE_PRIVATE_KEY, last_private_key_);

  ResetRequestResult();

  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      id2, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));
  EXPECT_EQ(FAKE_CERTIFICATE, last_certificate_);
  EXPECT_EQ(FAKE_PRIVATE_KEY, last_private_key_);
}

TEST_F(WebRtcIdentityServiceTest, TestQueuedRequestFailureCallback) {
  int id1 = RequestIdentity();
  int id2 = RequestIdentity();

  // Completes the outstanding request.
  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      id1, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));
  EXPECT_EQ(FAKE_CERTIFICATE, last_certificate_);
  EXPECT_EQ(FAKE_PRIVATE_KEY, last_private_key_);

  ResetRequestResult();

  service_->OnControlMessageReceived(
      WebRTCIdentityHostMsg_RequestFailed(id2, FAKE_ERROR));
  EXPECT_EQ(FAKE_ERROR, last_error_);
}

// Verifies that receiving a response for a cancelled request does not incur the
// callbacks.
TEST_F(WebRtcIdentityServiceTest, TestRequestCompletedAfterCancelled) {
  int id1 = RequestIdentity();
  RequestIdentity();
  service_->CancelRequest(id1);

  service_->OnControlMessageReceived(WebRTCIdentityHostMsg_IdentityReady(
      id1, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY));

  EXPECT_NE(FAKE_CERTIFICATE, last_certificate_);
  EXPECT_NE(FAKE_PRIVATE_KEY, last_private_key_);

  service_->OnControlMessageReceived(
      WebRTCIdentityHostMsg_RequestFailed(id1, FAKE_ERROR));
  EXPECT_NE(FAKE_ERROR, last_error_);
}

}  // namespace content
