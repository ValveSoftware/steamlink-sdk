// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include <stddef.h>

#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

using base::UTF8ToUTF16;

namespace message_center {

class NotificationListTest : public testing::Test {
 public:
  NotificationListTest() {}
  ~NotificationListTest() override {}

  void SetUp() override {
    message_center_.reset(new FakeMessageCenter());
    notification_list_.reset(new NotificationList(message_center_.get()));
    counter_ = 0;
  }

 protected:
  // Currently NotificationListTest doesn't care about some fields like title or
  // message, so put a simple template on it. Returns the id of the new
  // notification.
  std::string AddNotification(
      const message_center::RichNotificationData& optional_fields) {
    std::string new_id;
    std::unique_ptr<Notification> notification(
        MakeNotification(optional_fields, &new_id));
    notification_list_->AddNotification(std::move(notification));
    counter_++;
    return new_id;
  }

  std::string AddNotification() {
    return AddNotification(message_center::RichNotificationData());
  }

  // Construct a new notification for testing, but don't add it to the list yet.
  std::unique_ptr<Notification> MakeNotification(
      const message_center::RichNotificationData& optional_fields,
      std::string* id_out) {
    *id_out = base::StringPrintf(kIdFormat, counter_);
    std::unique_ptr<Notification> notification(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, *id_out,
        UTF8ToUTF16(base::StringPrintf(kTitleFormat, counter_)),
        UTF8ToUTF16(base::StringPrintf(kMessageFormat, counter_)), gfx::Image(),
        UTF8ToUTF16(kDisplaySource), GURL(),
        NotifierId(NotifierId::APPLICATION, kExtensionId), optional_fields,
        NULL));
    return notification;
  }

  std::unique_ptr<Notification> MakeNotification(std::string* id_out) {
    return MakeNotification(message_center::RichNotificationData(), id_out);
  }

  // Utility methods of AddNotification.
  std::string AddPriorityNotification(NotificationPriority priority) {
    message_center::RichNotificationData optional;
    optional.priority = priority;
    return AddNotification(optional);
  }

  NotificationList::PopupNotifications GetPopups() {
    return notification_list()->GetPopupNotifications(blockers_, NULL);
  }

  size_t GetPopupCounts() {
    return GetPopups().size();
  }

  Notification* GetNotification(const std::string& id) {
    auto iter = notification_list()->GetNotification(id);
    if (iter == notification_list()->notifications_.end())
      return NULL;
    return iter->get();
  }

  NotificationList* notification_list() { return notification_list_.get(); }
  const NotificationBlockers& blockers() const { return blockers_; }

  static const char kIdFormat[];
  static const char kTitleFormat[];
  static const char kMessageFormat[];
  static const char kDisplaySource[];
  static const char kExtensionId[];

 private:
  std::unique_ptr<FakeMessageCenter> message_center_;
  std::unique_ptr<NotificationList> notification_list_;
  NotificationBlockers blockers_;
  size_t counter_;

  DISALLOW_COPY_AND_ASSIGN(NotificationListTest);
};

bool IsInNotifications(const NotificationList::Notifications& notifications,
                       const std::string& id) {
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

const char NotificationListTest::kIdFormat[] = "id%ld";
const char NotificationListTest::kTitleFormat[] = "id%ld";
const char NotificationListTest::kMessageFormat[] = "message%ld";
const char NotificationListTest::kDisplaySource[] = "source";
const char NotificationListTest::kExtensionId[] = "ext";

TEST_F(NotificationListTest, Basic) {
  ASSERT_EQ(0u, notification_list()->NotificationCount(blockers()));
  ASSERT_EQ(0u, notification_list()->UnreadCount(blockers()));

  std::string id0 = AddNotification();
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  std::string id1 = AddNotification();
  EXPECT_EQ(2u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(2u, notification_list()->UnreadCount(blockers()));

  EXPECT_TRUE(notification_list()->HasPopupNotifications(blockers()));
  EXPECT_TRUE(notification_list()->GetNotificationById(id0));
  EXPECT_TRUE(notification_list()->GetNotificationById(id1));
  EXPECT_FALSE(notification_list()->GetNotificationById(id1 + "foo"));

  EXPECT_EQ(2u, GetPopupCounts());

  notification_list()->MarkSinglePopupAsShown(id0, true);
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(2u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list()->RemoveNotification(id0);
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));

  AddNotification();
  EXPECT_EQ(2u, notification_list()->NotificationCount(blockers()));
}

TEST_F(NotificationListTest, MessageCenterVisible) {
  AddNotification();
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  ASSERT_EQ(1u, notification_list()->UnreadCount(blockers()));
  ASSERT_EQ(1u, GetPopupCounts());

  // Resets the unread count and popup counts.
  notification_list()->SetNotificationsShown(blockers(), NULL);
  ASSERT_EQ(0u, notification_list()->UnreadCount(blockers()));
  ASSERT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, UnreadCount) {
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  ASSERT_EQ(2u, notification_list()->UnreadCount(blockers()));

  notification_list()->MarkSinglePopupAsDisplayed(id0);
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));
  notification_list()->MarkSinglePopupAsDisplayed(id0);
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));
  notification_list()->MarkSinglePopupAsDisplayed(id1);
  EXPECT_EQ(0u, notification_list()->UnreadCount(blockers()));
}

TEST_F(NotificationListTest, UpdateNotification) {
  std::string id0 = AddNotification();
  std::string replaced = id0 + "_replaced";
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  std::unique_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE, replaced,
                       UTF8ToUTF16("newtitle"), UTF8ToUTF16("newbody"),
                       gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
                       NotifierId(NotifierId::APPLICATION, kExtensionId),
                       message_center::RichNotificationData(), NULL));
  notification_list()->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  const NotificationList::Notifications notifications =
      notification_list()->GetVisibleNotifications(blockers());
  EXPECT_EQ(replaced, (*notifications.begin())->id());
  EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
  EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());
}

TEST_F(NotificationListTest, GetNotificationsByNotifierId) {
  NotifierId id0(NotifierId::APPLICATION, "ext0");
  NotifierId id1(NotifierId::APPLICATION, "ext1");
  NotifierId id2(GURL("http://example.com"));
  NotifierId id3(NotifierId::SYSTEM_COMPONENT, "system-notifier");
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id0", UTF8ToUTF16("title0"),
      UTF8ToUTF16("message0"), gfx::Image(), UTF8ToUTF16("source0"), GURL(),
      id0, message_center::RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(notification));
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id1", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source0"), GURL(),
      id0, message_center::RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(notification));
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id2", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source1"), GURL(),
      id0, message_center::RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(notification));
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id3", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source2"), GURL(),
      id1, message_center::RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(notification));
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id4", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source2"), GURL(),
      id2, message_center::RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(notification));
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "id5", UTF8ToUTF16("title1"),
      UTF8ToUTF16("message1"), gfx::Image(), UTF8ToUTF16("source2"), GURL(),
      id3, message_center::RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(notification));

  NotificationList::Notifications by_notifier_id =
      notification_list()->GetNotificationsByNotifierId(id0);
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id5"));

  by_notifier_id = notification_list()->GetNotificationsByNotifierId(id1);
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id5"));

  by_notifier_id = notification_list()->GetNotificationsByNotifierId(id2);
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id5"));

  by_notifier_id = notification_list()->GetNotificationsByNotifierId(id3);
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id0"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id1"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id2"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id3"));
  EXPECT_FALSE(IsInNotifications(by_notifier_id, "id4"));
  EXPECT_TRUE(IsInNotifications(by_notifier_id, "id5"));
}

TEST_F(NotificationListTest, OldPopupShouldNotBeHidden) {
  std::vector<std::string> ids;
  for (size_t i = 0; i <= kMaxVisiblePopupNotifications; i++)
    ids.push_back(AddNotification());

  NotificationList::PopupNotifications popups = GetPopups();
  // The popup should contain the oldest kMaxVisiblePopupNotifications. Newer
  // one should come earlier in the popup list. It means, the last element
  // of |popups| should be the firstly added one, and so on.
  EXPECT_EQ(kMaxVisiblePopupNotifications, popups.size());
  NotificationList::PopupNotifications::const_reverse_iterator iter =
      popups.rbegin();
  for (size_t i = 0; i < kMaxVisiblePopupNotifications; ++i, ++iter) {
    EXPECT_EQ(ids[i], (*iter)->id()) << i;
  }

  for (NotificationList::PopupNotifications::const_iterator iter =
           popups.begin(); iter != popups.end(); ++iter) {
    notification_list()->MarkSinglePopupAsShown((*iter)->id(), false);
  }
  popups.clear();
  popups = GetPopups();
  ASSERT_EQ(1u, popups.size());
  EXPECT_EQ(ids.back(), (*popups.begin())->id());
}

TEST_F(NotificationListTest, Priority) {
  ASSERT_EQ(0u, notification_list()->NotificationCount(blockers()));
  ASSERT_EQ(0u, notification_list()->UnreadCount(blockers()));

  // Default priority has the limit on the number of the popups.
  for (size_t i = 0; i <= kMaxVisiblePopupNotifications; ++i)
    AddNotification();
  EXPECT_EQ(kMaxVisiblePopupNotifications + 1,
            notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(kMaxVisiblePopupNotifications, GetPopupCounts());

  // Low priority: not visible to popups.
  notification_list()->SetNotificationsShown(blockers(), NULL);
  EXPECT_EQ(0u, notification_list()->UnreadCount(blockers()));
  AddPriorityNotification(LOW_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications + 2,
            notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));
  EXPECT_EQ(0u, GetPopupCounts());

  // Minimum priority: doesn't update the unread count.
  AddPriorityNotification(MIN_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications + 3,
            notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));
  EXPECT_EQ(0u, GetPopupCounts());

  NotificationList::Notifications notifications =
      notification_list()->GetVisibleNotifications(blockers());
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    notification_list()->RemoveNotification((*iter)->id());
  }

  // Higher priority: no limits to the number of popups.
  for (size_t i = 0; i < kMaxVisiblePopupNotifications * 2; ++i)
    AddPriorityNotification(HIGH_PRIORITY);
  for (size_t i = 0; i < kMaxVisiblePopupNotifications * 2; ++i)
    AddPriorityNotification(MAX_PRIORITY);
  EXPECT_EQ(kMaxVisiblePopupNotifications * 4,
            notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(kMaxVisiblePopupNotifications * 4, GetPopupCounts());
}

TEST_F(NotificationListTest, HasPopupsWithPriority) {
  ASSERT_EQ(0u, notification_list()->NotificationCount(blockers()));
  ASSERT_EQ(0u, notification_list()->UnreadCount(blockers()));

  AddPriorityNotification(MIN_PRIORITY);
  AddPriorityNotification(MAX_PRIORITY);

  EXPECT_EQ(1u, GetPopupCounts());
}

TEST_F(NotificationListTest, HasPopupsWithSystemPriority) {
  ASSERT_EQ(0u, notification_list()->NotificationCount(blockers()));
  ASSERT_EQ(0u, notification_list()->UnreadCount(blockers()));

  std::string normal_id = AddPriorityNotification(DEFAULT_PRIORITY);
  std::string system_id = AddNotification();
  GetNotification(system_id)->SetSystemPriority();

  EXPECT_EQ(2u, GetPopupCounts());

  notification_list()->MarkSinglePopupAsDisplayed(normal_id);
  notification_list()->MarkSinglePopupAsDisplayed(system_id);

  notification_list()->MarkSinglePopupAsShown(normal_id, false);
  notification_list()->MarkSinglePopupAsShown(system_id, false);

  notification_list()->SetNotificationsShown(blockers(), NULL);
  EXPECT_EQ(1u, GetPopupCounts());

  // Mark as read -- emulation of mouse click.
  notification_list()->MarkSinglePopupAsShown(system_id, true);
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, PriorityPromotion) {
  std::string id0 = AddPriorityNotification(LOW_PRIORITY);
  std::string replaced = id0 + "_replaced";
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(0u, GetPopupCounts());
  message_center::RichNotificationData optional;
  optional.priority = 1;
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, replaced,
      UTF8ToUTF16("newtitle"), UTF8ToUTF16("newbody"), gfx::Image(),
      UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), optional, NULL));
  notification_list()->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(1u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(1u, GetPopupCounts());
  const NotificationList::Notifications notifications =
      notification_list()->GetVisibleNotifications(blockers());
  EXPECT_EQ(replaced, (*notifications.begin())->id());
  EXPECT_EQ(UTF8ToUTF16("newtitle"), (*notifications.begin())->title());
  EXPECT_EQ(UTF8ToUTF16("newbody"), (*notifications.begin())->message());
  EXPECT_EQ(1, (*notifications.begin())->priority());
}

TEST_F(NotificationListTest, PriorityPromotionWithPopups) {
  std::string id0 = AddPriorityNotification(LOW_PRIORITY);
  std::string id1 = AddPriorityNotification(DEFAULT_PRIORITY);
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id0 promoted to LOW->DEFAULT, it'll appear as toast (popup).
  message_center::RichNotificationData priority;
  priority.priority = DEFAULT_PRIORITY;
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id0, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), priority, NULL));
  notification_list()->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id0, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // update with no promotion change for id0, it won't appear as a toast.
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id0, UTF8ToUTF16("newtitle2"),
      UTF8ToUTF16("newbody2"), gfx::Image(), UTF8ToUTF16(kDisplaySource),
      GURL(), NotifierId(NotifierId::APPLICATION, kExtensionId), priority,
      NULL));
  notification_list()->UpdateNotificationMessage(id0, std::move(notification));
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 promoted to DEFAULT->HIGH, it'll appear as toast (popup).
  priority.priority = HIGH_PRIORITY;
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id1, UTF8ToUTF16("newtitle"),
      UTF8ToUTF16("newbody"), gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(NotifierId::APPLICATION, kExtensionId), priority, NULL));
  notification_list()->UpdateNotificationMessage(id1, std::move(notification));
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 promoted to HIGH->MAX, it'll appear as toast again.
  priority.priority = MAX_PRIORITY;
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id1, UTF8ToUTF16("newtitle2"),
      UTF8ToUTF16("newbody2"), gfx::Image(), UTF8ToUTF16(kDisplaySource),
      GURL(), NotifierId(NotifierId::APPLICATION, kExtensionId), priority,
      NULL));
  notification_list()->UpdateNotificationMessage(id1, std::move(notification));
  EXPECT_EQ(1u, GetPopupCounts());
  notification_list()->MarkSinglePopupAsShown(id1, true);
  EXPECT_EQ(0u, GetPopupCounts());

  // id1 demoted to MAX->DEFAULT, no appearing as toast.
  priority.priority = DEFAULT_PRIORITY;
  notification.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id1, UTF8ToUTF16("newtitle3"),
      UTF8ToUTF16("newbody3"), gfx::Image(), UTF8ToUTF16(kDisplaySource),
      GURL(), NotifierId(NotifierId::APPLICATION, kExtensionId), priority,
      NULL));
  notification_list()->UpdateNotificationMessage(id1, std::move(notification));
  EXPECT_EQ(0u, GetPopupCounts());
}

TEST_F(NotificationListTest, WebNotificationUpdatePromotion) {
  std::string notification_id = "replaced-web-notification";
  std::unique_ptr<Notification> original_notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      UTF8ToUTF16("Web Notification"), UTF8ToUTF16("Notification contents"),
      gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(GURL("https://example.com/")),
      message_center::RichNotificationData(), NULL));

  EXPECT_EQ(0u, GetPopupCounts());
  notification_list()->AddNotification(std::move(original_notification));
  EXPECT_EQ(1u, GetPopupCounts());

  notification_list()->MarkSinglePopupAsShown(notification_id, true);
  EXPECT_EQ(0u, GetPopupCounts());

  std::unique_ptr<Notification> replaced_notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      UTF8ToUTF16("Web Notification Replacement"),
      UTF8ToUTF16("New notification contents"), gfx::Image(),
      UTF8ToUTF16(kDisplaySource), GURL(),
      NotifierId(GURL("https://example.com/")),
      message_center::RichNotificationData(), NULL));

  // Web Notifications will be re-shown as popups even if their priority didn't
  // change, to match the behavior of the Web Notification API.
  notification_list()->UpdateNotificationMessage(
      notification_id, std::move(replaced_notification));
  EXPECT_EQ(1u, GetPopupCounts());
}

TEST_F(NotificationListTest, NotificationOrderAndPriority) {
  base::Time now = base::Time::Now();
  message_center::RichNotificationData optional;
  optional.timestamp = now;
  optional.priority = 2;
  std::string max_id = AddNotification(optional);

  now += base::TimeDelta::FromSeconds(1);
  optional.timestamp = now;
  optional.priority = 1;
  std::string high_id = AddNotification(optional);

  now += base::TimeDelta::FromSeconds(1);
  optional.timestamp = now;
  optional.priority = 0;
  std::string default_id = AddNotification(optional);

  {
    // Popups: latest comes first.
    NotificationList::PopupNotifications popups = GetPopups();
    EXPECT_EQ(3u, popups.size());
    NotificationList::PopupNotifications::const_iterator iter = popups.begin();
    EXPECT_EQ(default_id, (*iter)->id());
    iter++;
    EXPECT_EQ(high_id, (*iter)->id());
    iter++;
    EXPECT_EQ(max_id, (*iter)->id());
  }
  {
    // Notifications: high priority comes ealier.
    const NotificationList::Notifications notifications =
        notification_list()->GetVisibleNotifications(blockers());
    EXPECT_EQ(3u, notifications.size());
    NotificationList::Notifications::const_iterator iter =
        notifications.begin();
    EXPECT_EQ(max_id, (*iter)->id());
    iter++;
    EXPECT_EQ(high_id, (*iter)->id());
    iter++;
    EXPECT_EQ(default_id, (*iter)->id());
  }
}

TEST_F(NotificationListTest, MarkSinglePopupAsShown) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  std::string id3 = AddNotification();
  ASSERT_EQ(3u, notification_list()->NotificationCount(blockers()));
  ASSERT_EQ(std::min(static_cast<size_t>(3u), kMaxVisiblePopupNotifications),
            GetPopupCounts());
  notification_list()->MarkSinglePopupAsDisplayed(id1);
  notification_list()->MarkSinglePopupAsDisplayed(id2);
  notification_list()->MarkSinglePopupAsDisplayed(id3);

  notification_list()->MarkSinglePopupAsShown(id2, true);
  notification_list()->MarkSinglePopupAsShown(id3, false);
  EXPECT_EQ(3u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));
  EXPECT_EQ(1u, GetPopupCounts());
  NotificationList::PopupNotifications popups = GetPopups();
  EXPECT_EQ(id1, (*popups.begin())->id());

  // The notifications in the NotificationCenter are unaffected by popups shown.
  NotificationList::Notifications notifications =
      notification_list()->GetVisibleNotifications(blockers());
  NotificationList::Notifications::const_iterator iter = notifications.begin();
  EXPECT_EQ(id3, (*iter)->id());
  iter++;
  EXPECT_EQ(id2, (*iter)->id());
  iter++;
  EXPECT_EQ(id1, (*iter)->id());
}

TEST_F(NotificationListTest, UpdateAfterMarkedAsShown) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  notification_list()->MarkSinglePopupAsDisplayed(id1);
  notification_list()->MarkSinglePopupAsDisplayed(id2);

  EXPECT_EQ(2u, GetPopupCounts());

  const Notification* n1 = GetNotification(id1);
  EXPECT_FALSE(n1->shown_as_popup());
  EXPECT_TRUE(n1->IsRead());

  notification_list()->MarkSinglePopupAsShown(id1, true);

  n1 = GetNotification(id1);
  EXPECT_TRUE(n1->shown_as_popup());
  EXPECT_TRUE(n1->IsRead());

  const std::string replaced("test-replaced-id");
  std::unique_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE, replaced,
                       UTF8ToUTF16("newtitle"), UTF8ToUTF16("newbody"),
                       gfx::Image(), UTF8ToUTF16(kDisplaySource), GURL(),
                       NotifierId(NotifierId::APPLICATION, kExtensionId),
                       message_center::RichNotificationData(), NULL));
  notification_list()->UpdateNotificationMessage(id1, std::move(notification));
  n1 = GetNotification(id1);
  EXPECT_TRUE(n1 == NULL);
  const Notification* nr = GetNotification(replaced);
  EXPECT_TRUE(nr->shown_as_popup());
  EXPECT_TRUE(nr->IsRead());
}

TEST_F(NotificationListTest, QuietMode) {
  notification_list()->SetQuietMode(true);
  AddNotification();
  AddPriorityNotification(HIGH_PRIORITY);
  AddPriorityNotification(MAX_PRIORITY);
  EXPECT_EQ(3u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(0u, GetPopupCounts());

  notification_list()->SetQuietMode(false);
  AddNotification();
  EXPECT_EQ(4u, notification_list()->NotificationCount(blockers()));
  EXPECT_EQ(1u, GetPopupCounts());

  // TODO(mukai): Add test of quiet mode with expiration.
}

// Verifies that unread_count doesn't become negative.
TEST_F(NotificationListTest, UnreadCountNoNegative) {
  std::string id = AddNotification();
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));

  notification_list()->MarkSinglePopupAsDisplayed(id);
  EXPECT_EQ(0u, notification_list()->UnreadCount(blockers()));
  notification_list()->MarkSinglePopupAsShown(
      id, false /* mark_notification_as_read */);
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));

  // Updates the notification  and verifies unread_count doesn't change.
  std::unique_ptr<Notification> updated_notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, UTF8ToUTF16("updated"),
      UTF8ToUTF16("updated"), gfx::Image(), base::string16(), GURL(),
      NotifierId(), RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(updated_notification));
  EXPECT_EQ(1u, notification_list()->UnreadCount(blockers()));
}

TEST_F(NotificationListTest, TestPushingShownNotification) {
  // Create a notification and mark it as shown.
  std::string id1;
  std::unique_ptr<Notification> notification(MakeNotification(&id1));
  notification->set_shown_as_popup(true);

  // Call PushNotification on this notification.
  notification_list()->PushNotification(std::move(notification));

  // Ensure it is still marked as shown.
  EXPECT_TRUE(GetNotification(id1)->shown_as_popup());
}

TEST_F(NotificationListTest, TestHasNotificationOfType) {
  std::string id = AddNotification();

  EXPECT_TRUE(notification_list()->HasNotificationOfType(
      id, message_center::NOTIFICATION_TYPE_SIMPLE));
  EXPECT_FALSE(notification_list()->HasNotificationOfType(
      id, message_center::NOTIFICATION_TYPE_PROGRESS));

  std::unique_ptr<Notification> updated_notification(new Notification(
      message_center::NOTIFICATION_TYPE_PROGRESS, id, UTF8ToUTF16("updated"),
      UTF8ToUTF16("updated"), gfx::Image(), base::string16(), GURL(),
      NotifierId(), RichNotificationData(), NULL));
  notification_list()->AddNotification(std::move(updated_notification));

  EXPECT_FALSE(notification_list()->HasNotificationOfType(
      id, message_center::NOTIFICATION_TYPE_SIMPLE));
  EXPECT_TRUE(notification_list()->HasNotificationOfType(
      id, message_center::NOTIFICATION_TYPE_PROGRESS));
}

}  // namespace message_center
