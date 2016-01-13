// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_button_bar.h"

#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"

namespace message_center {

namespace {
const int kButtonSize = 40;
const int kChevronWidth = 8;
const int kFooterTopMargin = 6;
const int kFooterBottomMargin = 3;
const int kFooterLeftMargin = 20;
const int kFooterRightMargin = 14;
}  // namespace

// NotificationCenterButton ////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
class NotificationCenterButton : public views::ToggleImageButton {
 public:
  NotificationCenterButton(views::ButtonListener* listener,
                           int normal_id,
                           int hover_id,
                           int pressed_id,
                           int text_id);
  void set_size(gfx::Size size) { size_ = size; }

 protected:
  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(NotificationCenterButton);
};

NotificationCenterButton::NotificationCenterButton(
    views::ButtonListener* listener,
    int normal_id,
    int hover_id,
    int pressed_id,
    int text_id)
    : views::ToggleImageButton(listener), size_(kButtonSize, kButtonSize) {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  SetImage(STATE_NORMAL, resource_bundle.GetImageSkiaNamed(normal_id));
  SetImage(STATE_HOVERED, resource_bundle.GetImageSkiaNamed(hover_id));
  SetImage(STATE_PRESSED, resource_bundle.GetImageSkiaNamed(pressed_id));
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  if (text_id)
    SetTooltipText(resource_bundle.GetLocalizedString(text_id));

  SetFocusable(true);
  set_request_focus_on_press(false);

  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor,
      gfx::Insets(1, 2, 2, 2)));
}

gfx::Size NotificationCenterButton::GetPreferredSize() const { return size_; }

// MessageCenterButtonBar /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MessageCenterButtonBar::MessageCenterButtonBar(
    MessageCenterView* message_center_view,
    MessageCenter* message_center,
    NotifierSettingsProvider* notifier_settings_provider,
    bool settings_initially_visible,
    const base::string16& title)
    : message_center_view_(message_center_view),
      message_center_(message_center),
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
      close_bubble_button_(NULL),
#endif
      title_arrow_(NULL),
      notification_label_(NULL),
      button_container_(NULL),
      close_all_button_(NULL),
      settings_button_(NULL),
      quiet_mode_button_(NULL) {
  SetPaintToLayer(true);
  set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));

  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();

  title_arrow_ = new NotificationCenterButton(this,
                                              IDR_NOTIFICATION_ARROW,
                                              IDR_NOTIFICATION_ARROW_HOVER,
                                              IDR_NOTIFICATION_ARROW_PRESSED,
                                              0);
  title_arrow_->set_size(gfx::Size(kChevronWidth, kButtonSize));

  // Keyboardists can use the gear button to switch modes.
  title_arrow_->SetFocusable(false);
  AddChildView(title_arrow_);

  notification_label_ = new views::Label(title);
  notification_label_->SetAutoColorReadabilityEnabled(false);
  notification_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  notification_label_->SetEnabledColor(kRegularTextColor);
  AddChildView(notification_label_);

  button_container_ = new views::View;
  button_container_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  quiet_mode_button_ = new NotificationCenterButton(
      this,
      IDR_NOTIFICATION_DO_NOT_DISTURB,
      IDR_NOTIFICATION_DO_NOT_DISTURB_HOVER,
      IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED,
      IDS_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP);
  quiet_mode_button_->SetToggledImage(
      views::Button::STATE_NORMAL,
      resource_bundle.GetImageSkiaNamed(
          IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED));
  quiet_mode_button_->SetToggledImage(
      views::Button::STATE_HOVERED,
      resource_bundle.GetImageSkiaNamed(
          IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED));
  quiet_mode_button_->SetToggledImage(
      views::Button::STATE_PRESSED,
      resource_bundle.GetImageSkiaNamed(
          IDR_NOTIFICATION_DO_NOT_DISTURB_PRESSED));
  quiet_mode_button_->SetToggled(message_center->IsQuietMode());
  button_container_->AddChildView(quiet_mode_button_);

  close_all_button_ =
      new NotificationCenterButton(this,
                                   IDR_NOTIFICATION_CLEAR_ALL,
                                   IDR_NOTIFICATION_CLEAR_ALL_HOVER,
                                   IDR_NOTIFICATION_CLEAR_ALL_PRESSED,
                                   IDS_MESSAGE_CENTER_CLEAR_ALL);
  close_all_button_->SetImage(
      views::Button::STATE_DISABLED,
      resource_bundle.GetImageSkiaNamed(IDR_NOTIFICATION_CLEAR_ALL_DISABLED));
  button_container_->AddChildView(close_all_button_);

  settings_button_ =
      new NotificationCenterButton(this,
                                   IDR_NOTIFICATION_SETTINGS,
                                   IDR_NOTIFICATION_SETTINGS_HOVER,
                                   IDR_NOTIFICATION_SETTINGS_PRESSED,
                                   IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL);
  button_container_->AddChildView(settings_button_);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  close_bubble_button_ = new views::ImageButton(this);
  close_bubble_button_->SetImage(
      views::Button::STATE_NORMAL,
      resource_bundle.GetImageSkiaNamed(IDR_NOTIFICATION_BUBBLE_CLOSE));
  close_bubble_button_->SetImage(
      views::Button::STATE_HOVERED,
      resource_bundle.GetImageSkiaNamed(IDR_NOTIFICATION_BUBBLE_CLOSE_HOVER));
  close_bubble_button_->SetImage(
      views::Button::STATE_PRESSED,
      resource_bundle.GetImageSkiaNamed(IDR_NOTIFICATION_BUBBLE_CLOSE_PRESSED));
  AddChildView(close_bubble_button_);
#endif

  SetCloseAllButtonEnabled(!settings_initially_visible);
  SetBackArrowVisible(settings_initially_visible);
  ViewVisibilityChanged();
}

void MessageCenterButtonBar::ViewVisibilityChanged() {
  gfx::ImageSkia* settings_image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_NOTIFICATION_SETTINGS);
  int image_margin = std::max(0, (kButtonSize - settings_image->width()) / 2);
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  layout->SetInsets(kFooterTopMargin,
                    kFooterLeftMargin,
                    kFooterBottomMargin,
                    std::max(0, kFooterRightMargin - image_margin));
  views::ColumnSet* column = layout->AddColumnSet(0);
  if (title_arrow_->visible()) {
    // Column for the left-arrow used to back out of settings.
    column->AddColumn(views::GridLayout::LEADING,
                      views::GridLayout::CENTER,
                      0.0f,
                      views::GridLayout::USE_PREF,
                      0,
                      0);

    column->AddPaddingColumn(0.0f, 10);
  }

  // Column for the label "Notifications".
  column->AddColumn(views::GridLayout::LEADING,
                    views::GridLayout::CENTER,
                    0.0f,
                    views::GridLayout::USE_PREF,
                    0,
                    0);

  // Fills in the remaining space between "Notifications" and buttons.
  column->AddPaddingColumn(1.0f, image_margin);

  // The button area column.
  column->AddColumn(views::GridLayout::LEADING,
                    views::GridLayout::CENTER,
                    0.0f,
                    views::GridLayout::USE_PREF,
                    0,
                    0);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // The close-bubble button.
  column->AddColumn(views::GridLayout::LEADING,
                    views::GridLayout::LEADING,
                    0.0f,
                    views::GridLayout::USE_PREF,
                    0,
                    0);
#endif

  layout->StartRow(0, 0);
  if (title_arrow_->visible())
    layout->AddView(title_arrow_);
  layout->AddView(notification_label_);
  layout->AddView(button_container_);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  layout->AddView(close_bubble_button_);
#endif
}

MessageCenterButtonBar::~MessageCenterButtonBar() {}

void MessageCenterButtonBar::SetAllButtonsEnabled(bool enabled) {
  if (close_all_button_)
    close_all_button_->SetEnabled(enabled);
  settings_button_->SetEnabled(enabled);
  quiet_mode_button_->SetEnabled(enabled);
}

void MessageCenterButtonBar::SetCloseAllButtonEnabled(bool enabled) {
  if (close_all_button_)
    close_all_button_->SetEnabled(enabled);
}

void MessageCenterButtonBar::SetBackArrowVisible(bool visible) {
  if (title_arrow_)
    title_arrow_->SetVisible(visible);
  ViewVisibilityChanged();
  Layout();
}

void MessageCenterButtonBar::ChildVisibilityChanged(views::View* child) {
  InvalidateLayout();
}

void MessageCenterButtonBar::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == close_all_button_) {
    message_center_view()->ClearAllNotifications();
  } else if (sender == settings_button_ || sender == title_arrow_) {
    MessageCenterView* center_view = message_center_view();
    center_view->SetSettingsVisible(!center_view->settings_visible());
  } else if (sender == quiet_mode_button_) {
    if (message_center()->IsQuietMode())
      message_center()->SetQuietMode(false);
    else
      message_center()->EnterQuietModeWithExpire(base::TimeDelta::FromDays(1));
    quiet_mode_button_->SetToggled(message_center()->IsQuietMode());
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  } else if (sender == close_bubble_button_) {
    message_center_view()->tray()->HideMessageCenterBubble();
#endif
  } else {
    NOTREACHED();
  }
}

}  // namespace message_center
