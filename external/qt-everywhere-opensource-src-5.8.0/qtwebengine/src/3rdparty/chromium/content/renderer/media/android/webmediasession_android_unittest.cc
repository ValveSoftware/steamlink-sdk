// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediasession_android.h"

#include <memory>

#include "content/common/media/media_session_messages_android.h"
#include "content/renderer/media/android/renderer_media_session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

const int kRouteId = 0;

}  // anonymous namespace

class WebMediaSessionTest : public testing::Test {
 public:
  void OnSuccess() { ++success_count_; }
  void OnError() { ++error_count_; }

  bool SessionManagerHasSession(RendererMediaSessionManager* session_manager,
                                WebMediaSessionAndroid* session) {
    for (auto& iter : session_manager->sessions_) {
      if (iter.second == session)
        return true;
    }
    return false;
  }

  bool IsSessionManagerEmpty(RendererMediaSessionManager* session_manager) {
    return session_manager->sessions_.empty();
  }

 protected:
  int success_count_ = 0;
  int error_count_ = 0;
};

class TestActivateCallback : public blink::WebMediaSessionActivateCallback {
 public:
  TestActivateCallback(WebMediaSessionTest* test) : test_(test) {}

 private:
  void onSuccess() override { test_->OnSuccess(); }
  void onError(const blink::WebMediaSessionError&) override {
    test_->OnError();
  }

  WebMediaSessionTest* test_;
};

class TestDeactivateCallback : public blink::WebMediaSessionDeactivateCallback {
 public:
  TestDeactivateCallback(WebMediaSessionTest* test) : test_(test) {}

 private:
  void onSuccess() override { test_->OnSuccess(); }
  void onError() override { test_->OnError(); }

  WebMediaSessionTest* test_;
};

TEST_F(WebMediaSessionTest, TestRegistration) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
  {
    std::unique_ptr<WebMediaSessionAndroid> session(
        new WebMediaSessionAndroid(session_manager.get()));
    EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session.get()));
  }
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
}

TEST_F(WebMediaSessionTest, TestMultipleRegistration) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));

  {
    std::unique_ptr<WebMediaSessionAndroid> session1(
        new WebMediaSessionAndroid(session_manager.get()));
    EXPECT_TRUE(
        SessionManagerHasSession(session_manager.get(), session1.get()));

    {
      std::unique_ptr<WebMediaSessionAndroid> session2(
          new WebMediaSessionAndroid(session_manager.get()));
      EXPECT_TRUE(
          SessionManagerHasSession(session_manager.get(), session2.get()));
    }

    EXPECT_TRUE(
        SessionManagerHasSession(session_manager.get(), session1.get()));
  }

  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
}

TEST_F(WebMediaSessionTest, TestMultipleRegistrationOutOfOrder) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));

  WebMediaSessionAndroid* session1 =
      new WebMediaSessionAndroid(session_manager.get());
  EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session1));

  WebMediaSessionAndroid* session2 =
      new WebMediaSessionAndroid(session_manager.get());
  EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session2));

  delete session1;
  EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session2));

  delete session2;
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
}

TEST_F(WebMediaSessionTest, ActivationOutOfOrder) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));

  std::unique_ptr<WebMediaSessionAndroid> session(
      new WebMediaSessionAndroid(session_manager.get()));

  // Request activate three times
  session->activate(new TestActivateCallback(this));  // request 1
  session->activate(new TestActivateCallback(this));  // request 2
  session->activate(new TestActivateCallback(this));  // request 3

  // Confirm activation out of order
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 3, true));

  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 2, true));

  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 1, true));

  EXPECT_EQ(3, success_count_);
  EXPECT_EQ(0, error_count_);
}

TEST_F(WebMediaSessionTest, ActivationInOrder) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));

  std::unique_ptr<WebMediaSessionAndroid> session(
      new WebMediaSessionAndroid(session_manager.get()));

  // Request activate three times
  session->activate(new TestActivateCallback(this));  // request 1
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 1, true));

  session->activate(new TestActivateCallback(this));  // request 2
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 2, true));

  session->activate(new TestActivateCallback(this));  // request 3
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 3, true));

  EXPECT_EQ(3, success_count_);
  EXPECT_EQ(0, error_count_);
}

TEST_F(WebMediaSessionTest, ActivationInFlight) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));

  std::unique_ptr<WebMediaSessionAndroid> session(
      new WebMediaSessionAndroid(session_manager.get()));

  session->activate(new TestActivateCallback(this));  // request 1
  session->activate(new TestActivateCallback(this));  // request 2
  session->activate(new TestActivateCallback(this));  // request 3

  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 1, true));

  session->activate(new TestActivateCallback(this));  // request 4
  session->activate(new TestActivateCallback(this));  // request 5

  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 3, true));
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 2, true));
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 5, true));
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 4, true));

  EXPECT_EQ(5, success_count_);
  EXPECT_EQ(0, error_count_);
}

TEST_F(WebMediaSessionTest, ActivationFailure) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));

  std::unique_ptr<WebMediaSessionAndroid> session(
      new WebMediaSessionAndroid(session_manager.get()));

  session->activate(new TestActivateCallback(this));  // request 1
  session->activate(new TestActivateCallback(this));  // request 2
  session->activate(new TestActivateCallback(this));  // request 3

  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 1, true));
  EXPECT_EQ(1, success_count_);
  EXPECT_EQ(0, error_count_);
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 2, false));
  EXPECT_EQ(1, success_count_);
  EXPECT_EQ(1, error_count_);
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidActivate(kRouteId, 3, true));
  EXPECT_EQ(2, success_count_);
  EXPECT_EQ(1, error_count_);
}

TEST_F(WebMediaSessionTest, Deactivation) {
  std::unique_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));

  std::unique_ptr<WebMediaSessionAndroid> session(
      new WebMediaSessionAndroid(session_manager.get()));

  // Request deactivate three times
  session->deactivate(new TestDeactivateCallback(this));  // request 1
  session->deactivate(new TestDeactivateCallback(this));  // request 2
  session->deactivate(new TestDeactivateCallback(this));  // request 3
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidDeactivate(kRouteId, 1));
  EXPECT_EQ(1, success_count_);
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidDeactivate(kRouteId, 2));
  EXPECT_EQ(2, success_count_);
  session_manager->OnMessageReceived(
      MediaSessionMsg_DidDeactivate(kRouteId, 3));
  EXPECT_EQ(3, success_count_);
  EXPECT_EQ(0, error_count_);
}

}  // namespace content
