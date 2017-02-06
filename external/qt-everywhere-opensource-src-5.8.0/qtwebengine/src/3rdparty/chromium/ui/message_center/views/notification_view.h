// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/view_targeter_delegate.h"

class GURL;

namespace views {
class ProgressBar;
}

namespace message_center {

class BoundedLabel;
class MessageCenter;
class NotificationButton;
class NotificationProgressBarBase;
class PaddedButton;
class ProportionalImageView;

// View that displays all current types of notification (web, basic, image, and
// list) except the custom notification. Future notification types may be
// handled by other classes, in which case instances of those classes would be
// returned by the Create() factory method below.
class MESSAGE_CENTER_EXPORT NotificationView
    : public MessageView,
      public views::ViewTargeterDelegate {
 public:
  NotificationView(MessageCenterController* controller,
                   const Notification& notification);
  ~NotificationView() override;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;
  void OnFocus() override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;

  // Overridden from MessageView:
  void UpdateWithNotification(const Notification& notification) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, CreateOrUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest,
                           CreateOrUpdateTestSettingsButton);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, FormatContextMessageTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, SettingsButtonTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestLineLimits);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestIconSizing);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestImageSizing);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, UpdateButtonsStateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, UpdateButtonCountTest);

  friend class NotificationViewTest;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

  void CreateOrUpdateViews(const Notification& notification);
  void SetAccessibleName(const Notification& notification);

  void CreateOrUpdateTitleView(const Notification& notification);
  void CreateOrUpdateMessageView(const Notification& notification);
  void CreateOrUpdateContextMessageView(const Notification& notification);
  void CreateOrUpdateSettingsButtonView(const Notification& notification);
  void CreateOrUpdateProgressBarView(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);

  int GetMessageLineLimit(int title_lines, int width) const;
  int GetMessageHeight(int width, int limit) const;

  // Formats the context message to be displayed based on |context|
  // so it shows as much information as possible
  // given the space available in the ContextMessage section of the
  // notification.
  base::string16 FormatContextMessage(const Notification& notification) const;

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;

  // Weak references to NotificationView descendants owned by their parents.
  views::View* top_view_ = nullptr;
  BoundedLabel* title_view_ = nullptr;
  BoundedLabel* message_view_ = nullptr;
  BoundedLabel* context_message_view_ = nullptr;
  views::ImageButton* settings_button_view_ = nullptr;
  std::vector<views::View*> item_views_;
  ProportionalImageView* icon_view_ = nullptr;
  views::View* bottom_view_ = nullptr;
  views::View* image_container_ = nullptr;
  ProportionalImageView* image_view_ = nullptr;
  NotificationProgressBarBase* progress_bar_view_ = nullptr;
  std::vector<NotificationButton*> action_buttons_;
  std::vector<views::View*> separators_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
