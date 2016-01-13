// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/browser/renderer_host/media/webrtc_identity_service_host.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char FAKE_ORIGIN[] = "http://fake.com";
const char FAKE_IDENTITY_NAME[] = "fake identity";
const char FAKE_COMMON_NAME[] = "fake common name";
const char FAKE_CERTIFICATE[] = "fake cert";
const char FAKE_PRIVATE_KEY[] = "fake private key";
const int FAKE_RENDERER_ID = 10;
const int FAKE_SEQUENCE_NUMBER = 1;

class MockWebRTCIdentityStore : public WebRTCIdentityStore {
 public:
  MockWebRTCIdentityStore() : WebRTCIdentityStore(base::FilePath(), NULL) {}

  virtual base::Closure RequestIdentity(
      const GURL& origin,
      const std::string& identity_name,
      const std::string& common_name,
      const CompletionCallback& callback) OVERRIDE {
    EXPECT_TRUE(callback_.is_null());

    callback_ = callback;
    return base::Bind(&MockWebRTCIdentityStore::OnCancel,
                      base::Unretained(this));
  }

  bool HasPendingRequest() const { return !callback_.is_null(); }

  void RunCompletionCallback(int error,
                             const std::string& cert,
                             const std::string& key) {
    callback_.Run(error, cert, key);
    callback_.Reset();
  }

 private:
  virtual ~MockWebRTCIdentityStore() {}

  void OnCancel() { callback_.Reset(); }

  CompletionCallback callback_;
};

class WebRTCIdentityServiceHostForTest : public WebRTCIdentityServiceHost {
 public:
  explicit WebRTCIdentityServiceHostForTest(WebRTCIdentityStore* identity_store)
      : WebRTCIdentityServiceHost(FAKE_RENDERER_ID, identity_store) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->Add(FAKE_RENDERER_ID);
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    messages_.push_back(*message);
    delete message;
    return true;
  }

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    return WebRTCIdentityServiceHost::OnMessageReceived(message);
  }

  IPC::Message GetLastMessage() { return messages_.back(); }

  int GetNumberOfMessages() { return messages_.size(); }

  void ClearMessages() { messages_.clear(); }

 private:
  virtual ~WebRTCIdentityServiceHostForTest() {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->Remove(FAKE_RENDERER_ID);
  }

  std::deque<IPC::Message> messages_;
};

class WebRTCIdentityServiceHostTest : public ::testing::Test {
 public:
  WebRTCIdentityServiceHostTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        store_(new MockWebRTCIdentityStore()),
        host_(new WebRTCIdentityServiceHostForTest(store_.get())) {}

  void SendRequestToHost() {
    host_->OnMessageReceived(
        WebRTCIdentityMsg_RequestIdentity(FAKE_SEQUENCE_NUMBER,
                                          GURL(FAKE_ORIGIN),
                                          FAKE_IDENTITY_NAME,
                                          FAKE_COMMON_NAME));
  }

  void SendCancelRequestToHost() {
    host_->OnMessageReceived(WebRTCIdentityMsg_CancelRequest());
  }

  void VerifyRequestFailedMessage(int error) {
    EXPECT_EQ(1, host_->GetNumberOfMessages());
    IPC::Message ipc = host_->GetLastMessage();
    EXPECT_EQ(ipc.type(), WebRTCIdentityHostMsg_RequestFailed::ID);

    Tuple2<int, int> error_in_message;
    WebRTCIdentityHostMsg_RequestFailed::Read(&ipc, &error_in_message);
    EXPECT_EQ(FAKE_SEQUENCE_NUMBER, error_in_message.a);
    EXPECT_EQ(error, error_in_message.b);
  }

  void VerifyIdentityReadyMessage(const std::string& cert,
                                  const std::string& key) {
    EXPECT_EQ(1, host_->GetNumberOfMessages());
    IPC::Message ipc = host_->GetLastMessage();
    EXPECT_EQ(ipc.type(), WebRTCIdentityHostMsg_IdentityReady::ID);

    Tuple3<int, std::string, std::string> identity_in_message;
    WebRTCIdentityHostMsg_IdentityReady::Read(&ipc, &identity_in_message);
    EXPECT_EQ(FAKE_SEQUENCE_NUMBER, identity_in_message.a);
    EXPECT_EQ(cert, identity_in_message.b);
    EXPECT_EQ(key, identity_in_message.c);
  }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_refptr<MockWebRTCIdentityStore> store_;
  scoped_refptr<WebRTCIdentityServiceHostForTest> host_;
};

}  // namespace

TEST_F(WebRTCIdentityServiceHostTest, TestSendAndCancelRequest) {
  SendRequestToHost();
  EXPECT_TRUE(store_->HasPendingRequest());
  SendCancelRequestToHost();
  EXPECT_FALSE(store_->HasPendingRequest());
}

TEST_F(WebRTCIdentityServiceHostTest, TestOnlyOneRequestAllowed) {
  SendRequestToHost();
  EXPECT_TRUE(store_->HasPendingRequest());
  EXPECT_EQ(0, host_->GetNumberOfMessages());
  SendRequestToHost();

  VerifyRequestFailedMessage(net::ERR_INSUFFICIENT_RESOURCES);
}

TEST_F(WebRTCIdentityServiceHostTest, TestOnIdentityReady) {
  SendRequestToHost();
  store_->RunCompletionCallback(net::OK, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY);
  VerifyIdentityReadyMessage(FAKE_CERTIFICATE, FAKE_PRIVATE_KEY);
}

TEST_F(WebRTCIdentityServiceHostTest, TestOnRequestFailed) {
  SendRequestToHost();
  store_->RunCompletionCallback(net::ERR_KEY_GENERATION_FAILED, "", "");
  VerifyRequestFailedMessage(net::ERR_KEY_GENERATION_FAILED);
}

TEST_F(WebRTCIdentityServiceHostTest, TestOriginAccessDenied) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  policy->Remove(FAKE_RENDERER_ID);

  SendRequestToHost();
  VerifyRequestFailedMessage(net::ERR_ACCESS_DENIED);
}

// Verifies that we do not crash if we try to cancel a completed request.
TEST_F(WebRTCIdentityServiceHostTest, TestCancelAfterRequestCompleted) {
  SendRequestToHost();
  store_->RunCompletionCallback(net::OK, FAKE_CERTIFICATE, FAKE_PRIVATE_KEY);
  SendCancelRequestToHost();
}

}  // namespace content
