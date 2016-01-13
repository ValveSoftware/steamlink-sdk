// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/notification_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {

/* Test fixture ***************************************************************/

class NotificationViewTest : public views::ViewsTestBase,
                             public MessageCenterController {
 public:
  NotificationViewTest();
  virtual ~NotificationViewTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  views::Widget* widget() { return notification_view_->GetWidget(); }
  NotificationView* notification_view() { return notification_view_.get(); }
  Notification* notification() { return notification_.get(); }
  RichNotificationData* data() { return data_.get(); }

  // Overridden from MessageCenterController:
  virtual void ClickOnNotification(const std::string& notification_id) OVERRIDE;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) OVERRIDE;
  virtual scoped_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) OVERRIDE;
  virtual bool HasClickedListener(const std::string& notification_id) OVERRIDE;
  virtual void ClickOnNotificationButton(const std::string& notification_id,
                                         int button_index) OVERRIDE;

 protected:
  const gfx::Image CreateTestImage(int width, int height) {
    return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height));
  }

  const SkBitmap CreateBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
    bitmap.allocPixels();
    bitmap.eraseRGB(0, 255, 0);
    return bitmap;
  }

  std::vector<ButtonInfo> CreateButtons(int number) {
    ButtonInfo info(base::ASCIIToUTF16("Test button title."));
    info.icon = CreateTestImage(4, 4);
    return std::vector<ButtonInfo>(number, info);
  }

  void CheckVerticalOrderInNotification() {
    std::vector<views::View*> vertical_order;
    vertical_order.push_back(notification_view()->top_view_);
    vertical_order.push_back(notification_view()->image_view_);
    std::copy(notification_view()->action_buttons_.begin(),
              notification_view()->action_buttons_.end(),
              std::back_inserter(vertical_order));
    std::vector<views::View*>::iterator current = vertical_order.begin();
    std::vector<views::View*>::iterator last = current++;
    while (current != vertical_order.end()) {
      gfx::Point last_point = (*last)->bounds().origin();
      views::View::ConvertPointToTarget(
          (*last), notification_view(), &last_point);

      gfx::Point current_point = (*current)->bounds().origin();
      views::View::ConvertPointToTarget(
          (*current), notification_view(), &current_point);

      EXPECT_LT(last_point.y(), current_point.y());
      last = current++;
    }
  }

  void UpdateNotificationViews() {
    notification_view()->CreateOrUpdateViews(*notification());
    notification_view()->Layout();
  }

 private:
  scoped_ptr<RichNotificationData> data_;
  scoped_ptr<Notification> notification_;
  scoped_ptr<NotificationView> notification_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationViewTest);
};

NotificationViewTest::NotificationViewTest() {
}

NotificationViewTest::~NotificationViewTest() {
}

void NotificationViewTest::SetUp() {
  views::ViewsTestBase::SetUp();
  // Create a dummy notification.
  SkBitmap bitmap;
  data_.reset(new RichNotificationData());
  notification_.reset(
      new Notification(NOTIFICATION_TYPE_BASE_FORMAT,
                       std::string("notification id"),
                       base::UTF8ToUTF16("title"),
                       base::UTF8ToUTF16("message"),
                       CreateTestImage(80, 80),
                       base::UTF8ToUTF16("display source"),
                       NotifierId(NotifierId::APPLICATION, "extension_id"),
                       *data_,
                       NULL));
  notification_->set_small_image(CreateTestImage(16, 16));
  notification_->set_image(CreateTestImage(320, 240));

  // Then create a new NotificationView with that single notification.
  notification_view_.reset(
      NotificationView::Create(this, *notification_, true));
  notification_view_->set_owned_by_client();

  views::Widget::InitParams init_params(
      CreateParams(views::Widget::InitParams::TYPE_POPUP));
  views::Widget* widget = new views::Widget();
  widget->Init(init_params);
  widget->SetContentsView(notification_view_.get());
  widget->SetSize(notification_view_->GetPreferredSize());
}

void NotificationViewTest::TearDown() {
  widget()->Close();
  notification_view_.reset();
  views::ViewsTestBase::TearDown();
}

void NotificationViewTest::ClickOnNotification(
    const std::string& notification_id) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

void NotificationViewTest::RemoveNotification(
    const std::string& notification_id,
    bool by_user) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

scoped_ptr<ui::MenuModel> NotificationViewTest::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  // For this test, this method should not be invoked.
  NOTREACHED();
  return scoped_ptr<ui::MenuModel>();
}

bool NotificationViewTest::HasClickedListener(
    const std::string& notification_id) {
  return true;
}

void NotificationViewTest::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  // For this test, this method should not be invoked.
  NOTREACHED();
}

/* Unit tests *****************************************************************/

TEST_F(NotificationViewTest, CreateOrUpdateTest) {
  EXPECT_TRUE(NULL != notification_view()->title_view_);
  EXPECT_TRUE(NULL != notification_view()->message_view_);
  EXPECT_TRUE(NULL != notification_view()->icon_view_);
  EXPECT_TRUE(NULL != notification_view()->image_view_);

  notification()->set_image(gfx::Image());
  notification()->set_title(base::ASCIIToUTF16(""));
  notification()->set_message(base::ASCIIToUTF16(""));
  notification()->set_icon(gfx::Image());

  notification_view()->CreateOrUpdateViews(*notification());
  EXPECT_TRUE(NULL == notification_view()->title_view_);
  EXPECT_TRUE(NULL == notification_view()->message_view_);
  EXPECT_TRUE(NULL == notification_view()->image_view_);
  // We still expect an icon view for all layouts.
  EXPECT_TRUE(NULL != notification_view()->icon_view_);
}

TEST_F(NotificationViewTest, TestLineLimits) {
  notification()->set_image(CreateTestImage(0, 0));
  notification()->set_context_message(base::ASCIIToUTF16(""));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_EQ(5, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(5, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(3, notification_view()->GetMessageLineLimit(2, 360));

  notification()->set_image(CreateTestImage(2, 2));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_EQ(2, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(2, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(2, 360));

  notification()->set_context_message(base::UTF8ToUTF16("foo"));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_TRUE(notification_view()->context_message_view_ != NULL);

  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(0, notification_view()->GetMessageLineLimit(2, 360));
}

TEST_F(NotificationViewTest, UpdateButtonsStateTest) {
  notification()->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification());
  widget()->Show();

  EXPECT_TRUE(NULL == notification_view()->action_buttons_[0]->background());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(notification_view()->action_buttons_[0],
                                    &cursor_location);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                      cursor_location,
                      cursor_location,
                      ui::EF_NONE,
                      ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_TRUE(NULL != notification_view()->action_buttons_[0]->background());

  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_TRUE(NULL != notification_view()->action_buttons_[0]->background());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  move = ui::MouseEvent(ui::ET_MOUSE_MOVED,
                        cursor_location,
                        cursor_location,
                        ui::EF_NONE,
                        ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_TRUE(NULL == notification_view()->action_buttons_[0]->background());
}

TEST_F(NotificationViewTest, UpdateButtonCountTest) {
  notification()->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification());
  widget()->Show();

  EXPECT_TRUE(NULL == notification_view()->action_buttons_[0]->background());
  EXPECT_TRUE(NULL == notification_view()->action_buttons_[1]->background());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(notification_view()->action_buttons_[0],
                                    &cursor_location);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED,
                      cursor_location,
                      cursor_location,
                      ui::EF_NONE,
                      ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_TRUE(NULL != notification_view()->action_buttons_[0]->background());
  EXPECT_TRUE(NULL == notification_view()->action_buttons_[1]->background());

  notification()->set_buttons(CreateButtons(1));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_TRUE(NULL != notification_view()->action_buttons_[0]->background());
  EXPECT_EQ(1u, notification_view()->action_buttons_.size());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  move = ui::MouseEvent(ui::ET_MOUSE_MOVED,
                        cursor_location,
                        cursor_location,
                        ui::EF_NONE,
                        ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_TRUE(NULL == notification_view()->action_buttons_[0]->background());
}

TEST_F(NotificationViewTest, ViewOrderingTest) {
  // Tests that views are created in the correct vertical order.
  notification()->set_buttons(CreateButtons(2));

  // Layout the initial views.
  UpdateNotificationViews();

  // Double-check that vertical order is correct.
  CheckVerticalOrderInNotification();

  // Tests that views remain in that order even after an update.
  UpdateNotificationViews();
  CheckVerticalOrderInNotification();
}

}  // namespace message_center
