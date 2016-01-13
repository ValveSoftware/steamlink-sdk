// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/non_blocking_push_client.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "jingle/notifier/listener/fake_push_client_observer.h"
#include "jingle/notifier/listener/push_client_observer.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

class NonBlockingPushClientTest : public testing::Test {
 protected:
  NonBlockingPushClientTest() : fake_push_client_(NULL) {}

  virtual ~NonBlockingPushClientTest() {}

  virtual void SetUp() OVERRIDE {
    push_client_.reset(
        new NonBlockingPushClient(
            base::MessageLoopProxy::current(),
            base::Bind(&NonBlockingPushClientTest::CreateFakePushClient,
                       base::Unretained(this))));
    push_client_->AddObserver(&fake_observer_);
    // Pump message loop to run CreateFakePushClient.
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    // Clear out any pending notifications before removing observers.
    message_loop_.RunUntilIdle();
    push_client_->RemoveObserver(&fake_observer_);
    push_client_.reset();
    // Then pump message loop to run
    // NonBlockingPushClient::DestroyOnDelegateThread().
    message_loop_.RunUntilIdle();
  }

  scoped_ptr<PushClient> CreateFakePushClient() {
    if (fake_push_client_) {
      ADD_FAILURE();
      return scoped_ptr<PushClient>();
    }
    fake_push_client_ = new FakePushClient();
    return scoped_ptr<PushClient>(fake_push_client_);
  }

  base::MessageLoop message_loop_;
  FakePushClientObserver fake_observer_;
  scoped_ptr<NonBlockingPushClient> push_client_;
  // Owned by |push_client_|.
  FakePushClient* fake_push_client_;
};

// Make sure UpdateSubscriptions() gets delegated properly.
TEST_F(NonBlockingPushClientTest, UpdateSubscriptions) {
  SubscriptionList subscriptions(10);
  subscriptions[0].channel = "channel";
  subscriptions[9].from = "from";

  push_client_->UpdateSubscriptions(subscriptions);
  EXPECT_TRUE(fake_push_client_->subscriptions().empty());
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(
      SubscriptionListsEqual(
          fake_push_client_->subscriptions(), subscriptions));
}

// Make sure UpdateCredentials() gets delegated properly.
TEST_F(NonBlockingPushClientTest, UpdateCredentials) {
  const char kEmail[] = "foo@bar.com";
  const char kToken[] = "baz";

  push_client_->UpdateCredentials(kEmail, kToken);
  EXPECT_TRUE(fake_push_client_->email().empty());
  EXPECT_TRUE(fake_push_client_->token().empty());
  message_loop_.RunUntilIdle();
  EXPECT_EQ(kEmail, fake_push_client_->email());
  EXPECT_EQ(kToken, fake_push_client_->token());
}

Notification MakeTestNotification() {
  Notification notification;
  notification.channel = "channel";
  notification.recipients.resize(10);
  notification.recipients[0].to = "to";
  notification.recipients[9].user_specific_data = "user_specific_data";
  notification.data = "data";
  return notification;
}

// Make sure SendNotification() gets delegated properly.
TEST_F(NonBlockingPushClientTest, SendNotification) {
  const Notification notification = MakeTestNotification();

  push_client_->SendNotification(notification);
  EXPECT_TRUE(fake_push_client_->sent_notifications().empty());
  message_loop_.RunUntilIdle();
  ASSERT_EQ(1u, fake_push_client_->sent_notifications().size());
  EXPECT_TRUE(
      fake_push_client_->sent_notifications()[0].Equals(notification));
}

// Make sure SendPing() gets delegated properly.
TEST_F(NonBlockingPushClientTest, SendPing) {
  push_client_->SendPing();
  EXPECT_EQ(0, fake_push_client_->sent_pings());
  message_loop_.RunUntilIdle();
  ASSERT_EQ(1, fake_push_client_->sent_pings());
}

// Make sure notification state changes get propagated back to the
// parent.
TEST_F(NonBlockingPushClientTest, NotificationStateChange) {
  EXPECT_EQ(DEFAULT_NOTIFICATION_ERROR,
            fake_observer_.last_notifications_disabled_reason());
  fake_push_client_->EnableNotifications();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NO_NOTIFICATION_ERROR,
            fake_observer_.last_notifications_disabled_reason());
  fake_push_client_->DisableNotifications(
      NOTIFICATION_CREDENTIALS_REJECTED);
  message_loop_.RunUntilIdle();
  EXPECT_EQ(NOTIFICATION_CREDENTIALS_REJECTED,
            fake_observer_.last_notifications_disabled_reason());
}

// Make sure incoming notifications get propagated back to the parent.
TEST_F(NonBlockingPushClientTest, OnIncomingNotification) {
  const Notification notification = MakeTestNotification();

  fake_push_client_->SimulateIncomingNotification(notification);
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(
      fake_observer_.last_incoming_notification().Equals(notification));
}

}  // namespace

}  // namespace notifier
