// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/views/custom_notification_view.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

namespace {

const SkColor kBackgroundColor = SK_ColorGREEN;

class TestCustomView : public views::View {
 public:
  TestCustomView() {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  }
  ~TestCustomView() override {}

  void Reset() {
    mouse_event_count_ = 0;
    keyboard_event_count_ = 0;
  }

  // views::View
  gfx::Size GetPreferredSize() const override { return gfx::Size(100, 100); }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    ++mouse_event_count_;
    return true;
  }
  void OnMouseMoved(const ui::MouseEvent& event) override {
    ++mouse_event_count_;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    ++mouse_event_count_;
  }
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    ++keyboard_event_count_;
    return true;
  }

  int mouse_event_count() const { return mouse_event_count_; }
  int keyboard_event_count() const { return keyboard_event_count_; }

 private:
  int mouse_event_count_ = 0;
  int keyboard_event_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestCustomView);
};

class TestNotificationDelegate : public NotificationDelegate {
 public:
  TestNotificationDelegate() {}

  // NotificateDelegate
  std::unique_ptr<views::View> CreateCustomContent() override {
    return base::WrapUnique(new TestCustomView);
  }

 private:
  ~TestNotificationDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(TestNotificationDelegate);
};

class TestMessageCenterController : public MessageCenterController {
 public:
  TestMessageCenterController() {}

  // MessageCenterController
  void ClickOnNotification(const std::string& notification_id) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override {
    removed_ids_.insert(notification_id);
  }

  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
    return nullptr;
  }

  bool HasClickedListener(const std::string& notification_id) override {
    return false;
  }

  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  void ClickOnSettingsButton(const std::string& notification_id) override {
    // For this test, this method should not be invoked.
    NOTREACHED();
  }

  bool IsRemoved(const std::string& notification_id) const {
    return (removed_ids_.find(notification_id) != removed_ids_.end());
  }

 private:
  std::set<std::string> removed_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestMessageCenterController);
};

}  // namespace

class CustomNotificationViewTest : public views::ViewsTestBase {
 public:
  CustomNotificationViewTest() {}
  ~CustomNotificationViewTest() override {}

  // views::ViewsTestBase
  void SetUp() override {
    views::ViewsTestBase::SetUp();

    notification_delegate_ = new TestNotificationDelegate;

    notification_.reset(new Notification(
        NOTIFICATION_TYPE_CUSTOM, std::string("notification id"),
        base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"), gfx::Image(),
        base::UTF8ToUTF16("display source"), GURL(),
        NotifierId(NotifierId::APPLICATION, "extension_id"),
        message_center::RichNotificationData(), notification_delegate_.get()));

    notification_view_.reset(static_cast<CustomNotificationView*>(
        MessageViewFactory::Create(controller(), *notification_, true)));
    notification_view_->set_owned_by_client();

    views::Widget::InitParams init_params(
        CreateParams(views::Widget::InitParams::TYPE_POPUP));
    views::Widget* widget = new views::Widget();
    widget->Init(init_params);
    widget->SetContentsView(notification_view_.get());
    widget->SetSize(notification_view_->GetPreferredSize());
  }

  void TearDown() override {
    widget()->Close();
    notification_view_.reset();
    views::ViewsTestBase::TearDown();
  }

  SkColor GetBackgroundColor() const {
    return notification_view_->background_view()->background()->get_color();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&released_event);
  }

  void KeyPress(ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    widget()->OnKeyEvent(&event);
  }

  views::ImageButton* close_button() {
    return notification_view_->close_button();
  }
  TestMessageCenterController* controller() { return &controller_; }
  Notification* notification() { return notification_.get(); }
  TestCustomView* custom_view() {
    return static_cast<TestCustomView*>(notification_view_->contents_view_);
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }

 private:
  TestMessageCenterController controller_;
  scoped_refptr<TestNotificationDelegate> notification_delegate_;
  std::unique_ptr<Notification> notification_;
  std::unique_ptr<CustomNotificationView> notification_view_;

  DISALLOW_COPY_AND_ASSIGN(CustomNotificationViewTest);
};

TEST_F(CustomNotificationViewTest, Background) {
  EXPECT_EQ(kBackgroundColor, GetBackgroundColor());
}

TEST_F(CustomNotificationViewTest, ClickCloseButton) {
  widget()->Show();

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(close_button(), &cursor_location);
  PerformClick(cursor_location);
  EXPECT_TRUE(controller()->IsRemoved(notification()->id()));
}

TEST_F(CustomNotificationViewTest, Events) {
  widget()->Show();
  custom_view()->RequestFocus();

  EXPECT_EQ(0, custom_view()->mouse_event_count());
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(custom_view(), &cursor_location);
  PerformClick(cursor_location);
  EXPECT_EQ(2, custom_view()->mouse_event_count());

  ui::MouseEvent move(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);
  EXPECT_EQ(3, custom_view()->mouse_event_count());

  EXPECT_EQ(0, custom_view()->keyboard_event_count());
  KeyPress(ui::VKEY_A);
  EXPECT_EQ(1, custom_view()->keyboard_event_count());
}

}  // namespace message_center
