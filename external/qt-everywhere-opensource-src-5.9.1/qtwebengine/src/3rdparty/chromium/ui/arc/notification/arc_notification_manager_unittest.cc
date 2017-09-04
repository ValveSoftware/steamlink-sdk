// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/arc/instance_holder.h"
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
  }

  void AddNotification(
      std::unique_ptr<message_center::Notification> notification) override {
    visible_notifications_.insert(notification.get());
    std::string id = notification->id();
    owned_notifications_[id] = std::move(notification);
  }

  void RemoveNotification(const std::string& id, bool by_user) override {
    auto it = owned_notifications_.find(id);
    if (it == owned_notifications_.end())
      return;

    visible_notifications_.erase(it->second.get());
    owned_notifications_.erase(it);
  }

  const message_center::NotificationList::Notifications&
  GetVisibleNotifications() override {
    return visible_notifications_;
  }

 private:
  message_center::NotificationList::Notifications visible_notifications_;
  std::map<std::string, std::unique_ptr<message_center::Notification>>
      owned_notifications_;

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
  ~ArcNotificationManagerTest() override { base::RunLoop().RunUntilIdle(); }

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
    data->icon_data = icon_data;

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
    arc_notifications_instance_.reset(new FakeNotificationsInstance());
    service_.reset(new FakeArcBridgeService());
    message_center_.reset(new MockMessageCenter());

    arc_notification_manager_.reset(new ArcNotificationManager(
        service(), EmptyAccountId(), message_center_.get()));

    NotificationsObserver observer;
    service_->notifications()->AddObserver(&observer);
    service_->notifications()->SetInstance(arc_notifications_instance_.get());

    while (!observer.IsReady())
      base::RunLoop().RunUntilIdle();

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
