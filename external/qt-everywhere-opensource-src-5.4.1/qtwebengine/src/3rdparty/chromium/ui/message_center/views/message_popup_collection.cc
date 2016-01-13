// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <set>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/views/message_view_context_menu_controller.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/toast_contents_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {
namespace {

// Timeout between the last user-initiated close of the toast and the moment
// when normal layout/update of the toast stack continues. If the last toast was
// just closed, the timeout is shorter.
const int kMouseExitedDeferTimeoutMs = 200;

// The margin between messages (and between the anchor unless
// first_item_has_no_margin was specified).
const int kToastMarginY = kMarginBetweenItems;
#if defined(OS_CHROMEOS)
const int kToastMarginX = 3;
#else
const int kToastMarginX = kMarginBetweenItems;
#endif


// If there should be no margin for the first item, this value needs to be
// substracted to flush the message to the shelf (the width of the border +
// shadow).
const int kNoToastMarginBorderAndShadowOffset = 2;

}  // namespace.

MessagePopupCollection::MessagePopupCollection(gfx::NativeView parent,
                                               MessageCenter* message_center,
                                               MessageCenterTray* tray,
                                               bool first_item_has_no_margin)
    : parent_(parent),
      message_center_(message_center),
      tray_(tray),
      display_id_(gfx::Display::kInvalidDisplayID),
      screen_(NULL),
      defer_counter_(0),
      latest_toast_entered_(NULL),
      user_is_closing_toasts_by_clicking_(false),
      first_item_has_no_margin_(first_item_has_no_margin),
      context_menu_controller_(new MessageViewContextMenuController(this)),
      weak_factory_(this) {
  DCHECK(message_center_);
  defer_timer_.reset(new base::OneShotTimer<MessagePopupCollection>);
  message_center_->AddObserver(this);
}

MessagePopupCollection::~MessagePopupCollection() {
  weak_factory_.InvalidateWeakPtrs();

  if (screen_)
    screen_->RemoveObserver(this);
  message_center_->RemoveObserver(this);

  CloseAllWidgets();
}

void MessagePopupCollection::ClickOnNotification(
    const std::string& notification_id) {
  message_center_->ClickOnNotification(notification_id);
}

void MessagePopupCollection::RemoveNotification(
    const std::string& notification_id,
    bool by_user) {
  message_center_->RemoveNotification(notification_id, by_user);
}

scoped_ptr<ui::MenuModel> MessagePopupCollection::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  return tray_->CreateNotificationMenuModel(notifier_id, display_source);
}

bool MessagePopupCollection::HasClickedListener(
    const std::string& notification_id) {
  return message_center_->HasClickedListener(notification_id);
}

void MessagePopupCollection::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  message_center_->ClickOnNotificationButton(notification_id, button_index);
}

void MessagePopupCollection::MarkAllPopupsShown() {
  std::set<std::string> closed_ids = CloseAllWidgets();
  for (std::set<std::string>::iterator iter = closed_ids.begin();
       iter != closed_ids.end(); iter++) {
    message_center_->MarkSinglePopupAsShown(*iter, false);
  }
}

void MessagePopupCollection::UpdateWidgets() {
  NotificationList::PopupNotifications popups =
      message_center_->GetPopupNotifications();

  if (popups.empty()) {
    CloseAllWidgets();
    return;
  }

  bool top_down = alignment_ & POPUP_ALIGNMENT_TOP;
  int base = GetBaseLine(toasts_.empty() ? NULL : toasts_.back());

  // Iterate in the reverse order to keep the oldest toasts on screen. Newer
  // items may be ignored if there are no room to place them.
  for (NotificationList::PopupNotifications::const_reverse_iterator iter =
           popups.rbegin(); iter != popups.rend(); ++iter) {
    if (FindToast((*iter)->id()))
      continue;

    NotificationView* view =
        NotificationView::Create(NULL,
                                 *(*iter),
                                 true); // Create top-level notification.
    view->set_context_menu_controller(context_menu_controller_.get());
    int view_height = ToastContentsView::GetToastSizeForView(view).height();
    int height_available = top_down ? work_area_.bottom() - base : base;

    if (height_available - view_height - kToastMarginY < 0) {
      delete view;
      break;
    }

    ToastContentsView* toast =
        new ToastContentsView((*iter)->id(), weak_factory_.GetWeakPtr());
    // There will be no contents already since this is a new ToastContentsView.
    toast->SetContents(view, /*a11y_feedback_for_updates=*/false);
    toasts_.push_back(toast);
    view->set_controller(toast);

    gfx::Size preferred_size = toast->GetPreferredSize();
    gfx::Point origin(GetToastOriginX(gfx::Rect(preferred_size)), base);
    // The toast slides in from the edge of the screen horizontally.
    if (alignment_ & POPUP_ALIGNMENT_LEFT)
      origin.set_x(origin.x() - preferred_size.width());
    else
      origin.set_x(origin.x() + preferred_size.width());
    if (top_down)
      origin.set_y(origin.y() + view_height);

    toast->RevealWithAnimation(origin);

    // Shift the base line to be a few pixels above the last added toast or (few
    // pixels below last added toast if top-aligned).
    if (top_down)
      base += view_height + kToastMarginY;
    else
      base -= view_height + kToastMarginY;

    if (views::ViewsDelegate::views_delegate) {
      views::ViewsDelegate::views_delegate->NotifyAccessibilityEvent(
          toast, ui::AX_EVENT_ALERT);
    }

    message_center_->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_POPUP);
  }
}

void MessagePopupCollection::OnMouseEntered(ToastContentsView* toast_entered) {
  // Sometimes we can get two MouseEntered/MouseExited in a row when animating
  // toasts.  So we need to keep track of which one is the currently active one.
  latest_toast_entered_ = toast_entered;

  message_center_->PausePopupTimers();

  if (user_is_closing_toasts_by_clicking_)
    defer_timer_->Stop();
}

void MessagePopupCollection::OnMouseExited(ToastContentsView* toast_exited) {
  // If we're exiting a toast after entering a different toast, then ignore
  // this mouse event.
  if (toast_exited != latest_toast_entered_)
    return;
  latest_toast_entered_ = NULL;

  if (user_is_closing_toasts_by_clicking_) {
    defer_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kMouseExitedDeferTimeoutMs),
        this,
        &MessagePopupCollection::OnDeferTimerExpired);
  } else {
    message_center_->RestartPopupTimers();
  }
}

std::set<std::string> MessagePopupCollection::CloseAllWidgets() {
  std::set<std::string> closed_toast_ids;

  while (!toasts_.empty()) {
    ToastContentsView* toast = toasts_.front();
    toasts_.pop_front();
    closed_toast_ids.insert(toast->id());

    OnMouseExited(toast);

    // CloseWithAnimation will cause the toast to forget about |this| so it is
    // required when we forget a toast.
    toast->CloseWithAnimation();
  }

  return closed_toast_ids;
}

void MessagePopupCollection::ForgetToast(ToastContentsView* toast) {
  toasts_.remove(toast);
  OnMouseExited(toast);
}

void MessagePopupCollection::RemoveToast(ToastContentsView* toast,
                                         bool mark_as_shown) {
  ForgetToast(toast);

  toast->CloseWithAnimation();

  if (mark_as_shown)
    message_center_->MarkSinglePopupAsShown(toast->id(), false);
}

int MessagePopupCollection::GetToastOriginX(const gfx::Rect& toast_bounds)
    const {
#if defined(OS_CHROMEOS)
  // In ChromeOS, RTL UI language mirrors the whole desktop layout, so the toast
  // widgets should be at the bottom-left instead of bottom right.
  if (base::i18n::IsRTL())
    return work_area_.x() + kToastMarginX;
#endif
  if (alignment_ & POPUP_ALIGNMENT_LEFT)
    return work_area_.x() + kToastMarginX;
  return work_area_.right() - kToastMarginX - toast_bounds.width();
}

void MessagePopupCollection::RepositionWidgets() {
  bool top_down = alignment_ & POPUP_ALIGNMENT_TOP;
  int base = GetBaseLine(NULL);  // We don't want to position relative to last
                                 // toast - we want re-position.

  for (Toasts::const_iterator iter = toasts_.begin(); iter != toasts_.end();) {
    Toasts::const_iterator curr = iter++;
    gfx::Rect bounds((*curr)->bounds());
    bounds.set_x(GetToastOriginX(bounds));
    bounds.set_y(alignment_ & POPUP_ALIGNMENT_TOP ? base
                                                  : base - bounds.height());

    // The notification may scrolls the boundary of the screen due to image
    // load and such notifications should disappear. Do not call
    // CloseWithAnimation, we don't want to show the closing animation, and we
    // don't want to mark such notifications as shown. See crbug.com/233424
    if ((top_down ? work_area_.bottom() - bounds.bottom() : bounds.y()) >= 0)
      (*curr)->SetBoundsWithAnimation(bounds);
    else
      RemoveToast(*curr, /*mark_as_shown=*/false);

    // Shift the base line to be a few pixels above the last added toast or (few
    // pixels below last added toast if top-aligned).
    if (top_down)
      base += bounds.height() + kToastMarginY;
    else
      base -= bounds.height() + kToastMarginY;
  }
}

void MessagePopupCollection::RepositionWidgetsWithTarget() {
  if (toasts_.empty())
    return;

  bool top_down = alignment_ & POPUP_ALIGNMENT_TOP;

  // Nothing to do if there are no widgets above target if bottom-aligned or no
  // widgets below target if top-aligned.
  if (top_down ? toasts_.back()->origin().y() < target_top_edge_
               : toasts_.back()->origin().y() > target_top_edge_)
    return;

  Toasts::reverse_iterator iter = toasts_.rbegin();
  for (; iter != toasts_.rend(); ++iter) {
    // We only reposition widgets above target if bottom-aligned or widgets
    // below target if top-aligned.
    if (top_down ? (*iter)->origin().y() < target_top_edge_
                 : (*iter)->origin().y() > target_top_edge_)
      break;
  }
  --iter;

  // Slide length is the number of pixels the widgets should move so that their
  // bottom edge (top-edge if top-aligned) touches the target.
  int slide_length = std::abs(target_top_edge_ - (*iter)->origin().y());
  for (;; --iter) {
    gfx::Rect bounds((*iter)->bounds());

    // If top-aligned, shift widgets upwards by slide_length. If bottom-aligned,
    // shift them downwards by slide_length.
    if (top_down)
      bounds.set_y(bounds.y() - slide_length);
    else
      bounds.set_y(bounds.y() + slide_length);
    (*iter)->SetBoundsWithAnimation(bounds);

    if (iter == toasts_.rbegin())
      break;
  }
}

void MessagePopupCollection::ComputePopupAlignment(gfx::Rect work_area,
                                                   gfx::Rect screen_bounds) {
  // If the taskbar is at the top, render notifications top down. Some platforms
  // like Gnome can have taskbars at top and bottom. In this case it's more
  // likely that the systray is on the top one.
  alignment_ = work_area.y() > screen_bounds.y() ? POPUP_ALIGNMENT_TOP
                                                 : POPUP_ALIGNMENT_BOTTOM;

  // If the taskbar is on the left show the notifications on the left. Otherwise
  // show it on right since it's very likely that the systray is on the right if
  // the taskbar is on the top or bottom.
  // Since on some platforms like Ubuntu Unity there's also a launcher along
  // with a taskbar (panel), we need to check that there is really nothing at
  // the top before concluding that the taskbar is at the left.
  alignment_ = static_cast<PopupAlignment>(
      alignment_ |
      ((work_area.x() > screen_bounds.x() && work_area.y() == screen_bounds.y())
           ? POPUP_ALIGNMENT_LEFT
           : POPUP_ALIGNMENT_RIGHT));
}

int MessagePopupCollection::GetBaseLine(ToastContentsView* last_toast) const {
  bool top_down = alignment_ & POPUP_ALIGNMENT_TOP;
  int base;

  if (top_down) {
    if (!last_toast) {
      base = work_area_.y();
      if (!first_item_has_no_margin_)
        base += kToastMarginY;
      else
        base -= kNoToastMarginBorderAndShadowOffset;
    } else {
      base = toasts_.back()->bounds().bottom() + kToastMarginY;
    }
  } else {
    if (!last_toast) {
      base = work_area_.bottom();
      if (!first_item_has_no_margin_)
        base -= kToastMarginY;
      else
        base += kNoToastMarginBorderAndShadowOffset;
    } else {
      base = toasts_.back()->origin().y() - kToastMarginY;
    }
  }
  return base;
}

void MessagePopupCollection::OnNotificationAdded(
    const std::string& notification_id) {
  DoUpdateIfPossible();
}

void MessagePopupCollection::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  // Find a toast.
  Toasts::const_iterator iter = toasts_.begin();
  for (; iter != toasts_.end(); ++iter) {
    if ((*iter)->id() == notification_id)
      break;
  }
  if (iter == toasts_.end())
    return;

  target_top_edge_ = (*iter)->bounds().y();
  if (by_user && !user_is_closing_toasts_by_clicking_) {
    // [Re] start a timeout after which the toasts re-position to their
    // normal locations after tracking the mouse pointer for easy deletion.
    // This provides a period of time when toasts are easy to remove because
    // they re-position themselves to have Close button right under the mouse
    // pointer. If the user continue to remove the toasts, the delay is reset.
    // Once user stopped removing the toasts, the toasts re-populate/rearrange
    // after the specified delay.
    user_is_closing_toasts_by_clicking_ = true;
    IncrementDeferCounter();
  }

  // CloseWithAnimation ultimately causes a call to RemoveToast, which calls
  // OnMouseExited.  This means that |user_is_closing_toasts_by_clicking_| must
  // have been set before this call, otherwise it will remain true even after
  // the toast is closed, since the defer timer won't be started.
  RemoveToast(*iter, /*mark_as_shown=*/true);

  if (by_user)
    RepositionWidgetsWithTarget();
}

void MessagePopupCollection::OnDeferTimerExpired() {
  user_is_closing_toasts_by_clicking_ = false;
  DecrementDeferCounter();

  message_center_->RestartPopupTimers();
}

void MessagePopupCollection::OnNotificationUpdated(
    const std::string& notification_id) {
  // Find a toast.
  Toasts::const_iterator toast_iter = toasts_.begin();
  for (; toast_iter != toasts_.end(); ++toast_iter) {
    if ((*toast_iter)->id() == notification_id)
      break;
  }
  if (toast_iter == toasts_.end())
    return;

  NotificationList::PopupNotifications notifications =
      message_center_->GetPopupNotifications();
  bool updated = false;

  for (NotificationList::PopupNotifications::iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    Notification* notification = *iter;
    DCHECK(notification);
    ToastContentsView* toast_contents_view = *toast_iter;
    DCHECK(toast_contents_view);

    if (notification->id() != notification_id)
      continue;

    const RichNotificationData& optional_fields =
        notification->rich_notification_data();
    bool a11y_feedback_for_updates =
        optional_fields.should_make_spoken_feedback_for_popup_updates;

    toast_contents_view->UpdateContents(*notification,
                                        a11y_feedback_for_updates);

    updated = true;
  }

  // OnNotificationUpdated() can be called when a notification is excluded from
  // the popup notification list but still remains in the full notification
  // list. In that case the widget for the notification has to be closed here.
  if (!updated)
    RemoveToast(*toast_iter, /*mark_as_shown=*/true);

  if (user_is_closing_toasts_by_clicking_)
    RepositionWidgetsWithTarget();
  else
    DoUpdateIfPossible();
}

ToastContentsView* MessagePopupCollection::FindToast(
    const std::string& notification_id) const {
  for (Toasts::const_iterator iter = toasts_.begin(); iter != toasts_.end();
       ++iter) {
    if ((*iter)->id() == notification_id)
      return *iter;
  }
  return NULL;
}

void MessagePopupCollection::IncrementDeferCounter() {
  defer_counter_++;
}

void MessagePopupCollection::DecrementDeferCounter() {
  defer_counter_--;
  DCHECK(defer_counter_ >= 0);
  DoUpdateIfPossible();
}

// This is the main sequencer of tasks. It does a step, then waits for
// all started transitions to play out before doing the next step.
// First, remove all expired toasts.
// Then, reposition widgets (the reposition on close happens before all
// deferred tasks are even able to run)
// Then, see if there is vacant space for new toasts.
void MessagePopupCollection::DoUpdateIfPossible() {
  if (!screen_) {
    gfx::Display display;
    if (!parent_) {
      // On Win+Aura, we don't have a parent since the popups currently show up
      // on the Windows desktop, not in the Aura/Ash desktop.  This code will
      // display the popups on the primary display.
      screen_ = gfx::Screen::GetNativeScreen();
      display = screen_->GetPrimaryDisplay();
    } else {
      screen_ = gfx::Screen::GetScreenFor(parent_);
      display = screen_->GetDisplayNearestWindow(parent_);
    }
    screen_->AddObserver(this);

    display_id_ = display.id();
    // |work_area_| can be set already and it should not be overwritten here.
    if (work_area_.IsEmpty()) {
      work_area_ = display.work_area();
      ComputePopupAlignment(work_area_, display.bounds());
    }
  }

  if (defer_counter_ > 0)
    return;

  RepositionWidgets();

  if (defer_counter_ > 0)
    return;

  // Reposition could create extra space which allows additional widgets.
  UpdateWidgets();

  if (defer_counter_ > 0)
    return;

  // Test support. Quit the test run loop when no more updates are deferred,
  // meaining th echeck for updates did not cause anything to change so no new
  // transition animations were started.
  if (run_loop_for_test_.get())
    run_loop_for_test_->Quit();
}

void MessagePopupCollection::SetDisplayInfo(const gfx::Rect& work_area,
                                            const gfx::Rect& screen_bounds) {
  if (work_area_ == work_area)
    return;

  work_area_ = work_area;
  ComputePopupAlignment(work_area, screen_bounds);
  RepositionWidgets();
}

void MessagePopupCollection::OnDisplayAdded(const gfx::Display& new_display) {
}

void MessagePopupCollection::OnDisplayRemoved(const gfx::Display& old_display) {
  if (display_id_ == old_display.id() && !parent_) {
    gfx::Display display = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
    display_id_ = display.id();
    SetDisplayInfo(display.work_area(), display.bounds());
  }
}

void MessagePopupCollection::OnDisplayMetricsChanged(
    const gfx::Display& display, uint32_t metrics) {
  if (display.id() != display_id_)
    return;

  if (metrics & DISPLAY_METRIC_BOUNDS || metrics & DISPLAY_METRIC_WORK_AREA)
    SetDisplayInfo(display.work_area(), display.bounds());
}

views::Widget* MessagePopupCollection::GetWidgetForTest(const std::string& id)
    const {
  for (Toasts::const_iterator iter = toasts_.begin(); iter != toasts_.end();
       ++iter) {
    if ((*iter)->id() == id)
      return (*iter)->GetWidget();
  }
  return NULL;
}

void MessagePopupCollection::CreateRunLoopForTest() {
  run_loop_for_test_.reset(new base::RunLoop());
}

void MessagePopupCollection::WaitForTest() {
  run_loop_for_test_->Run();
  run_loop_for_test_.reset();
}

gfx::Rect MessagePopupCollection::GetToastRectAt(size_t index) const {
  DCHECK(defer_counter_ == 0) << "Fetching the bounds with animations active.";
  size_t i = 0;
  for (Toasts::const_iterator iter = toasts_.begin(); iter != toasts_.end();
       ++iter) {
    if (i++ == index) {
      views::Widget* widget = (*iter)->GetWidget();
      if (widget)
        return widget->GetWindowBoundsInScreen();
      break;
    }
  }
  return gfx::Rect();
}

}  // namespace message_center
