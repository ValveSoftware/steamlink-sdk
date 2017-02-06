// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_bubble.h"

#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace message_center {

// ContentsView ////////////////////////////////////////////////////////////////

// Handles changes in MessageCenterView sizes.
class ContentsView : public views::View {
 public:
  explicit ContentsView(MessageCenterBubble* bubble, views::View* contents);
  ~ContentsView() override;

  // Overridden from views::View:
  int GetHeightForWidth(int width) const override;

 protected:
  // Overridden from views::View:
  void ChildPreferredSizeChanged(View* child) override;

 private:
  base::WeakPtr<MessageCenterBubble> bubble_;

  DISALLOW_COPY_AND_ASSIGN(ContentsView);
};

ContentsView::ContentsView(MessageCenterBubble* bubble, views::View* contents)
    : bubble_(bubble->AsWeakPtr()) {
  SetLayoutManager(new views::FillLayout());
  AddChildView(contents);
}

ContentsView::~ContentsView() {
}

int ContentsView::GetHeightForWidth(int width) const {
  DCHECK_EQ(1, child_count());
  int contents_width = std::max(width - GetInsets().width(), 0);
  int contents_height = child_at(0)->GetHeightForWidth(contents_width);
  return contents_height + GetInsets().height();
}

void ContentsView::ChildPreferredSizeChanged(View* child) {
  // TODO(dharcourt): Reduce the amount of updating this requires.
  if (bubble_.get())
    bubble_->bubble_view()->UpdateBubble();
}

// MessageCenterBubble /////////////////////////////////////////////////////////

MessageCenterBubble::MessageCenterBubble(MessageCenter* message_center,
                                         MessageCenterTray* tray,
                                         bool first_item_has_no_margin)
    : MessageBubbleBase(message_center, tray),
      message_center_view_(NULL),
      initially_settings_visible_(false),
      first_item_has_no_margin_(first_item_has_no_margin) {}

MessageCenterBubble::~MessageCenterBubble() {
  // Removs this from the widget observers just in case. MessageCenterBubble
  // might be destoryed without calling its Widget's Close/CloseNow.
  if (bubble_view() && bubble_view()->GetWidget())
    bubble_view()->GetWidget()->RemoveObserver(this);
}

void MessageCenterBubble::SetSettingsVisible() {
  if (message_center_view_)
    message_center_view_->SetSettingsVisible(true);
  else
    initially_settings_visible_ = true;
}

views::TrayBubbleView::InitParams MessageCenterBubble::GetInitParams(
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  views::TrayBubbleView::InitParams init_params =
      GetDefaultInitParams(anchor_alignment);
  init_params.min_width += kMarginBetweenItems * 2;
  init_params.max_width += kMarginBetweenItems * 2;
  init_params.max_height = max_height();
  init_params.can_activate = true;
  init_params.first_item_has_no_margin = first_item_has_no_margin_;
  return init_params;
}

void MessageCenterBubble::InitializeContents(
    views::TrayBubbleView* new_bubble_view) {
  set_bubble_view(new_bubble_view);
  bubble_view()->GetWidget()->AddObserver(this);
  message_center_view_ = new MessageCenterView(
      message_center(), tray(), max_height(), initially_settings_visible_,
      false); /* MessageCenterBubble should be used only on ChromeOS.
                 Message center is never shown top down in ChromeOS. */
  bubble_view()->AddChildView(new ContentsView(this, message_center_view_));
  // Resize the content of the bubble view to the given bubble size. This is
  // necessary in case of the bubble border forcing a bigger size then the
  // |new_bubble_view| actually wants. See crbug.com/169390.
  bubble_view()->Layout();
  UpdateBubbleView();
}

void MessageCenterBubble::OnBubbleViewDestroyed() {
  message_center_view_ = NULL;
}

void MessageCenterBubble::UpdateBubbleView() {
  if (!bubble_view())
    return;  // Could get called after view is closed
  const NotificationList::Notifications& notifications =
      message_center()->GetVisibleNotifications();
  message_center_view_->SetNotifications(notifications);
  bubble_view()->GetWidget()->Show();
  bubble_view()->UpdateBubble();
}

void MessageCenterBubble::OnWidgetClosing(views::Widget* widget) {
  if (bubble_view() && bubble_view()->GetWidget())
    bubble_view()->GetWidget()->RemoveObserver(this);
  if (message_center_view_)
    message_center_view_->SetIsClosing(true);
}

void MessageCenterBubble::OnMouseEnteredView() {
}

void MessageCenterBubble::OnMouseExitedView() {
}

size_t MessageCenterBubble::NumMessageViewsForTest() const {
  return message_center_view_->NumMessageViewsForTest();
}

}  // namespace message_center
