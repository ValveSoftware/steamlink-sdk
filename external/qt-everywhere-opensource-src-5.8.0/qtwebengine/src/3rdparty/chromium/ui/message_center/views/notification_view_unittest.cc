// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/constants.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/message_center/views/notification_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

std::unique_ptr<ui::GestureEvent> GenerateGestureEvent(ui::EventType type) {
  ui::GestureEventDetails detail(type);
  std::unique_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(0, 0, 0, base::TimeTicks(), detail));
  return event;
}

std::unique_ptr<ui::GestureEvent> GenerateGestureVerticalScrollUpdateEvent(
    int dx) {
  ui::GestureEventDetails detail(ui::ET_GESTURE_SCROLL_UPDATE, dx, 0);
  std::unique_ptr<ui::GestureEvent> event(
      new ui::GestureEvent(0, 0, 0, base::TimeTicks(), detail));
  return event;
}

}  // anonymouse namespace

namespace message_center {

// A test delegate used for tests that deal with the notification settings
// button.
class NotificationSettingsDelegate : public NotificationDelegate {
  bool ShouldDisplaySettingsButton() override { return true; }

 private:
  ~NotificationSettingsDelegate() override {}
};

/* Test fixture ***************************************************************/

class NotificationViewTest : public views::ViewsTestBase,
                             public MessageCenterController {
 public:
  NotificationViewTest();
  ~NotificationViewTest() override;

  void SetUp() override;
  void TearDown() override;

  views::Widget* widget() { return notification_view_->GetWidget(); }
  NotificationView* notification_view() const {
    return notification_view_.get();
  }
  Notification* notification() { return notification_.get(); }
  RichNotificationData* data() { return data_.get(); }

  // Overridden from MessageCenterController:
  void ClickOnNotification(const std::string& notification_id) override;
  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override;
  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override;
  bool HasClickedListener(const std::string& notification_id) override;
  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override;
  void ClickOnSettingsButton(const std::string& notification_id) override;

 protected:
  // Used to fill bitmaps returned by CreateBitmap().
  static const SkColor kBitmapColor = SK_ColorGREEN;

  const gfx::Image CreateTestImage(int width, int height) {
    return gfx::Image::CreateFrom1xBitmap(CreateBitmap(width, height));
  }

  const SkBitmap CreateBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(kBitmapColor);
    return bitmap;
  }

  std::vector<ButtonInfo> CreateButtons(int number) {
    ButtonInfo info(base::ASCIIToUTF16("Test button title."));
    info.icon = CreateTestImage(4, 4);
    return std::vector<ButtonInfo>(number, info);
  }

  // Paints |view| and returns the size that the original image (which must have
  // been created by CreateBitmap()) was scaled to.
  gfx::Size GetImagePaintSize(ProportionalImageView* view) {
    CHECK(view);
    if (view->bounds().IsEmpty())
      return gfx::Size();

    gfx::Size canvas_size = view->bounds().size();
    gfx::Canvas canvas(canvas_size, 1.0 /* image_scale */,
                       true /* is_opaque */);
    static_assert(kBitmapColor != SK_ColorBLACK,
                  "The bitmap color must match the background color");
    canvas.DrawColor(SK_ColorBLACK);
    view->OnPaint(&canvas);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(canvas_size.width(), canvas_size.height());
    canvas.sk_canvas()->readPixels(&bitmap, 0, 0);

    // Incrementally inset each edge at its midpoint to find the bounds of the
    // rect containing the image's color. This assumes that the image is
    // centered in the canvas.
    const int kHalfWidth = canvas_size.width() / 2;
    const int kHalfHeight = canvas_size.height() / 2;
    gfx::Rect rect(canvas_size);
    while (rect.width() > 0 &&
           bitmap.getColor(rect.x(), kHalfHeight) != kBitmapColor)
      rect.Inset(1, 0, 0, 0);
    while (rect.height() > 0 &&
           bitmap.getColor(kHalfWidth, rect.y()) != kBitmapColor)
      rect.Inset(0, 1, 0, 0);
    while (rect.width() > 0 &&
           bitmap.getColor(rect.right() - 1, kHalfHeight) != kBitmapColor)
      rect.Inset(0, 0, 1, 0);
    while (rect.height() > 0 &&
           bitmap.getColor(kHalfWidth, rect.bottom() - 1) != kBitmapColor)
      rect.Inset(0, 0, 0, 1);

    return rect.size();
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

  views::ImageButton* GetCloseButton() {
    return notification_view()->close_button();
  }

  void UpdateNotificationViews() {
    notification_view()->UpdateWithNotification(*notification());
  }

  float GetNotificationScrollAmount() const {
    return notification_view()->GetTransform().To2dTranslation().x();
  }

  bool IsRemoved(const std::string& notification_id) const {
    return (removed_ids_.find(notification_id) != removed_ids_.end());
  }

  void RemoveNotificationView() { notification_view_.reset(); }

 private:
  std::set<std::string> removed_ids_;

  std::unique_ptr<RichNotificationData> data_;
  std::unique_ptr<Notification> notification_;
  std::unique_ptr<NotificationView> notification_view_;

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
  notification_.reset(new Notification(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string("notification id"),
      base::UTF8ToUTF16("title"), base::UTF8ToUTF16("message"),
      CreateTestImage(80, 80), base::UTF8ToUTF16("display source"), GURL(),
      NotifierId(NotifierId::APPLICATION, "extension_id"), *data_,
      NULL));
  notification_->set_small_image(CreateTestImage(16, 16));
  notification_->set_image(CreateTestImage(320, 240));

  // Then create a new NotificationView with that single notification.
  notification_view_.reset(static_cast<NotificationView*>(
      MessageViewFactory::Create(this, *notification_, true)));
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
  removed_ids_.insert(notification_id);
}

std::unique_ptr<ui::MenuModel> NotificationViewTest::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  // For this test, this method should not be invoked.
  NOTREACHED();
  return nullptr;
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

void NotificationViewTest::ClickOnSettingsButton(
    const std::string& notification_id) {
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
  EXPECT_TRUE(NULL == notification_view()->settings_button_view_);
  // We still expect an icon view for all layouts.
  EXPECT_TRUE(NULL != notification_view()->icon_view_);
}

TEST_F(NotificationViewTest, CreateOrUpdateTestSettingsButton) {
  scoped_refptr<NotificationSettingsDelegate> delegate =
      new NotificationSettingsDelegate();
  Notification notf(NOTIFICATION_TYPE_BASE_FORMAT,
                    std::string("notification id"), base::UTF8ToUTF16("title"),
                    base::UTF8ToUTF16("message"), CreateTestImage(80, 80),
                    base::UTF8ToUTF16("display source"),
                    GURL("https://hello.com"),
                    NotifierId(NotifierId::APPLICATION, "extension_id"),
                    *data(), delegate.get());

  notification_view()->CreateOrUpdateViews(notf);
  EXPECT_TRUE(NULL != notification_view()->title_view_);
  EXPECT_TRUE(NULL != notification_view()->message_view_);
  EXPECT_TRUE(NULL != notification_view()->context_message_view_);
  EXPECT_TRUE(NULL != notification_view()->settings_button_view_);
  EXPECT_TRUE(NULL != notification_view()->icon_view_);

  EXPECT_TRUE(NULL == notification_view()->image_view_);
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

  notification()->set_context_message(base::ASCIIToUTF16("foo"));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_TRUE(notification_view()->context_message_view_ != NULL);

  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(0, 360));
  EXPECT_EQ(1, notification_view()->GetMessageLineLimit(1, 360));
  EXPECT_EQ(0, notification_view()->GetMessageLineLimit(2, 360));
}

TEST_F(NotificationViewTest, TestIconSizing) {
  notification()->set_type(NOTIFICATION_TYPE_SIMPLE);
  ProportionalImageView* view = notification_view()->icon_view_;

  // Icons smaller than the maximum size should remain unscaled.
  notification()->set_icon(CreateTestImage(kNotificationIconSize / 2,
                                           kNotificationIconSize / 4));
  UpdateNotificationViews();
  EXPECT_EQ(gfx::Size(kNotificationIconSize / 2,
                      kNotificationIconSize / 4).ToString(),
            GetImagePaintSize(view).ToString());

  // Icons of exactly the intended icon size should remain unscaled.
  notification()->set_icon(CreateTestImage(kNotificationIconSize,
                                           kNotificationIconSize));
  UpdateNotificationViews();
  EXPECT_EQ(gfx::Size(kNotificationIconSize, kNotificationIconSize).ToString(),
            GetImagePaintSize(view).ToString());

  // Icons over the maximum size should be scaled down, maintaining proportions.
  notification()->set_icon(CreateTestImage(2 * kNotificationIconSize,
                                           2 * kNotificationIconSize));
  UpdateNotificationViews();
  EXPECT_EQ(gfx::Size(kNotificationIconSize, kNotificationIconSize).ToString(),
            GetImagePaintSize(view).ToString());

  notification()->set_icon(CreateTestImage(4 * kNotificationIconSize,
                                           2 * kNotificationIconSize));
  UpdateNotificationViews();
  EXPECT_EQ(gfx::Size(kNotificationIconSize,
                      kNotificationIconSize / 2).ToString(),
            GetImagePaintSize(view).ToString());
}

TEST_F(NotificationViewTest, TestImageSizing) {
  ProportionalImageView* view = notification_view()->image_view_;
  const gfx::Size kIdealSize(kNotificationPreferredImageWidth,
                             kNotificationPreferredImageHeight);

  // Images should be scaled to the ideal size.
  notification()->set_image(CreateTestImage(kIdealSize.width() / 2,
                                            kIdealSize.height() / 2));
  UpdateNotificationViews();
  EXPECT_EQ(kIdealSize.ToString(), GetImagePaintSize(view).ToString());

  notification()->set_image(CreateTestImage(kIdealSize.width(),
                                            kIdealSize.height()));
  UpdateNotificationViews();
  EXPECT_EQ(kIdealSize.ToString(), GetImagePaintSize(view).ToString());

  notification()->set_image(CreateTestImage(kIdealSize.width() * 2,
                                            kIdealSize.height() * 2));
  UpdateNotificationViews();
  EXPECT_EQ(kIdealSize.ToString(), GetImagePaintSize(view).ToString());

  // Original aspect ratios should be preserved.
  gfx::Size orig_size(kIdealSize.width() * 2, kIdealSize.height());
  notification()->set_image(
      CreateTestImage(orig_size.width(), orig_size.height()));
  UpdateNotificationViews();
  gfx::Size paint_size = GetImagePaintSize(view);
  gfx::Size container_size = kIdealSize;
  container_size.Enlarge(-2 * kNotificationImageBorderSize,
                         -2 * kNotificationImageBorderSize);
  EXPECT_EQ(GetImageSizeForContainerSize(container_size, orig_size).ToString(),
            paint_size.ToString());
  ASSERT_GT(paint_size.height(), 0);
  EXPECT_EQ(orig_size.width() / orig_size.height(),
            paint_size.width() / paint_size.height());

  orig_size.SetSize(kIdealSize.width(), kIdealSize.height() * 2);
  notification()->set_image(
      CreateTestImage(orig_size.width(), orig_size.height()));
  UpdateNotificationViews();
  paint_size = GetImagePaintSize(view);
  EXPECT_EQ(GetImageSizeForContainerSize(container_size, orig_size).ToString(),
            paint_size.ToString());
  ASSERT_GT(paint_size.height(), 0);
  EXPECT_EQ(orig_size.width() / orig_size.height(),
            paint_size.width() / paint_size.height());
}

TEST_F(NotificationViewTest, UpdateButtonsStateTest) {
  notification()->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification());
  widget()->Show();

  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(notification_view()->action_buttons_[0],
                                    &cursor_location);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_EQ(views::CustomButton::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());

  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_EQ(views::CustomButton::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  move = ui::MouseEvent(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());
}

TEST_F(NotificationViewTest, UpdateButtonCountTest) {
  notification()->set_buttons(CreateButtons(2));
  notification_view()->CreateOrUpdateViews(*notification());
  widget()->Show();

  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());
  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->action_buttons_[1]->state());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->action_buttons_[0],
                                    &cursor_location);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  ui::EventDispatchDetails details =
      views::test::WidgetTest::GetEventProcessor(widget())->
          OnEventFromSource(&move);
  EXPECT_FALSE(details.dispatcher_destroyed);

  EXPECT_EQ(views::CustomButton::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());
  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->action_buttons_[1]->state());

  notification()->set_buttons(CreateButtons(1));
  notification_view()->CreateOrUpdateViews(*notification());

  EXPECT_EQ(views::CustomButton::STATE_HOVERED,
            notification_view()->action_buttons_[0]->state());
  EXPECT_EQ(1u, notification_view()->action_buttons_.size());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  move = ui::MouseEvent(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->action_buttons_[0]->state());
}

TEST_F(NotificationViewTest, SettingsButtonTest) {
  scoped_refptr<NotificationSettingsDelegate> delegate =
      new NotificationSettingsDelegate();
  Notification notf(NOTIFICATION_TYPE_BASE_FORMAT,
                    std::string("notification id"), base::UTF8ToUTF16("title"),
                    base::UTF8ToUTF16("message"), CreateTestImage(80, 80),
                    base::UTF8ToUTF16("display source"),
                    GURL("https://hello.com"),
                    NotifierId(NotifierId::APPLICATION, "extension_id"),
                    *data(), delegate.get());
  notification_view()->CreateOrUpdateViews(notf);
  widget()->Show();
  notification_view()->Layout();

  EXPECT_TRUE(NULL != notification_view()->settings_button_view_);
  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->settings_button_view_->state());

  // Now construct a mouse move event 1 pixel inside the boundary of the action
  // button.
  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToScreen(notification_view()->settings_button_view_,
                                    &cursor_location);
  ui::MouseEvent move(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                      ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);
  ui::EventDispatchDetails details =
      views::test::WidgetTest::GetEventProcessor(widget())
          ->OnEventFromSource(&move);
  EXPECT_FALSE(details.dispatcher_destroyed);

  EXPECT_EQ(views::CustomButton::STATE_HOVERED,
            notification_view()->settings_button_view_->state());

  // Now construct a mouse move event 1 pixel outside the boundary of the
  // widget.
  cursor_location = gfx::Point(-1, -1);
  move = ui::MouseEvent(ui::ET_MOUSE_MOVED, cursor_location, cursor_location,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);
  widget()->OnMouseEvent(&move);

  EXPECT_EQ(views::CustomButton::STATE_NORMAL,
            notification_view()->settings_button_view_->state());
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

TEST_F(NotificationViewTest, FormatContextMessageTest) {
  const std::string kRegularContextText = "Context Text";
  const std::string kVeryLongContextText =
      "VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY"
      "VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY"
      "VERY VERY VERY VERY Long Long Long Long Long Long Long Long context";

  const std::string kVeryLongElidedContextText =
      "VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY VERY VERYVERY VERY "
      "VERY VERY VERY VERY VERY VERY VERY VERY VERY\xE2\x80\xA6";

  const std::string kChromeUrl = "chrome://settings";
  const std::string kUrlContext = "http://chromium.org/hello";
  const std::string kHostContext = "chromium.org";
  const std::string kLongUrlContext =
      "https://"
      "veryveryveryveryveyryveryveryveryveryveyryveryvery.veryveryveyrylong."
      "chromium.org/hello";

  Notification notification1(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(""), base::UTF8ToUTF16(""),
      base::UTF8ToUTF16(""), CreateTestImage(80, 80), base::UTF8ToUTF16(""),
      GURL(), message_center::NotifierId(GURL()), *data(), NULL);
  notification1.set_context_message(base::ASCIIToUTF16(kRegularContextText));

  base::string16 result =
      notification_view()->FormatContextMessage(notification1);
  EXPECT_EQ(kRegularContextText, base::UTF16ToUTF8(result));

  notification1.set_context_message(base::ASCIIToUTF16(kVeryLongContextText));
  result = notification_view()->FormatContextMessage(notification1);
  EXPECT_EQ(kVeryLongElidedContextText, base::UTF16ToUTF8(result));

  Notification notification2(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(""), base::UTF8ToUTF16(""),
      base::UTF8ToUTF16(""), CreateTestImage(80, 80), base::UTF8ToUTF16(""),
      GURL(kUrlContext), message_center::NotifierId(GURL()), *data(), NULL);
  notification2.set_context_message(base::ASCIIToUTF16(""));

  result = notification_view()->FormatContextMessage(notification2);
  EXPECT_EQ(kHostContext, base::UTF16ToUTF8(result));

  // Non http url and empty context message should yield an empty context
  // message.
  Notification notification3(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(""), base::UTF8ToUTF16(""),
      base::UTF8ToUTF16(""), CreateTestImage(80, 80), base::UTF8ToUTF16(""),
      GURL(kChromeUrl), message_center::NotifierId(GURL()), *data(), NULL);
  notification3.set_context_message(base::ASCIIToUTF16(""));
  result = notification_view()->FormatContextMessage(notification3);
  EXPECT_TRUE(result.empty());

  // Long http url should be elided
  Notification notification4(
      NOTIFICATION_TYPE_BASE_FORMAT, std::string(""), base::UTF8ToUTF16(""),
      base::UTF8ToUTF16(""), CreateTestImage(80, 80), base::UTF8ToUTF16(""),
      GURL(kLongUrlContext), message_center::NotifierId(GURL()), *data(), NULL);
  notification4.set_context_message(base::ASCIIToUTF16(""));
  result = notification_view()->FormatContextMessage(notification4);

  // Different platforms elide at different lengths so we do
  // some generic checking here.
  // The url has been elided (it starts with an ellipsis)
  // The end of the domainsuffix is shown
  // the url piece is not shown
  EXPECT_TRUE(base::UTF16ToUTF8(result).find(
                  ".veryveryveyrylong.chromium.org") != std::string::npos);
  EXPECT_TRUE(base::UTF16ToUTF8(result).find("\xE2\x80\xA6") == 0);
  EXPECT_TRUE(base::UTF16ToUTF8(result).find("hello") == std::string::npos);
}

TEST_F(NotificationViewTest, SlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  UpdateNotificationViews();
  std::string notification_id = notification()->id();

  auto event_begin = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN);
  auto event_scroll10 = GenerateGestureVerticalScrollUpdateEvent(-10);
  auto event_scroll500 = GenerateGestureVerticalScrollUpdateEvent(-500);
  auto event_end = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_END);

  notification_view()->OnGestureEvent(event_begin.get());
  notification_view()->OnGestureEvent(event_scroll10.get());
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(-10.f, GetNotificationScrollAmount());
  notification_view()->OnGestureEvent(event_end.get());
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(0.f, GetNotificationScrollAmount());

  notification_view()->OnGestureEvent(event_begin.get());
  notification_view()->OnGestureEvent(event_scroll500.get());
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_EQ(-500.f, GetNotificationScrollAmount());
  notification_view()->OnGestureEvent(event_end.get());
  EXPECT_TRUE(IsRemoved(notification_id));
}

// Pinning notification is ChromeOS only feature.
#if defined(OS_CHROMEOS)

TEST_F(NotificationViewTest, SlideOutPinned) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  notification()->set_pinned(true);
  UpdateNotificationViews();
  std::string notification_id = notification()->id();

  auto event_begin = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_BEGIN);
  auto event_scroll500 = GenerateGestureVerticalScrollUpdateEvent(-500);
  auto event_end = GenerateGestureEvent(ui::ET_GESTURE_SCROLL_END);

  notification_view()->OnGestureEvent(event_begin.get());
  notification_view()->OnGestureEvent(event_scroll500.get());
  EXPECT_FALSE(IsRemoved(notification_id));
  EXPECT_LT(-500.f, GetNotificationScrollAmount());
  notification_view()->OnGestureEvent(event_end.get());
  EXPECT_FALSE(IsRemoved(notification_id));
}

TEST_F(NotificationViewTest, Pinned) {
  notification()->set_pinned(true);

  UpdateNotificationViews();
  EXPECT_EQ(NULL, GetCloseButton());
}

#endif // defined(OS_CHROMEOS)

}  // namespace message_center
