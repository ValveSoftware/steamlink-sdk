// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/arc/instance_holder.h"
#include "components/arc/test/fake_arc_bridge_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "components/arc/test/fake_notifications_instance.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/arc/notification/arc_notification_manager.h"
#include "ui/message_center/fake_message_center.h"

namespace arc {

namespace {

const char kDummyNotificationKey[] = "DUMMY_NOTIFICATION_KEY";

class MockMessageCenter : public message_center::FakeMessageCenter {
 public:
  MockMessageCenter() {}
  ~MockMessageCenter() override {
    STLDeleteContainerPointers(
        visible_notifications_.begin(), visible_notifications_.end());
  }

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    visible_notifications_.insert(notification.release());
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    for (auto it = visible_notifications_.begin();
         it != visible_notifications_.end(); it++) {
      if ((*it)->id() == id) {
        delete *it;
        visible_notifications_.erase(it);
        return;
      }
    }
  }

  const message_center::NotificationList::Notifications&
  GetVisibleNotifications() override {
    return visible_notifications_;
  }

 private:
  message_center::NotificationList::Notifications visible_notifications_;

  DISALLOW_COPY_AND_ASSIGN(MockMessageCenter);
};

class NotificationsObserver
    : public InstanceHolder<mojom::NotificationsInstance>::Observer {
 public:
  NotificationsObserver() = default;
  void OnInstanceReady() override { ready_ = true; }

  bool IsReady() { return ready_; }

 private:
  bool ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationsObserver);
};

}  // anonymous namespace

class ArcNotificationManagerTest : public testing::Test {
 public:
  ArcNotificationManagerTest() {}
  ~ArcNotificationManagerTest() override { loop_.RunUntilIdle(); }

 protected:
  FakeArcBridgeService* service() { return service_.get(); }
  FakeNotificationsInstance* arc_notifications_instance() {
    return arc_notifications_instance_.get();
  }
  ArcNotificationManager* arc_notification_manager() {
    return arc_notification_manager_.get();
  }
  MockMessageCenter* message_center() { return message_center_.get(); }

  std::string CreateNotification() {
    return CreateNotificationWithKey(kDummyNotificationKey);
  }

  std::string CreateNotificationWithKey(const std::string& key) {
    auto data = mojom::ArcNotificationData::New();
    data->key = key;
    data->title = "TITLE";
    data->message = "MESSAGE";

    std::vector<unsigned char> icon_data;
    data->icon_data.Swap(&icon_data);

    arc_notification_manager()->OnNotificationPosted(std::move(data));

    return key;
  }

 private:
  base::MessageLoop loop_;
  std::unique_ptr<FakeArcBridgeService> service_;
  std::unique_ptr<FakeNotificationsInstance> arc_notifications_instance_;
  std::unique_ptr<ArcNotificationManager> arc_notification_manager_;
  std::unique_ptr<MockMessageCenter> message_center_;

  void SetUp() override {
    mojom::NotificationsInstancePtr arc_notifications_instance;
    arc_notifications_instance_.reset(
        new FakeNotificationsInstance(GetProxy(&arc_notifications_instance)));
    service_.reset(new FakeArcBridgeService());
    message_center_.reset(new MockMessageCenter());

    arc_notification_manager_.reset(new ArcNotificationManager(
        service(), EmptyAccountId(), message_center_.get()));

    NotificationsObserver observer;
    service_->notifications()->AddObserver(&observer);
    service_->OnNotificationsInstanceReady(
        std::move(arc_notifications_instance));

    while (!observer.IsReady())
      loop_.RunUntilIdle();

    service_->notifications()->RemoveObserver(&observer);
  }

  void TearDown() override {
    arc_notification_manager_.reset();
    message_center_.reset();
    service_.reset();
    arc_notifications_instance_.reset();
  }

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationManagerTest);
};

TEST_F(ArcNotificationManagerTest, NotificationCreatedAndRemoved) {
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  std::string key = CreateNotification();
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  arc_notification_manager()->OnNotificationRemoved(key);

  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
}

TEST_F(ArcNotificationManagerTest, NotificationRemovedByChrome) {
  service()->SetReady();
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  std::string key = CreateNotification();
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  {
    message_center::Notification* notification =
        *message_center()->GetVisibleNotifications().begin();
    notification->delegate()->Close(true /* by_user */);
    // |notification| gets stale here.
  }

  arc_notifications_instance()->WaitForIncomingMethodCall();

  ASSERT_EQ(1u, arc_notifications_instance()->events().size());
  EXPECT_EQ(key, arc_notifications_instance()->events().at(0).first);
  EXPECT_EQ(mojom::ArcNotificationEvent::CLOSED,
            arc_notifications_instance()->events().at(0).second);
}

TEST_F(ArcNotificationManagerTest, NotificationRemovedByConnectionClose) {
  service()->SetReady();
  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
  CreateNotificationWithKey("notification1");
  CreateNotificationWithKey("notification2");
  CreateNotificationWithKey("notification3");
  EXPECT_EQ(3u, message_center()->GetVisibleNotifications().size());

  arc_notification_manager()->OnInstanceClosed();

  EXPECT_EQ(0u, message_center()->GetVisibleNotifications().size());
}

}  // namespace arc
