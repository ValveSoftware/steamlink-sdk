// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

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
  virtual ~MockDelegate() {}
  virtual void OnMessageCenterTrayChanged() OVERRIDE {}
  virtual bool ShowPopups() OVERRIDE {
    return show_message_center_success_;
  }
  virtual void HidePopups() OVERRIDE {}
  virtual bool ShowMessageCenter() OVERRIDE {
    return show_popups_success_;
  }
  virtual void HideMessageCenter() OVERRIDE {}
  virtual bool ShowNotifierSettings() OVERRIDE {
    return true;
  }
  virtual bool IsContextMenuEnabled() const OVERRIDE {
    return enable_context_menu_;
  }

  virtual MessageCenterTray* GetMessageCenterTray() OVERRIDE {
    return NULL;
  }

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
  virtual ~MessageCenterTrayTest() {}

  virtual void SetUp() {
    MessageCenter::Initialize();
    delegate_.reset(new MockDelegate);
    message_center_ = MessageCenter::Get();
    message_center_tray_.reset(
        new MessageCenterTray(delegate_.get(), message_center_));
  }

  virtual void TearDown() {
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
    scoped_ptr<Notification> notification(
        new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                         id,
                         ASCIIToUTF16("Test Web Notification"),
                         ASCIIToUTF16("Notification message body."),
                         gfx::Image(),
                         ASCIIToUTF16("www.test.org"),
                         DummyNotifierId(),
                         message_center::RichNotificationData(),
                         NULL /* delegate */));
    message_center_->AddNotification(notification.Pass());
  }
  scoped_ptr<MockDelegate> delegate_;
  scoped_ptr<MessageCenterTray> message_center_tray_;
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

  message_center_tray_->ToggleMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->ToggleMessageCenterBubble();

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

  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       "MessageCenterReopnPopupsForSystemPriority",
                       ASCIIToUTF16("Test Web Notification"),
                       ASCIIToUTF16("Notification message body."),
                       gfx::Image(),
                       ASCIIToUTF16("www.test.org"),
                       DummyNotifierId(),
                       message_center::RichNotificationData(),
                       NULL /* delegate */));
  notification->SetSystemPriority();
  message_center_->AddNotification(notification.Pass());

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

  message_center_tray_->ToggleMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HidePopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, ContextMenuTest) {
  const std::string id1 = "id1";
  const std::string id2 = "id2";
  const std::string id3 = "id3";
  AddNotification(id1);

  base::string16 display_source = ASCIIToUTF16("www.test.org");
  NotifierId notifier_id = DummyNotifierId();

  NotifierId notifier_id2(NotifierId::APPLICATION, "sample-app");
  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       id2,
                       ASCIIToUTF16("Test Web Notification"),
                       ASCIIToUTF16("Notification message body."),
                       gfx::Image(),
                       base::string16() /* empty display source */,
                       notifier_id2,
                       message_center::RichNotificationData(),
                       NULL /* delegate */));
  message_center_->AddNotification(notification.Pass());

  AddNotification(id3);

  scoped_ptr<ui::MenuModel> model(
      message_center_tray_->CreateNotificationMenuModel(
          notifier_id, display_source));
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

}  // namespace message_center
