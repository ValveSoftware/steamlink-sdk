// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_

#include <vector>

#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"

namespace views {
class ProgressBar;
}

namespace message_center {

class BoundedLabel;
class MessageCenter;
class MessageCenterController;
class NotificationButton;
class NotificationView;
class PaddedButton;

// View that displays all current types of notification (web, basic, image, and
// list). Future notification types may be handled by other classes, in which
// case instances of those classes would be returned by the Create() factory
// method below.
class MESSAGE_CENTER_EXPORT NotificationView : public MessageView,
                                               public MessageViewController {
 public:
  // Creates appropriate MessageViews for notifications. Those currently are
  // always NotificationView instances but in the future
  // may be instances of other classes, with the class depending on the
  // notification type. A notification is top level if it needs to be rendered
  // outside the browser window. No custom shadows are created for top level
  // notifications on Linux with Aura.
  // |controller| may be NULL, but has to be set before the view is shown.
  static NotificationView* Create(MessageCenterController* controller,
                                  const Notification& notification,
                                  bool top_level);
  virtual ~NotificationView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void ScrollRectToVisible(const gfx::Rect& rect) OVERRIDE;
  virtual views::View* GetEventHandlerForRect(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from MessageView:
  virtual void UpdateWithNotification(
      const Notification& notification) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from MessageViewController:
  virtual void ClickOnNotification(const std::string& notification_id) OVERRIDE;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) OVERRIDE;

  void set_controller(MessageCenterController* controller) {
    controller_ = controller;
  }

 protected:
  NotificationView(MessageCenterController* controller,
                   const Notification& notification);

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, CreateOrUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, TestLineLimits);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, UpdateButtonsStateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewTest, UpdateButtonCountTest);

  friend class NotificationViewTest;

  void CreateOrUpdateViews(const Notification& notification);
  void SetAccessibleName(const Notification& notification);

  void CreateOrUpdateTitleView(const Notification& notification);
  void CreateOrUpdateMessageView(const Notification& notification);
  void CreateOrUpdateContextMessageView(const Notification& notification);
  void CreateOrUpdateProgressBarView(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);

  int GetMessageLineLimit(int title_lines, int width) const;
  int GetMessageHeight(int width, int limit) const;

  MessageCenterController* controller_;  // Weak, lives longer then views.

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;

  // Weak references to NotificationView descendants owned by their parents.
  views::View* top_view_;
  BoundedLabel* title_view_;
  BoundedLabel* message_view_;
  BoundedLabel* context_message_view_;
  std::vector<views::View*> item_views_;
  views::View* icon_view_;
  views::View* bottom_view_;
  views::View* image_view_;
  views::ProgressBar* progress_bar_view_;
  std::vector<NotificationButton*> action_buttons_;
  std::vector<views::View*> separators_;

  DISALLOW_COPY_AND_ASSIGN(NotificationView);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_H_
