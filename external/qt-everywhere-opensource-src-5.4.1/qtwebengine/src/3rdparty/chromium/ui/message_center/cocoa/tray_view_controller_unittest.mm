// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/tray_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#include "ui/message_center/fake_notifier_settings_provider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_settings.h"

using base::ASCIIToUTF16;

namespace message_center {

class TrayViewControllerTest : public ui::CocoaTest {
 public:
  TrayViewControllerTest()
    : center_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    message_center::MessageCenter::Initialize();
    center_ = message_center::MessageCenter::Get();
    center_->DisableTimersForTest();
    tray_.reset([[MCTrayViewController alloc] initWithMessageCenter:center_]);
    [tray_ setAnimationDuration:0.002];
    [tray_ setAnimateClearingNextNotificationDelay:0.001];
    [tray_ setAnimationEndedCallback:^{
        if (nested_run_loop_.get())
          nested_run_loop_->Quit();
    }];
    [tray_ view];  // Create the view.
  }

  virtual void TearDown() OVERRIDE {
    tray_.reset();
    message_center::MessageCenter::Shutdown();
    ui::CocoaTest::TearDown();
  }

  void WaitForAnimationEnded() {
    if (![tray_ isAnimating])
      return;
    nested_run_loop_.reset(new base::RunLoop());
    nested_run_loop_->Run();
    nested_run_loop_.reset();
  }

 protected:
  message_center::NotifierId DummyNotifierId() {
    return message_center::NotifierId();
  }

  message_center::MessageCenter* center_;  // Weak, global.

  base::MessageLoopForUI message_loop_;
  scoped_ptr<base::RunLoop> nested_run_loop_;
  base::scoped_nsobject<MCTrayViewController> tray_;
};

TEST_F(TrayViewControllerTest, AddRemoveOne) {
  NSScrollView* view = [[tray_ scrollView] documentView];
  EXPECT_EQ(0u, [[view subviews] count]);
  scoped_ptr<message_center::Notification> notification_data;
  notification_data.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "1",
      ASCIIToUTF16("First notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification_data.Pass());
  [tray_ onMessageCenterTrayChanged];
  ASSERT_EQ(1u, [[view subviews] count]);

  // The view should have padding around it.
  NSView* notification = [[view subviews] objectAtIndex:0];
  NSRect notification_frame = [notification frame];
  EXPECT_CGFLOAT_EQ(2 * message_center::kMarginBetweenItems,
                    NSHeight([view frame]) - NSHeight(notification_frame));
  EXPECT_CGFLOAT_EQ(2 * message_center::kMarginBetweenItems,
                    NSWidth([view frame]) - NSWidth(notification_frame));
  EXPECT_GT(NSHeight([[tray_ view] frame]),
            NSHeight([[tray_ scrollView] frame]));

  center_->RemoveNotification("1", true);
  [tray_ onMessageCenterTrayChanged];
  EXPECT_EQ(0u, [[view subviews] count]);
  // The empty tray is now 100px tall to accommodate
  // the empty message.
  EXPECT_CGFLOAT_EQ(message_center::kMinScrollViewHeight,
                    NSHeight([view frame]));
}

TEST_F(TrayViewControllerTest, AddThreeClearAll) {
  NSScrollView* view = [[tray_ scrollView] documentView];
  EXPECT_EQ(0u, [[view subviews] count]);
  scoped_ptr<message_center::Notification> notification;
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "1",
      ASCIIToUTF16("First notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification.Pass());
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "2",
      ASCIIToUTF16("Second notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification.Pass());
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "3",
      ASCIIToUTF16("Third notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification.Pass());
  [tray_ onMessageCenterTrayChanged];
  ASSERT_EQ(3u, [[view subviews] count]);

  [tray_ clearAllNotifications:nil];
  WaitForAnimationEnded();
  [tray_ onMessageCenterTrayChanged];

  EXPECT_EQ(0u, [[view subviews] count]);
  // The empty tray is now 100px tall to accommodate
  // the empty message.
  EXPECT_CGFLOAT_EQ(message_center::kMinScrollViewHeight,
                    NSHeight([view frame]));
}

TEST_F(TrayViewControllerTest, NoClearAllWhenNoNotifications) {
  EXPECT_TRUE([tray_ pauseButton]);
  EXPECT_TRUE([tray_ clearAllButton]);

  // With no notifications, the clear all button should be hidden.
  EXPECT_TRUE([[tray_ clearAllButton] isHidden]);
  EXPECT_LT(NSMinX([[tray_ clearAllButton] frame]),
            NSMinX([[tray_ pauseButton] frame]));

  // Add a notification.
  scoped_ptr<message_center::Notification> notification;
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "1",
      ASCIIToUTF16("First notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification.Pass());
  [tray_ onMessageCenterTrayChanged];

  // Clear all should now be visible.
  EXPECT_FALSE([[tray_ clearAllButton] isHidden]);
  EXPECT_GT(NSMinX([[tray_ clearAllButton] frame]),
            NSMinX([[tray_ pauseButton] frame]));

  // Adding a second notification should keep things still visible.
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "2",
      ASCIIToUTF16("Second notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification.Pass());
  [tray_ onMessageCenterTrayChanged];
  EXPECT_FALSE([[tray_ clearAllButton] isHidden]);
  EXPECT_GT(NSMinX([[tray_ clearAllButton] frame]),
            NSMinX([[tray_ pauseButton] frame]));

  // Clear all notifications.
  [tray_ clearAllNotifications:nil];
  WaitForAnimationEnded();
  [tray_ onMessageCenterTrayChanged];

  // The button should be hidden again.
  EXPECT_TRUE([[tray_ clearAllButton] isHidden]);
  EXPECT_LT(NSMinX([[tray_ clearAllButton] frame]),
            NSMinX([[tray_ pauseButton] frame]));
}

namespace {

Notifier* NewNotifier(const std::string& id,
                      const std::string& title,
                      bool enabled) {
  NotifierId notifier_id(NotifierId::APPLICATION, id);
  return new Notifier(notifier_id, base::UTF8ToUTF16(title), enabled);
}

}  // namespace


TEST_F(TrayViewControllerTest, Settings) {
  std::vector<Notifier*> notifiers;
  notifiers.push_back(NewNotifier("id", "title", /*enabled=*/true));
  notifiers.push_back(NewNotifier("id2", "other title", /*enabled=*/false));

  FakeNotifierSettingsProvider provider(notifiers);
  center_->SetNotifierSettingsProvider(&provider);

  CGFloat trayHeight = NSHeight([[tray_ view] frame]);
  EXPECT_EQ(0, provider.closed_called_count());

  [tray_ showSettings:nil];
  EXPECT_FALSE(center_->IsMessageCenterVisible());

  // There are 0 notifications, but 2 notifiers. The settings pane should be
  // higher than the empty tray bubble.
  EXPECT_LT(trayHeight, NSHeight([[tray_ view] frame]));

  [tray_ showMessages:nil];
  EXPECT_EQ(1, provider.closed_called_count());
  EXPECT_TRUE(center_->IsMessageCenterVisible());

  // The tray should be back at its previous height now.
  EXPECT_EQ(trayHeight, NSHeight([[tray_ view] frame]));

  // Clean up since this frame owns FakeNotifierSettingsProvider.
  center_->SetNotifierSettingsProvider(NULL);
}

TEST_F(TrayViewControllerTest, EmptyCenter) {
  EXPECT_FALSE([[tray_ emptyDescription] isHidden]);

  // With no notifications, the divider should be hidden.
  EXPECT_TRUE([[tray_ divider] isHidden]);
  EXPECT_TRUE([[tray_ scrollView] isHidden]);

  scoped_ptr<message_center::Notification> notification;
  notification.reset(new message_center::Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "1",
      ASCIIToUTF16("First notification"),
      ASCIIToUTF16("This is a simple test."),
      gfx::Image(),
      base::string16(),
      DummyNotifierId(),
      message_center::RichNotificationData(),
      NULL));
  center_->AddNotification(notification.Pass());
  [tray_ onMessageCenterTrayChanged];

  EXPECT_FALSE([[tray_ divider] isHidden]);
  EXPECT_FALSE([[tray_ scrollView] isHidden]);
  EXPECT_TRUE([[tray_ emptyDescription] isHidden]);
}

}  // namespace message_center
