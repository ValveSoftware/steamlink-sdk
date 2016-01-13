// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

using base::UTF8ToUTF16;

namespace message_center {
namespace {

class MessageCenterImplTest : public testing::Test,
                              public MessageCenterObserver {
 public:
  MessageCenterImplTest() {}

  virtual void SetUp() OVERRIDE {
    MessageCenter::Initialize();
    message_center_ = MessageCenter::Get();
    loop_.reset(new base::MessageLoop);
    run_loop_.reset(new base::RunLoop());
    closure_ = run_loop_->QuitClosure();
  }

  virtual void TearDown() OVERRIDE {
    run_loop_.reset();
    loop_.reset();
    message_center_ = NULL;
    MessageCenter::Shutdown();
  }

  MessageCenter* message_center() const { return message_center_; }
  NotifierSettingsObserver* notifier_settings_observer() const {
    return static_cast<NotifierSettingsObserver*>(message_center_impl());
  }
  MessageCenterImpl* message_center_impl() const {
    return reinterpret_cast<MessageCenterImpl*>(message_center_);
  }

  base::RunLoop* run_loop() const { return run_loop_.get(); }
  base::Closure closure() const { return closure_; }

 protected:
  Notification* CreateSimpleNotification(const std::string& id) {
    return CreateNotificationWithNotifierId(
        id,
        "app1",
        NOTIFICATION_TYPE_SIMPLE);
  }

  Notification* CreateSimpleNotificationWithNotifierId(
    const std::string& id, const std::string& notifier_id) {
    return CreateNotificationWithNotifierId(
        id,
        notifier_id,
        NOTIFICATION_TYPE_SIMPLE);
  }

  Notification* CreateNotification(const std::string& id,
                                   message_center::NotificationType type) {
    return CreateNotificationWithNotifierId(id, "app1", type);
  }

  Notification* CreateNotificationWithNotifierId(
      const std::string& id,
      const std::string& notifier_id,
      message_center::NotificationType type) {
    RichNotificationData optional_fields;
    optional_fields.buttons.push_back(ButtonInfo(UTF8ToUTF16("foo")));
    optional_fields.buttons.push_back(ButtonInfo(UTF8ToUTF16("foo")));
    return new Notification(type,
                            id,
                            UTF8ToUTF16("title"),
                            UTF8ToUTF16(id),
                            gfx::Image() /* icon */,
                            base::string16() /* display_source */,
                            NotifierId(NotifierId::APPLICATION, notifier_id),
                            optional_fields,
                            NULL);
  }


 private:
  MessageCenter* message_center_;
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterImplTest);
};

class ToggledNotificationBlocker : public NotificationBlocker {
 public:
  explicit ToggledNotificationBlocker(MessageCenter* message_center)
      : NotificationBlocker(message_center),
        notifications_enabled_(true) {}
  virtual ~ToggledNotificationBlocker() {}

  void SetNotificationsEnabled(bool enabled) {
    if (notifications_enabled_ != enabled) {
      notifications_enabled_ = enabled;
      NotifyBlockingStateChanged();
    }
  }

  // NotificationBlocker overrides:
  virtual  bool ShouldShowNotificationAsPopup(
      const message_center::NotifierId& notifier_id) const OVERRIDE {
    return notifications_enabled_;
  }

 private:
  bool notifications_enabled_;

  DISALLOW_COPY_AND_ASSIGN(ToggledNotificationBlocker);
};

class PopupNotificationBlocker : public ToggledNotificationBlocker {
 public:
  PopupNotificationBlocker(MessageCenter* message_center,
                           const NotifierId& allowed_notifier)
      : ToggledNotificationBlocker(message_center),
        allowed_notifier_(allowed_notifier) {}
  virtual ~PopupNotificationBlocker() {}

  // NotificationBlocker overrides:
  virtual bool ShouldShowNotificationAsPopup(
      const NotifierId& notifier_id) const OVERRIDE {
    return (notifier_id == allowed_notifier_) ||
        ToggledNotificationBlocker::ShouldShowNotificationAsPopup(notifier_id);
  }

 private:
  NotifierId allowed_notifier_;

  DISALLOW_COPY_AND_ASSIGN(PopupNotificationBlocker);
};

class TotalNotificationBlocker : public PopupNotificationBlocker {
 public:
  TotalNotificationBlocker(MessageCenter* message_center,
                           const NotifierId& allowed_notifier)
      : PopupNotificationBlocker(message_center, allowed_notifier) {}
  virtual ~TotalNotificationBlocker() {}

  // NotificationBlocker overrides:
  virtual bool ShouldShowNotification(
      const NotifierId& notifier_id) const OVERRIDE {
    return ShouldShowNotificationAsPopup(notifier_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TotalNotificationBlocker);
};

bool PopupNotificationsContain(
    const NotificationList::PopupNotifications& popups,
    const std::string& id) {
  for (NotificationList::PopupNotifications::const_iterator iter =
           popups.begin(); iter != popups.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

// Right now, MessageCenter::HasNotification() returns regardless of blockers.
bool NotificationsContain(
    const NotificationList::Notifications& notifications,
    const std::string& id) {
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id)
      return true;
  }
  return false;
}

}  // namespace

namespace internal {

class MockPopupTimersController : public PopupTimersController {
 public:
  MockPopupTimersController(MessageCenter* message_center,
                            base::Closure quit_closure)
      : PopupTimersController(message_center),
        timer_finished_(false),
        quit_closure_(quit_closure) {}
  virtual ~MockPopupTimersController() {}

  virtual void TimerFinished(const std::string& id) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
    timer_finished_ = true;
    last_id_ = id;
  }

  bool timer_finished() const { return timer_finished_; }
  const std::string& last_id() const { return last_id_; }

 private:
  bool timer_finished_;
  std::string last_id_;
  base::Closure quit_closure_;
};

TEST_F(MessageCenterImplTest, PopupTimersEmptyController) {
  scoped_ptr<PopupTimersController> popup_timers_controller =
      make_scoped_ptr(new PopupTimersController(message_center()));

  // Test that all functions succed without any timers created.
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  popup_timers_controller->CancelAll();
  popup_timers_controller->TimerFinished("unknown");
  popup_timers_controller->PauseTimer("unknown");
  popup_timers_controller->CancelTimer("unknown");
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  run_loop()->Run();
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerPauseTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseTimer("test");
  run_loop()->RunUntilIdle();

  EXPECT_FALSE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerCancelTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->CancelTimer("test");
  run_loop()->RunUntilIdle();

  EXPECT_FALSE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerPauseAllTimers) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseAll();
  run_loop()->RunUntilIdle();

  EXPECT_FALSE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartAllTimers) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartMultipleTimers) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(5));
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3",
                                      base::TimeDelta::FromMilliseconds(3));
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test2");
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartMultipleTimersPause) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(5));
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3",
                                      base::TimeDelta::FromMilliseconds(3));
  popup_timers_controller->PauseTimer("test2");

  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test3");
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerResetTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(5));
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3",
                                      base::TimeDelta::FromMilliseconds(3));
  popup_timers_controller->PauseTimer("test2");
  popup_timers_controller->ResetTimer("test",
                                      base::TimeDelta::FromMilliseconds(2));

  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test");
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, NotificationBlocker) {
  NotifierId notifier_id(NotifierId::APPLICATION, "app1");
  // Multiple blockers to verify the case that one blocker blocks but another
  // doesn't.
  ToggledNotificationBlocker blocker1(message_center());
  ToggledNotificationBlocker blocker2(message_center());

  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id1",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id,
      RichNotificationData(),
      NULL)));
  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id2",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id,
      RichNotificationData(),
      NULL)));
  EXPECT_EQ(2u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Block all notifications. All popups are gone and message center should be
  // hidden.
  blocker1.SetNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Updates |blocker2| state, which doesn't affect the global state.
  blocker2.SetNotificationsEnabled(false);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  blocker2.SetNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // If |blocker2| blocks, then unblocking blocker1 doesn't change the global
  // state.
  blocker2.SetNotificationsEnabled(false);
  blocker1.SetNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Unblock both blockers, which recovers the global state, but the popups
  // aren't shown.
  blocker2.SetNotificationsEnabled(true);
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
}

TEST_F(MessageCenterImplTest, NotificationsDuringBlocked) {
  NotifierId notifier_id(NotifierId::APPLICATION, "app1");
  ToggledNotificationBlocker blocker(message_center());

  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id1",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id,
      RichNotificationData(),
      NULL)));
  EXPECT_EQ(1u, message_center()->GetPopupNotifications().size());
  EXPECT_EQ(1u, message_center()->GetVisibleNotifications().size());

  // Create a notification during blocked. Still no popups.
  blocker.SetNotificationsEnabled(false);
  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id2",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id,
      RichNotificationData(),
      NULL)));
  EXPECT_TRUE(message_center()->GetPopupNotifications().empty());
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  // Unblock notifications, the id1 should appear as a popup.
  blocker.SetNotificationsEnabled(true);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());
}

// Similar to other blocker cases but this test case allows |notifier_id2| even
// in blocked.
TEST_F(MessageCenterImplTest, NotificationBlockerAllowsPopups) {
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierId::APPLICATION, "app2");
  PopupNotificationBlocker blocker(message_center(), notifier_id2);

  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id1",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id1,
      RichNotificationData(),
      NULL)));
  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id2",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id2,
      RichNotificationData(),
      NULL)));

  // "id1" is closed but "id2" is still visible as a popup.
  blocker.SetNotificationsEnabled(false);
  NotificationList::PopupNotifications popups =
      message_center()->GetPopupNotifications();
  EXPECT_EQ(1u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_EQ(2u, message_center()->GetVisibleNotifications().size());

  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id3",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id1,
      RichNotificationData(),
      NULL)));
  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id4",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id2,
      RichNotificationData(),
      NULL)));
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(2u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id4"));
  EXPECT_EQ(4u, message_center()->GetVisibleNotifications().size());

  blocker.SetNotificationsEnabled(true);
  popups = message_center()->GetPopupNotifications();
  EXPECT_EQ(3u, popups.size());
  EXPECT_TRUE(PopupNotificationsContain(popups, "id2"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id3"));
  EXPECT_TRUE(PopupNotificationsContain(popups, "id4"));
  EXPECT_EQ(4u, message_center()->GetVisibleNotifications().size());
}

// TotalNotificationBlocker suppresses showing notifications even from the list.
// This would provide the feature to 'separated' message centers per-profile for
// ChromeOS multi-login.
TEST_F(MessageCenterImplTest, TotalNotificationBlocker) {
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");
  NotifierId notifier_id2(NotifierId::APPLICATION, "app2");
  TotalNotificationBlocker blocker(message_center(), notifier_id2);

  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id1",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id1,
      RichNotificationData(),
      NULL)));
  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id2",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id2,
      RichNotificationData(),
      NULL)));

  // "id1" becomes invisible while "id2" is still visible.
  blocker.SetNotificationsEnabled(false);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  NotificationList::Notifications notifications =
      message_center()->GetVisibleNotifications();
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));

  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id3",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id1,
      RichNotificationData(),
      NULL)));
  message_center()->AddNotification(scoped_ptr<Notification>(new Notification(
      NOTIFICATION_TYPE_SIMPLE,
      "id4",
      UTF8ToUTF16("title"),
      UTF8ToUTF16("message"),
      gfx::Image() /* icon */,
      base::string16() /* display_source */,
      notifier_id2,
      RichNotificationData(),
      NULL)));
  EXPECT_EQ(2u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_FALSE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_FALSE(NotificationsContain(notifications, "id3"));
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));

  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(4u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_TRUE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  EXPECT_TRUE(NotificationsContain(notifications, "id4"));

  // RemoveAllVisibleNotifications should remove just visible notifications.
  blocker.SetNotificationsEnabled(false);
  message_center()->RemoveAllVisibleNotifications(false /* by_user */);
  EXPECT_EQ(0u, message_center()->NotificationCount());
  blocker.SetNotificationsEnabled(true);
  EXPECT_EQ(2u, message_center()->NotificationCount());
  notifications = message_center()->GetVisibleNotifications();
  EXPECT_TRUE(NotificationsContain(notifications, "id1"));
  EXPECT_FALSE(NotificationsContain(notifications, "id2"));
  EXPECT_TRUE(NotificationsContain(notifications, "id3"));
  EXPECT_FALSE(NotificationsContain(notifications, "id4"));

  // And RemoveAllNotifications should remove all.
  blocker.SetNotificationsEnabled(false);
  message_center()->RemoveAllNotifications(false /* by_user */);
  EXPECT_EQ(0u, message_center()->NotificationCount());
}

TEST_F(MessageCenterImplTest, QueueUpdatesWithCenterVisible) {
  std::string id("id1");
  std::string id2("id2");
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");

  // First, add and update a notification to ensure updates happen
  // normally.
  scoped_ptr<Notification> notification(CreateSimpleNotification(id));
  message_center()->AddNotification(notification.Pass());
  notification.reset(CreateSimpleNotification(id2));
  message_center()->UpdateNotification(id, notification.Pass());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));

  // Then open the message center.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Then update a notification; nothing should have happened.
  notification.reset(CreateSimpleNotification(id));
  message_center()->UpdateNotification(id2, notification.Pass());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));

  // Close the message center; then the update should have propagated.
  message_center()->SetVisibility(VISIBILITY_TRANSIENT);
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id2));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));
}

TEST_F(MessageCenterImplTest, ComplexQueueing) {
  std::string ids[5] = {"0", "1", "2", "3", "4p"};
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");

  scoped_ptr<Notification> notification;
  // Add some notifications
  int i = 0;
  for (; i < 3; i++) {
    notification.reset(CreateSimpleNotification(ids[i]));
    message_center()->AddNotification(notification.Pass());
  }
  for (i = 0; i < 3; i++) {
    EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[i]));
  }
  for (; i < 5; i++) {
    EXPECT_FALSE(message_center()->FindVisibleNotificationById(ids[i]));
  }

  notification.reset(CreateNotification(ids[4], NOTIFICATION_TYPE_PROGRESS));
  message_center()->AddNotification(notification.Pass());

  // Now start queueing.
  // NL: ["0", "1", "2", "4p"]
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // This should update notification "1" to have id "3".
  notification.reset(CreateSimpleNotification(ids[3]));
  message_center()->UpdateNotification(ids[1], notification.Pass());

  notification.reset(CreateSimpleNotification(ids[4]));
  message_center()->UpdateNotification(ids[4], notification.Pass());

  notification.reset(CreateNotification(ids[4], NOTIFICATION_TYPE_PROGRESS));
  message_center()->UpdateNotification(ids[4], notification.Pass());

  // This should update notification "3" to a new ID after we go TRANSIENT.
  notification.reset(CreateSimpleNotification("New id"));
  message_center()->UpdateNotification(ids[3], notification.Pass());

  // This should create a new "3", that doesn't overwrite the update to 3
  // before.
  notification.reset(CreateSimpleNotification(ids[3]));
  message_center()->AddNotification(notification.Pass());

  // The NL should still be the same: ["0", "1", "2", "4p"]
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[0]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[1]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[2]));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(ids[3]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[4]));
  EXPECT_EQ(message_center()->GetVisibleNotifications().size(), 4u);
  message_center()->SetVisibility(VISIBILITY_TRANSIENT);

  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[0]));
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(ids[1]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[2]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[3]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(ids[4]));
  EXPECT_TRUE(message_center()->FindVisibleNotificationById("New id"));
  EXPECT_EQ(message_center()->GetVisibleNotifications().size(), 5u);
}

TEST_F(MessageCenterImplTest, QueuedDirectUpdates) {
  std::string id("id1");
  std::string id2("id2");
  NotifierId notifier_id1(NotifierId::APPLICATION, "app1");

  gfx::Size original_size(0, 0);
  // Open the message center to prevent adding notifications
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);

  // Create new notification to be added to the queue; images all have the same
  // original size.
  scoped_ptr<Notification> notification(CreateSimpleNotification(id));

  // Double-check that sizes all match.
  const std::vector<ButtonInfo>& original_buttons = notification->buttons();
  ASSERT_EQ(2u, original_buttons.size());

  EXPECT_EQ(original_size, notification->icon().Size());
  EXPECT_EQ(original_size, notification->image().Size());
  EXPECT_EQ(original_size, original_buttons[0].icon.Size());
  EXPECT_EQ(original_size, original_buttons[1].icon.Size());

  message_center()->AddNotification(notification.Pass());

  // The notification should be in the queue.
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));

  // Now try setting the icon to a different size.
  gfx::Size new_size(16, 16);
  EXPECT_NE(original_size, new_size);

  gfx::Canvas canvas(new_size, 1.0f, true);
  canvas.DrawColor(SK_ColorBLUE);
  gfx::Image testImage(gfx::Image(gfx::ImageSkia(canvas.ExtractImageRep())));
  message_center()->SetNotificationIcon(id, testImage);
  message_center()->SetNotificationImage(id, testImage);
  message_center()->SetNotificationButtonIcon(id, 0, testImage);
  message_center()->SetNotificationButtonIcon(id, 1, testImage);

  // The notification should be in the queue.
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(id));

  // Close the message center; then the update should have propagated.
  message_center()->SetVisibility(VISIBILITY_TRANSIENT);
  // The notification should no longer be in the queue.
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(id));

  Notification* mc_notification =
      *(message_center()->GetVisibleNotifications().begin());
  const std::vector<ButtonInfo>& buttons = mc_notification->buttons();
  ASSERT_EQ(2u, buttons.size());

  EXPECT_EQ(new_size, mc_notification->icon().Size());
  EXPECT_EQ(new_size, mc_notification->image().Size());
  EXPECT_EQ(new_size, buttons[0].icon.Size());
  EXPECT_EQ(new_size, buttons[1].icon.Size());
}

TEST_F(MessageCenterImplTest, CachedUnreadCount) {
  message_center()->AddNotification(
      scoped_ptr<Notification>(CreateSimpleNotification("id1")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(CreateSimpleNotification("id2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(CreateSimpleNotification("id3")));
  ASSERT_EQ(3u, message_center()->UnreadNotificationCount());

  // Mark 'displayed' on all notifications by using for-loop. This shouldn't
  // recreate |notifications| inside of the loop.
  const NotificationList::Notifications& notifications =
      message_center()->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    message_center()->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  }
  EXPECT_EQ(0u, message_center()->UnreadNotificationCount());

  // Imitate the timeout, which recovers the unread count. Again, this shouldn't
  // recreate |notifications| inside of the loop.
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    message_center()->MarkSinglePopupAsShown((*iter)->id(), false);
  }
  EXPECT_EQ(3u, message_center()->UnreadNotificationCount());

  // Opening the message center will reset the unread count.
  message_center()->SetVisibility(VISIBILITY_MESSAGE_CENTER);
  EXPECT_EQ(0u, message_center()->UnreadNotificationCount());
}

TEST_F(MessageCenterImplTest, DisableNotificationsByNotifier) {
  ASSERT_EQ(0u, message_center()->NotificationCount());
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id1-1", "app1")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id1-2", "app1")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-1", "app2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-2", "app2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-3", "app2")));
  ASSERT_EQ(5u, message_center()->NotificationCount());

  // Removing all of app1's notifications should only leave app2's.
  message_center()->DisableNotificationsByNotifier(
      NotifierId(NotifierId::APPLICATION, "app1"));
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Now we remove the remaining notifications.
  message_center()->DisableNotificationsByNotifier(
      NotifierId(NotifierId::APPLICATION, "app2"));
  ASSERT_EQ(0u, message_center()->NotificationCount());
}

TEST_F(MessageCenterImplTest, NotifierEnabledChanged) {
  ASSERT_EQ(0u, message_center()->NotificationCount());
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id1-1", "app1")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id1-2", "app1")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id1-3", "app1")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-1", "app2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-2", "app2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-3", "app2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-4", "app2")));
  message_center()->AddNotification(
      scoped_ptr<Notification>(
          CreateSimpleNotificationWithNotifierId("id2-5", "app2")));
  ASSERT_EQ(8u, message_center()->NotificationCount());

  // Enabling an extension should have no effect on the count.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app1"), true);
  ASSERT_EQ(8u, message_center()->NotificationCount());

  // Removing all of app2's notifications should only leave app1's.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app2"), false);
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Removal operations should be idempotent.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app2"), false);
  ASSERT_EQ(3u, message_center()->NotificationCount());

  // Now we remove the remaining notifications.
  notifier_settings_observer()->NotifierEnabledChanged(
      NotifierId(NotifierId::APPLICATION, "app1"), false);
  ASSERT_EQ(0u, message_center()->NotificationCount());
}

}  // namespace internal
}  // namespace message_center
