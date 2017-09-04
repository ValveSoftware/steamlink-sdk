// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

using base::ASCIIToUTF16;

namespace message_center {
namespace {

class MockDelegate : public MessageCenterTrayDelegate {
 public:
  MockDelegate()
      : show_popups_success_(true),
        show_message_center_success_(true),
        enable_context_menu_(true) {}
  ~MockDelegate() override {}
  void OnMessageCenterTrayChanged() override {}
  bool ShowPopups() override { return show_message_center_success_; }
  void HidePopups() override {}
  bool ShowMessageCenter() override { return show_popups_success_; }
  void HideMessageCenter() override {}
  bool ShowNotifierSettings() override { return true; }
  bool IsContextMenuEnabled() const override { return enable_context_menu_; }

  MessageCenterTray* GetMessageCenterTray() override { return NULL; }

  bool show_popups_success_;
  bool show_message_center_success_;
  bool enable_context_menu_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

}  // namespace

class MessageCenterTrayTest : public testing::Test {
 public:
  MessageCenterTrayTest() {}
  ~MessageCenterTrayTest() override {}

  void SetUp() override {
    MessageCenter::Initialize();
    delegate_.reset(new MockDelegate);
    message_center_ = MessageCenter::Get();
    message_center_tray_.reset(
        new MessageCenterTray(delegate_.get(), message_center_));
  }

  void TearDown() override {
    message_center_tray_.reset();
    delegate_.reset();
    message_center_ = NULL;
    MessageCenter::Shutdown();
  }

 protected:
  NotifierId DummyNotifierId() {
    return NotifierId();
  }

  void AddNotification(const std::string& id) {
    AddNotification(id, DummyNotifierId());
  }

  void EnableChangeQueue(bool enabled) {
    message_center_->EnableChangeQueueForTest(enabled);
  }

  void AddNotification(const std::string& id, NotifierId notifier_id) {
    std::unique_ptr<Notification> notification(new Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        ASCIIToUTF16("Test Web Notification"),
        ASCIIToUTF16("Notification message body."), gfx::Image(),
        ASCIIToUTF16("www.test.org"), GURL(), notifier_id,
        message_center::RichNotificationData(), NULL /* delegate */));
    message_center_->AddNotification(std::move(notification));
  }
  std::unique_ptr<MockDelegate> delegate_;
  std::unique_ptr<MessageCenterTray> message_center_tray_;
  MessageCenter* message_center_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterTrayTest);
};

TEST_F(MessageCenterTrayTest, BasicMessageCenter) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, BasicPopup) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("BasicPopup");

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HidePopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, MessageCenterClosesPopups) {
  EnableChangeQueue(false);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("MessageCenterClosesPopups");

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  // The notification is queued if it's added when message center is visible.
  AddNotification("MessageCenterClosesPopups2");

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  // There is no queued notification.
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble();
  message_center_tray_->HideMessageCenterBubble();
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

// TODO(yoshiki): Remove this test after no-change-queue mode gets stable.
TEST_F(MessageCenterTrayTest, MessageCenterClosesPopupsWithChangeQueue) {
  EnableChangeQueue(true);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("MessageCenterClosesPopups");

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  // The notification is queued if it's added when message center is visible.
  AddNotification("MessageCenterClosesPopups2");

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  // The queued notification appears as a popup.
  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble();
  message_center_tray_->HideMessageCenterBubble();
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, MessageCenterReopenPopupsForSystemPriority) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "MessageCenterReopnPopupsForSystemPriority",
      ASCIIToUTF16("Test Web Notification"),
      ASCIIToUTF16("Notification message body."), gfx::Image(),
      ASCIIToUTF16("www.test.org"), GURL(), DummyNotifierId(),
      message_center::RichNotificationData(), NULL /* delegate */));
  notification->SetSystemPriority();
  message_center_->AddNotification(std::move(notification));

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, ShowBubbleFails) {
  // Now the delegate will signal that it was unable to show a bubble.
  delegate_->show_popups_success_ = false;
  delegate_->show_message_center_success_ = false;

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("ShowBubbleFails");

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_FALSE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HidePopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

#ifdef OS_CHROMEOS
TEST_F(MessageCenterTrayTest, ContextMenuTestWithMessageCenter) {
  const std::string id1 = "id1";
  const std::string id2 = "id2";
  const std::string id3 = "id3";
  AddNotification(id1);

  base::string16 display_source = ASCIIToUTF16("www.test.org");
  NotifierId notifier_id = DummyNotifierId();

  NotifierId notifier_id2(NotifierId::APPLICATION, "sample-app");
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id2,
      ASCIIToUTF16("Test Web Notification"),
      ASCIIToUTF16("Notification message body."), gfx::Image(),
      base::string16() /* empty display source */, GURL(), notifier_id2,
      message_center::RichNotificationData(), NULL /* delegate */));
  message_center_->AddNotification(std::move(notification));

  AddNotification(id3);

  std::unique_ptr<ui::MenuModel> model(
      message_center_tray_->CreateNotificationMenuModel(notifier_id,
                                                        display_source));
  EXPECT_EQ(2, model->GetItemCount());
  const int second_command = model->GetCommandIdAt(1);

  // The second item is to open the settings.
  EXPECT_TRUE(model->IsEnabledAt(0));
  EXPECT_TRUE(model->IsEnabledAt(1));
  model->ActivatedAt(1);
  EXPECT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  // The first item is to disable notifications from the notifier id. It also
  // removes all notifications from the same notifier, i.e. id1 and id3.
  model->ActivatedAt(0);
  NotificationList::Notifications notifications =
      message_center_->GetVisibleNotifications();
  EXPECT_EQ(1u, notifications.size());
  EXPECT_EQ(id2, (*notifications.begin())->id());

  // Disables the context menu.
  delegate_->enable_context_menu_ = false;

  // id2 doesn't have the display source, so it don't have the menu item for
  // disabling notifications.
  model = message_center_tray_->CreateNotificationMenuModel(
      notifier_id2, base::string16());
  EXPECT_EQ(1, model->GetItemCount());
  EXPECT_EQ(second_command, model->GetCommandIdAt(0));

  // The command itself is disabled because delegate disables context menu.
  EXPECT_FALSE(model->IsEnabledAt(0));
}

#else
TEST_F(MessageCenterTrayTest, ContextMenuTestPopupsOnly) {
  const std::string id1 = "id1";
  const std::string id2 = "id2";
  const std::string id3 = "id3";

  base::string16 display_source = ASCIIToUTF16("https://www.test.org");
  NotifierId notifier_id(GURL("https://www.test.org"));

  AddNotification(id1, notifier_id);

  NotifierId notifier_id2(NotifierId::APPLICATION, "sample-app");
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id2,
      ASCIIToUTF16("Test Web Notification"),
      ASCIIToUTF16("Notification message body."), gfx::Image(),
      base::string16() /* empty display source */, GURL(), notifier_id2,
      message_center::RichNotificationData(), NULL /* delegate */));
  message_center_->AddNotification(std::move(notification));

  AddNotification(id3, notifier_id);

  // The dummy notifier is SYSTEM_COMPONENT so no context menu is visible.
  std::unique_ptr<ui::MenuModel> model(
      message_center_tray_->CreateNotificationMenuModel(DummyNotifierId(),
                                                        display_source));
  EXPECT_EQ(nullptr, model.get());

  model = message_center_tray_->CreateNotificationMenuModel(notifier_id,
                                                            display_source);
  EXPECT_EQ(1, model->GetItemCount());

  // The first item is to disable notifications from the notifier id. It also
  // removes all notifications from the same notifier, i.e. id1 and id3.
  EXPECT_TRUE(model->IsEnabledAt(0));
  model->ActivatedAt(0);

  NotificationList::Notifications notifications =
      message_center_->GetVisibleNotifications();
  EXPECT_EQ(1u, notifications.size());
  EXPECT_EQ(id2, (*notifications.begin())->id());

  // Disables the context menu.
  delegate_->enable_context_menu_ = false;

  // id2 doesn't have the display source, so it don't have the menu item for
  // disabling notifications.
  EXPECT_EQ(nullptr, message_center_tray_->CreateNotificationMenuModel(
                         notifier_id2, base::string16()));
}
#endif

TEST_F(MessageCenterTrayTest, DelegateDisabledContextMenu) {
  const std::string id1 = "id1";
  base::string16 display_source = ASCIIToUTF16("www.test.org");
  AddNotification(id1);
  NotifierId notifier_id(GURL("www.test.org"));

  delegate_->enable_context_menu_ = false;
  // id2 doesn't have the display source, so it don't have the menu item for
  // disabling notifications.
  std::unique_ptr<ui::MenuModel> model(
      message_center_tray_->CreateNotificationMenuModel(notifier_id,
                                                        display_source));

// The commands are disabled because delegate disables context menu.
#ifndef OS_CHROMEOS
  EXPECT_EQ(1, model->GetItemCount());
#else
  EXPECT_EQ(2, model->GetItemCount());
  EXPECT_FALSE(model->IsEnabledAt(1));
#endif

  EXPECT_FALSE(model->IsEnabledAt(0));
}

}  // namespace message_center
