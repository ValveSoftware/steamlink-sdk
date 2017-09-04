// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/toast_contents_view.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/popup_alignment_delegate.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif

using display::Screen;

namespace message_center {
namespace {

// The width of a toast before animated reveal and after closing.
const int kClosedToastWidth = 5;

// FadeIn/Out look a bit better if they are slightly longer then default slide.
const int kFadeInOutDuration = 200;

}  // namespace.

// static
gfx::Size ToastContentsView::GetToastSizeForView(const views::View* view) {
  int width = kNotificationWidth + view->GetInsets().width();
  return gfx::Size(width, view->GetHeightForWidth(width));
}

ToastContentsView::ToastContentsView(
    const std::string& notification_id,
    PopupAlignmentDelegate* alignment_delegate,
    base::WeakPtr<MessagePopupCollection> collection)
    : collection_(collection),
      id_(notification_id),
      is_closing_(false),
      closing_animation_(NULL) {
  DCHECK(alignment_delegate);
  set_notify_enter_exit_on_child(true);
  // Sets the transparent background. Then, when the message view is slid out,
  // the whole toast seems to slide although the actual bound of the widget
  // remains. This is hacky but easier to keep the consistency.
  set_background(views::Background::CreateSolidBackground(0, 0, 0, 0));

  fade_animation_.reset(new gfx::SlideAnimation(this));
  fade_animation_->SetSlideDuration(kFadeInOutDuration);

  CreateWidget(alignment_delegate);
}

// This is destroyed when the toast window closes.
ToastContentsView::~ToastContentsView() {
  if (collection_)
    collection_->ForgetToast(this);
}

void ToastContentsView::SetContents(MessageView* view,
                                    bool a11y_feedback_for_updates) {
  bool already_has_contents = child_count() > 0;
  RemoveAllChildViews(true);
  AddChildView(view);
  UpdatePreferredSize();

  // If it has the contents already, this invocation means an update of the
  // popup toast, and the new contents should be read through a11y feature.
  // The notification type should be ALERT, otherwise the accessibility message
  // won't be read for this view which returns ROLE_WINDOW.
  if (already_has_contents && a11y_feedback_for_updates)
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, false);
}

void ToastContentsView::UpdateContents(const Notification& notification,
                                       bool a11y_feedback_for_updates) {
  DCHECK_GT(child_count(), 0);
  MessageView* message_view = static_cast<MessageView*>(child_at(0));
  message_view->UpdateWithNotification(notification);
  UpdatePreferredSize();
  if (a11y_feedback_for_updates)
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, false);
}

void ToastContentsView::RevealWithAnimation(gfx::Point origin) {
  // Place/move the toast widgets. Currently it stacks the widgets from the
  // right-bottom of the work area.
  // TODO(mukai): allow to specify the placement policy from outside of this
  // class. The policy should be specified from preference on Windows, or
  // the launcher alignment on ChromeOS.
  origin_ = gfx::Point(origin.x() - preferred_size_.width(),
                       origin.y() - preferred_size_.height());

  gfx::Rect stable_bounds(origin_, preferred_size_);

  SetBoundsInstantly(GetClosedToastBounds(stable_bounds));
  StartFadeIn();
  SetBoundsWithAnimation(stable_bounds);
}

void ToastContentsView::CloseWithAnimation() {
  if (is_closing_)
    return;
  is_closing_ = true;
  StartFadeOut();
}

void ToastContentsView::SetBoundsInstantly(gfx::Rect new_bounds) {
  DCHECK(new_bounds.size().width() <= preferred_size_.width() &&
         new_bounds.size().height() <= preferred_size_.height())
      << "we can not display widget bigger than notification";

  if (!GetWidget())
    return;

  if (new_bounds == GetWidget()->GetWindowBoundsInScreen())
    return;

  origin_ = new_bounds.origin();
  GetWidget()->SetBounds(new_bounds);
}

void ToastContentsView::SetBoundsWithAnimation(gfx::Rect new_bounds) {
  DCHECK(new_bounds.size().width() <= preferred_size_.width() &&
         new_bounds.size().height() <= preferred_size_.height())
      << "we can not display widget bigger than notification";

  if (!GetWidget())
    return;

  if (new_bounds == animated_bounds_end_)
    return;

  origin_ = new_bounds.origin();
  // This picks up the current bounds, so if there was a previous animation
  // half-done, the next one will pick up from the current location.
  // This is the only place that should query current location of the Widget
  // on screen, the rest should refer to the bounds_.
  animated_bounds_start_ = GetWidget()->GetWindowBoundsInScreen();
  animated_bounds_end_ = new_bounds;

  if (collection_)
    collection_->IncrementDeferCounter();

  if (bounds_animation_.get())
    bounds_animation_->Stop();

  bounds_animation_.reset(new gfx::SlideAnimation(this));
  bounds_animation_->Show();
}

void ToastContentsView::StartFadeIn() {
  // The decrement is done in OnBoundsAnimationEndedOrCancelled callback.
  if (collection_)
    collection_->IncrementDeferCounter();
  fade_animation_->Stop();

  GetWidget()->SetOpacity(0);
  GetWidget()->ShowInactive();
  fade_animation_->Reset(0);
  fade_animation_->Show();
}

void ToastContentsView::StartFadeOut() {
  // The decrement is done in OnBoundsAnimationEndedOrCancelled callback.
  if (collection_)
    collection_->IncrementDeferCounter();
  fade_animation_->Stop();

  closing_animation_ = (is_closing_ ? fade_animation_.get() : NULL);
  fade_animation_->Reset(1);
  fade_animation_->Hide();
}

void ToastContentsView::OnBoundsAnimationEndedOrCancelled(
    const gfx::Animation* animation) {
  if (is_closing_ && closing_animation_ == animation && GetWidget()) {
    views::Widget* widget = GetWidget();

    // TODO(dewittj): This is a workaround to prevent a nasty bug where
    // closing a transparent widget doesn't actually remove the window,
    // causing entire areas of the screen to become unresponsive to clicks.
    // See crbug.com/243469
    widget->Hide();
#if defined(OS_WIN)
    widget->SetOpacity(1.f);
#endif

    widget->Close();
  }

  // This cannot be called before GetWidget()->Close(). Decrementing defer count
  // will invoke update, which may invoke another close animation with
  // incrementing defer counter. Close() after such process will cause a
  // mismatch between increment/decrement. See crbug.com/238477
  if (collection_)
    collection_->DecrementDeferCounter();
}

// gfx::AnimationDelegate
void ToastContentsView::AnimationProgressed(const gfx::Animation* animation) {
  if (animation == bounds_animation_.get()) {
    gfx::Rect current(animation->CurrentValueBetween(
        animated_bounds_start_, animated_bounds_end_));
    GetWidget()->SetBounds(current);
  } else if (animation == fade_animation_.get()) {
    GetWidget()->SetOpacity(
        static_cast<float>(fade_animation_->GetCurrentValue()));
  }
}

void ToastContentsView::AnimationEnded(const gfx::Animation* animation) {
  OnBoundsAnimationEndedOrCancelled(animation);
}

void ToastContentsView::AnimationCanceled(
    const gfx::Animation* animation) {
  OnBoundsAnimationEndedOrCancelled(animation);
}

// views::WidgetDelegate
void ToastContentsView::WindowClosing() {
  if (!is_closing_ && collection_.get())
    collection_->ForgetToast(this);
}

void ToastContentsView::OnDisplayChanged() {
  views::Widget* widget = GetWidget();
  if (!widget)
    return;

  gfx::NativeView native_view = widget->GetNativeView();
  if (!native_view || !collection_.get())
    return;

  collection_->OnDisplayMetricsChanged(
      Screen::GetScreen()->GetDisplayNearestWindow(native_view));
}

void ToastContentsView::OnWorkAreaChanged() {
  views::Widget* widget = GetWidget();
  if (!widget)
    return;

  gfx::NativeView native_view = widget->GetNativeView();
  if (!native_view || !collection_.get())
    return;

  collection_->OnDisplayMetricsChanged(
      Screen::GetScreen()->GetDisplayNearestWindow(native_view));
}

// views::View
void ToastContentsView::OnMouseEntered(const ui::MouseEvent& event) {
  if (collection_)
    collection_->OnMouseEntered(this);
}

void ToastContentsView::OnMouseExited(const ui::MouseEvent& event) {
  if (collection_)
    collection_->OnMouseExited(this);
}

void ToastContentsView::Layout() {
  if (child_count() > 0) {
    child_at(0)->SetBounds(
        0, 0, preferred_size_.width(), preferred_size_.height());
  }
}

gfx::Size ToastContentsView::GetPreferredSize() const {
  return child_count() ? GetToastSizeForView(child_at(0)) : gfx::Size();
}

void ToastContentsView::UpdatePreferredSize() {
  DCHECK_GT(child_count(), 0);
  gfx::Size new_size = GetToastSizeForView(child_at(0));
  if (preferred_size_ == new_size)
    return;
  // Growing notifications instantly can cause notification's to overlap
  // And shrinking with animation, leaves area, where nothing is drawn
  const bool change_instantly = preferred_size_.width() > new_size.width() ||
                                preferred_size_.height() > new_size.height();
  preferred_size_ = new_size;
  Layout();
  if (change_instantly) {
    SetBoundsInstantly(bounds());
    return;
  }
  SetBoundsWithAnimation(bounds());
}

void ToastContentsView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  if (child_count() > 0)
    child_at(0)->GetAccessibleNodeData(node_data);
  node_data->role = ui::AX_ROLE_WINDOW;
}

void ToastContentsView::ClickOnNotification(
    const std::string& notification_id) {
  if (collection_)
    collection_->ClickOnNotification(notification_id);
}

void ToastContentsView::ClickOnSettingsButton(
    const std::string& notification_id) {
  if (collection_)
    collection_->ClickOnSettingsButton(notification_id);
}

void ToastContentsView::RemoveNotification(
    const std::string& notification_id,
    bool by_user) {
  if (collection_)
    collection_->RemoveNotification(notification_id, by_user);
}

std::unique_ptr<ui::MenuModel> ToastContentsView::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  // Should not reach, the context menu should be handled in
  // MessagePopupCollection.
  NOTREACHED();
  return nullptr;
}

bool ToastContentsView::HasClickedListener(
    const std::string& notification_id) {
  if (!collection_)
    return false;
  return collection_->HasClickedListener(notification_id);
}

void ToastContentsView::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  if (collection_)
    collection_->ClickOnNotificationButton(notification_id, button_index);
}

void ToastContentsView::CreateWidget(
    PopupAlignmentDelegate* alignment_delegate) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.keep_on_top = true;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
#else
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
#endif
  params.delegate = this;
  views::Widget* widget = new views::Widget();
  alignment_delegate->ConfigureWidgetInitParamsForContainer(widget, &params);
  widget->set_focus_on_creation(false);

#if defined(OS_WIN)
  // We want to ensure that this toast always goes to the native desktop,
  // not the Ash desktop (since there is already another toast contents view
  // there.
  if (!params.parent)
    params.native_widget = new views::DesktopNativeWidgetAura(widget);
#endif

  widget->Init(params);
}

gfx::Rect ToastContentsView::GetClosedToastBounds(gfx::Rect bounds) {
  return gfx::Rect(bounds.x() + bounds.width() - kClosedToastWidth,
                   bounds.y(),
                   kClosedToastWidth,
                   bounds.height());
}

}  // namespace message_center
