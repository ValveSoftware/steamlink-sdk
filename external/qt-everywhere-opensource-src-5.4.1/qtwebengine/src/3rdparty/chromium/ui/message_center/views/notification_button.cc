// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_button.h"

#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace message_center {

NotificationButton::NotificationButton(views::ButtonListener* listener)
    : views::CustomButton(listener),
      icon_(NULL),
      title_(NULL),
      focus_painter_(views::Painter::CreateSolidFocusPainter(
          message_center::kFocusBorderColor,
          gfx::Insets(1, 2, 2, 2))) {
  SetFocusable(true);
  set_request_focus_on_press(false);
  set_notify_enter_exit_on_child(true);
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           message_center::kButtonHorizontalPadding,
                           kButtonVecticalPadding,
                           message_center::kButtonIconToTitlePadding));
}

NotificationButton::~NotificationButton() {
}

void NotificationButton::SetIcon(const gfx::ImageSkia& image) {
  if (icon_ != NULL)
    delete icon_;  // This removes the icon from this view's children.
  if (image.isNull()) {
    icon_ = NULL;
  } else {
    icon_ = new views::ImageView();
    icon_->SetImageSize(gfx::Size(message_center::kNotificationButtonIconSize,
                                  message_center::kNotificationButtonIconSize));
    icon_->SetImage(image);
    icon_->SetHorizontalAlignment(views::ImageView::LEADING);
    icon_->SetVerticalAlignment(views::ImageView::LEADING);
    icon_->SetBorder(views::Border::CreateEmptyBorder(
        message_center::kButtonIconTopPadding, 0, 0, 0));
    AddChildViewAt(icon_, 0);
  }
}

void NotificationButton::SetTitle(const base::string16& title) {
  if (title_ != NULL)
    delete title_;  // This removes the title from this view's children.
  if (title.empty()) {
    title_ = NULL;
  } else {
    title_ = new views::Label(title);
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetEnabledColor(message_center::kRegularTextColor);
    title_->SetBackgroundColor(kRegularTextBackgroundColor);
    title_->SetBorder(
        views::Border::CreateEmptyBorder(kButtonTitleTopPadding, 0, 0, 0));
    AddChildView(title_);
  }
  SetAccessibleName(title);
}

gfx::Size NotificationButton::GetPreferredSize() const {
  return gfx::Size(message_center::kNotificationWidth,
                   message_center::kButtonHeight);
}

int NotificationButton::GetHeightForWidth(int width) const {
  return message_center::kButtonHeight;
}

void NotificationButton::OnPaint(gfx::Canvas* canvas) {
  CustomButton::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void NotificationButton::OnFocus() {
  views::CustomButton::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
  // We render differently when focused.
  SchedulePaint();
 }

void NotificationButton::OnBlur() {
  views::CustomButton::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

void NotificationButton::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // We disable view hierarchy change detection in the parent
  // because it resets the hoverstate, which we do not want
  // when we update the view to contain a new label or image.
  views::View::ViewHierarchyChanged(details);
}

void NotificationButton::StateChanged() {
  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    set_background(views::Background::CreateSolidBackground(
        message_center::kHoveredButtonBackgroundColor));
  } else {
    set_background(NULL);
  }
}

}  // namespace message_center
