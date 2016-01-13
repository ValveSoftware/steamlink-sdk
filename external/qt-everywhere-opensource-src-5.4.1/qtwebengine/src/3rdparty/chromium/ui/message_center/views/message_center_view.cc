// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_center_view.h"

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_types.h"
#include "ui/message_center/views/message_center_button_bar.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_context_menu_controller.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/notifier_settings_view.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

const SkColor kNoNotificationsTextColor = SkColorSetRGB(0xb4, 0xb4, 0xb4);
#if defined(OS_LINUX) && defined(OS_CHROMEOS)
const SkColor kTransparentColor = SkColorSetARGB(0, 0, 0, 0);
#endif
const int kAnimateClearingNextNotificationDelayMS = 40;

const int kDefaultAnimationDurationMs = 120;
const int kDefaultFrameRateHz = 60;
}  // namespace

class NoNotificationMessageView : public views::View {
 public:
  NoNotificationMessageView();
  virtual ~NoNotificationMessageView();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(NoNotificationMessageView);
};

NoNotificationMessageView::NoNotificationMessageView() {
  label_ = new views::Label(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_NO_MESSAGES));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(kNoNotificationsTextColor);
  // Set transparent background to ensure that subpixel rendering
  // is disabled. See crbug.com/169056
#if defined(OS_LINUX) && defined(OS_CHROMEOS)
  label_->SetBackgroundColor(kTransparentColor);
#endif
  AddChildView(label_);
}

NoNotificationMessageView::~NoNotificationMessageView() {
}

gfx::Size NoNotificationMessageView::GetPreferredSize() const {
  return gfx::Size(kMinScrollViewHeight, label_->GetPreferredSize().width());
}

int NoNotificationMessageView::GetHeightForWidth(int width) const {
  return kMinScrollViewHeight;
}

void NoNotificationMessageView::Layout() {
  int text_height = label_->GetHeightForWidth(width());
  int margin = (height() - text_height) / 2;
  label_->SetBounds(0, margin, width(), text_height);
}

// Displays a list of messages for rich notifications. Functions as an array of
// MessageViews and animates them on transitions. It also supports
// repositioning.
class MessageListView : public views::View,
                        public views::BoundsAnimatorObserver {
 public:
  explicit MessageListView(MessageCenterView* message_center_view,
                           bool top_down);
  virtual ~MessageListView();

  void AddNotificationAt(MessageView* view, int i);
  void RemoveNotification(MessageView* view);
  void UpdateNotification(MessageView* view, const Notification& notification);
  void SetRepositionTarget(const gfx::Rect& target_rect);
  void ResetRepositionSession();
  void ClearAllNotifications(const gfx::Rect& visible_scroll_rect);

 protected:
  // Overridden from views::View.
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void PaintChildren(gfx::Canvas* canvas,
                             const views::CullSet& cull_set) OVERRIDE;
  virtual void ReorderChildLayers(ui::Layer* parent_layer) OVERRIDE;

  // Overridden from views::BoundsAnimatorObserver.
  virtual void OnBoundsAnimatorProgressed(
      views::BoundsAnimator* animator) OVERRIDE;
  virtual void OnBoundsAnimatorDone(views::BoundsAnimator* animator) OVERRIDE;

 private:
  bool IsValidChild(const views::View* child) const;
  void DoUpdateIfPossible();

  // Animates all notifications below target upwards to align with the top of
  // the last closed notification.
  void AnimateNotificationsBelowTarget();
  // Animates all notifications above target downwards to align with the top of
  // the last closed notification.
  void AnimateNotificationsAboveTarget();

  // Schedules animation for a child to the specified position. Returns false
  // if |child| will disappear after the animation.
  bool AnimateChild(views::View* child, int top, int height);

  // Animate clearing one notification.
  void AnimateClearingOneNotification();
  MessageCenterView* message_center_view() const {
    return message_center_view_;
  }

  MessageCenterView* message_center_view_;  // Weak reference.
  // The top position of the reposition target rectangle.
  int reposition_top_;
  int fixed_height_;
  bool has_deferred_task_;
  bool clear_all_started_;
  bool top_down_;
  std::set<views::View*> adding_views_;
  std::set<views::View*> deleting_views_;
  std::set<views::View*> deleted_when_done_;
  std::list<views::View*> clearing_all_views_;
  scoped_ptr<views::BoundsAnimator> animator_;
  base::WeakPtrFactory<MessageListView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageListView);
};

MessageListView::MessageListView(MessageCenterView* message_center_view,
                                 bool top_down)
    : message_center_view_(message_center_view),
      reposition_top_(-1),
      fixed_height_(0),
      has_deferred_task_(false),
      clear_all_started_(false),
      top_down_(top_down),
      weak_ptr_factory_(this) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_FILL);
  SetLayoutManager(layout);

  // Set the margin to 0 for the layout. BoxLayout assumes the same margin
  // for top and bottom, but the bottom margin here should be smaller
  // because of the shadow of message view. Use an empty border instead
  // to provide this margin.
  gfx::Insets shadow_insets = MessageView::GetShadowInsets();
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));
  SetBorder(views::Border::CreateEmptyBorder(
      top_down ? 0 : kMarginBetweenItems - shadow_insets.top(),    /* top */
      kMarginBetweenItems - shadow_insets.left(),                  /* left */
      top_down ? kMarginBetweenItems - shadow_insets.bottom() : 0, /* bottom */
      kMarginBetweenItems - shadow_insets.right() /* right */));
}

MessageListView::~MessageListView() {
  if (animator_.get())
    animator_->RemoveObserver(this);
}

void MessageListView::Layout() {
  if (animator_.get())
    return;

  gfx::Rect child_area = GetContentsBounds();
  int top = child_area.y();
  int between_items =
      kMarginBetweenItems - MessageView::GetShadowInsets().bottom();

  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    int height = child->GetHeightForWidth(child_area.width());
    child->SetBounds(child_area.x(), top, child_area.width(), height);
    top += height + between_items;
  }
}

void MessageListView::AddNotificationAt(MessageView* view, int index) {
  // |index| refers to a position in a subset of valid children. |real_index|
  // in a list includes the invalid children, so we compute the real index by
  // walking the list until |index| number of valid children are encountered,
  // or to the end of the list.
  int real_index = 0;
  while (real_index < child_count()) {
    if (IsValidChild(child_at(real_index))) {
      --index;
      if (index < 0)
        break;
    }
    ++real_index;
  }

  AddChildViewAt(view, real_index);
  if (GetContentsBounds().IsEmpty())
    return;

  adding_views_.insert(view);
  DoUpdateIfPossible();
}

void MessageListView::RemoveNotification(MessageView* view) {
  DCHECK_EQ(view->parent(), this);
  if (GetContentsBounds().IsEmpty()) {
    delete view;
  } else {
    if (view->layer()) {
      deleting_views_.insert(view);
    } else {
      if (animator_.get())
        animator_->StopAnimatingView(view);
      delete view;
    }
    DoUpdateIfPossible();
  }
}

void MessageListView::UpdateNotification(MessageView* view,
                                         const Notification& notification) {
  int index = GetIndexOf(view);
  DCHECK_LE(0, index);  // GetIndexOf is negative if not a child.

  if (animator_.get())
    animator_->StopAnimatingView(view);
  if (deleting_views_.find(view) != deleting_views_.end())
    deleting_views_.erase(view);
  if (deleted_when_done_.find(view) != deleted_when_done_.end())
    deleted_when_done_.erase(view);
  view->UpdateWithNotification(notification);
  DoUpdateIfPossible();
}

gfx::Size MessageListView::GetPreferredSize() const {
  int width = 0;
  for (int i = 0; i < child_count(); i++) {
    const views::View* child = child_at(i);
    if (IsValidChild(child))
      width = std::max(width, child->GetPreferredSize().width());
  }

  return gfx::Size(width + GetInsets().width(),
                   GetHeightForWidth(width + GetInsets().width()));
}

int MessageListView::GetHeightForWidth(int width) const {
  if (fixed_height_ > 0)
    return fixed_height_;

  width -= GetInsets().width();
  int height = 0;
  int padding = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(i);
    if (!IsValidChild(child))
      continue;
    height += child->GetHeightForWidth(width) + padding;
    padding = kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
  }

  return height + GetInsets().height();
}

void MessageListView::PaintChildren(gfx::Canvas* canvas,
                                    const views::CullSet& cull_set) {
  // Paint in the inversed order. Otherwise upper notification may be
  // hidden by the lower one.
  for (int i = child_count() - 1; i >= 0; --i) {
    if (!child_at(i)->layer())
      child_at(i)->Paint(canvas, cull_set);
  }
}

void MessageListView::ReorderChildLayers(ui::Layer* parent_layer) {
  // Reorder children to stack the last child layer at the top. Otherwise
  // upper notification may be hidden by the lower one.
  for (int i = 0; i < child_count(); ++i) {
    if (child_at(i)->layer())
      parent_layer->StackAtBottom(child_at(i)->layer());
  }
}

void MessageListView::SetRepositionTarget(const gfx::Rect& target) {
  reposition_top_ = target.y();
  fixed_height_ = GetHeightForWidth(width());
}

void MessageListView::ResetRepositionSession() {
  // Don't call DoUpdateIfPossible(), but let Layout() do the task without
  // animation. Reset will cause the change of the bubble size itself, and
  // animation from the old location will look weird.
  if (reposition_top_ >= 0 && animator_.get()) {
    has_deferred_task_ = false;
    // cancel cause OnBoundsAnimatorDone which deletes |deleted_when_done_|.
    animator_->Cancel();
    STLDeleteContainerPointers(deleting_views_.begin(), deleting_views_.end());
    deleting_views_.clear();
    adding_views_.clear();
    animator_.reset();
  }

  reposition_top_ = -1;
  fixed_height_ = 0;
}

void MessageListView::ClearAllNotifications(
    const gfx::Rect& visible_scroll_rect) {
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!child->visible())
      continue;
    if (gfx::IntersectRects(child->bounds(), visible_scroll_rect).IsEmpty())
      continue;
    clearing_all_views_.push_back(child);
  }
  DoUpdateIfPossible();
}

void MessageListView::OnBoundsAnimatorProgressed(
    views::BoundsAnimator* animator) {
  DCHECK_EQ(animator_.get(), animator);
  for (std::set<views::View*>::iterator iter = deleted_when_done_.begin();
       iter != deleted_when_done_.end(); ++iter) {
    const gfx::SlideAnimation* animation = animator->GetAnimationForView(*iter);
    if (animation)
      (*iter)->layer()->SetOpacity(animation->CurrentValueBetween(1.0, 0.0));
  }
}

void MessageListView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  STLDeleteContainerPointers(
      deleted_when_done_.begin(), deleted_when_done_.end());
  deleted_when_done_.clear();

  if (clear_all_started_) {
    clear_all_started_ = false;
    message_center_view()->OnAllNotificationsCleared();
  }

  if (has_deferred_task_) {
    has_deferred_task_ = false;
    DoUpdateIfPossible();
  }

  if (GetWidget())
    GetWidget()->SynthesizeMouseMoveEvent();
}

bool MessageListView::IsValidChild(const views::View* child) const {
  return child->visible() &&
      deleting_views_.find(const_cast<views::View*>(child)) ==
          deleting_views_.end() &&
      deleted_when_done_.find(const_cast<views::View*>(child)) ==
          deleted_when_done_.end();
}

void MessageListView::DoUpdateIfPossible() {
  gfx::Rect child_area = GetContentsBounds();
  if (child_area.IsEmpty())
    return;

  if (animator_.get() && animator_->IsAnimating()) {
    has_deferred_task_ = true;
    return;
  }

  if (!animator_.get()) {
    animator_.reset(new views::BoundsAnimator(this));
    animator_->AddObserver(this);
  }

  if (!clearing_all_views_.empty()) {
    AnimateClearingOneNotification();
    return;
  }

  if (top_down_)
    AnimateNotificationsBelowTarget();
  else
    AnimateNotificationsAboveTarget();

  adding_views_.clear();
  deleting_views_.clear();
}

void MessageListView::AnimateNotificationsBelowTarget() {
  int last_index = -1;
  for (int i = 0; i < child_count(); ++i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child)) {
      AnimateChild(child, child->y(), child->height());
    } else if (reposition_top_ < 0 || child->y() > reposition_top_) {
      // Find first notification below target (or all notifications if no
      // target).
      last_index = i;
      break;
    }
  }
  if (last_index > 0) {
    int between_items =
        kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
    int top = (reposition_top_ > 0) ? reposition_top_ : GetInsets().top();

    for (int i = last_index; i < child_count(); ++i) {
      // Animate notifications below target upwards.
      views::View* child = child_at(i);
      if (AnimateChild(child, top, child->height()))
        top += child->height() + between_items;
    }
  }
}

void MessageListView::AnimateNotificationsAboveTarget() {
  int last_index = -1;
  for (int i = child_count() - 1; i >= 0; --i) {
    views::View* child = child_at(i);
    if (!IsValidChild(child)) {
      AnimateChild(child, child->y(), child->height());
    } else if (reposition_top_ < 0 || child->y() < reposition_top_) {
      // Find first notification above target (or all notifications if no
      // target).
      last_index = i;
      break;
    }
  }
  if (last_index >= 0) {
    int between_items =
        kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
    int bottom = (reposition_top_ > 0)
                     ? reposition_top_ + child_at(last_index)->height()
                     : GetHeightForWidth(width()) - GetInsets().bottom();
    for (int i = last_index; i >= 0; --i) {
      // Animate notifications above target downwards.
      views::View* child = child_at(i);
      if (AnimateChild(child, bottom - child->height(), child->height()))
        bottom -= child->height() + between_items;
    }
  }
}

bool MessageListView::AnimateChild(views::View* child, int top, int height) {
  gfx::Rect child_area = GetContentsBounds();
  if (adding_views_.find(child) != adding_views_.end()) {
    child->SetBounds(child_area.right(), top, child_area.width(), height);
    animator_->AnimateViewTo(
        child, gfx::Rect(child_area.x(), top, child_area.width(), height));
  } else if (deleting_views_.find(child) != deleting_views_.end()) {
    DCHECK(child->layer());
    // No moves, but animate to fade-out.
    animator_->AnimateViewTo(child, child->bounds());
    deleted_when_done_.insert(child);
    return false;
  } else {
    gfx::Rect target(child_area.x(), top, child_area.width(), height);
    if (child->bounds().origin() != target.origin())
      animator_->AnimateViewTo(child, target);
    else
      child->SetBoundsRect(target);
  }
  return true;
}

void MessageListView::AnimateClearingOneNotification() {
  DCHECK(!clearing_all_views_.empty());

  clear_all_started_ = true;

  views::View* child = clearing_all_views_.front();
  clearing_all_views_.pop_front();

  // Slide from left to right.
  gfx::Rect new_bounds = child->bounds();
  new_bounds.set_x(new_bounds.right() + kMarginBetweenItems);
  animator_->AnimateViewTo(child, new_bounds);

  // Schedule to start sliding out next notification after a short delay.
  if (!clearing_all_views_.empty()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MessageListView::AnimateClearingOneNotification,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAnimateClearingNextNotificationDelayMS));
  }
}

// MessageCenterView ///////////////////////////////////////////////////////////

MessageCenterView::MessageCenterView(MessageCenter* message_center,
                                     MessageCenterTray* tray,
                                     int max_height,
                                     bool initially_settings_visible,
                                     bool top_down,
                                     const base::string16& title)
    : message_center_(message_center),
      tray_(tray),
      scroller_(NULL),
      settings_view_(NULL),
      button_bar_(NULL),
      top_down_(top_down),
      settings_visible_(initially_settings_visible),
      source_view_(NULL),
      source_height_(0),
      target_view_(NULL),
      target_height_(0),
      is_closing_(false),
      context_menu_controller_(new MessageViewContextMenuController(this)) {
  message_center_->AddObserver(this);
  set_notify_enter_exit_on_child(true);
  set_background(views::Background::CreateSolidBackground(
      kMessageCenterBackgroundColor));

  NotifierSettingsProvider* notifier_settings_provider =
      message_center_->GetNotifierSettingsProvider();
  button_bar_ = new MessageCenterButtonBar(this,
                                           message_center,
                                           notifier_settings_provider,
                                           initially_settings_visible,
                                           title);

  const int button_height = button_bar_->GetPreferredSize().height();

  scroller_ = new views::ScrollView();
  scroller_->ClipHeightTo(kMinScrollViewHeight, max_height - button_height);
  scroller_->SetVerticalScrollBar(new views::OverlayScrollBar(false));
  scroller_->set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));

  scroller_->SetPaintToLayer(true);
  scroller_->SetFillsBoundsOpaquely(false);
  scroller_->layer()->SetMasksToBounds(true);

  empty_list_view_.reset(new NoNotificationMessageView);
  empty_list_view_->set_owned_by_client();
  message_list_view_.reset(new MessageListView(this, top_down));
  message_list_view_->set_owned_by_client();

  // We want to swap the contents of the scroll view between the empty list
  // view and the message list view, without constructing them afresh each
  // time.  So, since the scroll view deletes old contents each time you
  // set the contents (regardless of the |owned_by_client_| setting) we need
  // an intermediate view for the contents whose children we can swap in and
  // out.
  views::View* scroller_contents = new views::View();
  scroller_contents->SetLayoutManager(new views::FillLayout());
  scroller_contents->AddChildView(empty_list_view_.get());
  scroller_->SetContents(scroller_contents);

  settings_view_ = new NotifierSettingsView(notifier_settings_provider);

  if (initially_settings_visible)
    scroller_->SetVisible(false);
  else
    settings_view_->SetVisible(false);

  AddChildView(scroller_);
  AddChildView(settings_view_);
  AddChildView(button_bar_);
}

MessageCenterView::~MessageCenterView() {
  if (!is_closing_)
    message_center_->RemoveObserver(this);
}

void MessageCenterView::SetNotifications(
    const NotificationList::Notifications& notifications)  {
  if (is_closing_)
    return;

  notification_views_.clear();

  int index = 0;
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    AddNotificationAt(*(*iter), index++);

    message_center_->DisplayedNotification(
        (*iter)->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
    if (notification_views_.size() >= kMaxVisibleMessageCenterNotifications)
      break;
  }

  NotificationsChanged();
  scroller_->RequestFocus();
}

void MessageCenterView::SetSettingsVisible(bool visible) {
  if (is_closing_)
    return;

  if (visible == settings_visible_)
    return;

  settings_visible_ = visible;

  if (visible) {
    source_view_ = scroller_;
    target_view_ = settings_view_;
  } else {
    source_view_ = settings_view_;
    target_view_ = scroller_;
  }
  source_height_ = source_view_->GetHeightForWidth(width());
  target_height_ = target_view_->GetHeightForWidth(width());

  gfx::MultiAnimation::Parts parts;
  // First part: slide resize animation.
  parts.push_back(gfx::MultiAnimation::Part(
      (source_height_ == target_height_) ? 0 : kDefaultAnimationDurationMs,
      gfx::Tween::EASE_OUT));
  // Second part: fade-out the source_view.
  if (source_view_->layer()) {
    parts.push_back(gfx::MultiAnimation::Part(
        kDefaultAnimationDurationMs, gfx::Tween::LINEAR));
  } else {
    parts.push_back(gfx::MultiAnimation::Part());
  }
  // Third part: fade-in the target_view.
  if (target_view_->layer()) {
    parts.push_back(gfx::MultiAnimation::Part(
        kDefaultAnimationDurationMs, gfx::Tween::LINEAR));
    target_view_->layer()->SetOpacity(0);
    target_view_->SetVisible(true);
  } else {
    parts.push_back(gfx::MultiAnimation::Part());
  }
  settings_transition_animation_.reset(new gfx::MultiAnimation(
      parts, base::TimeDelta::FromMicroseconds(1000000 / kDefaultFrameRateHz)));
  settings_transition_animation_->set_delegate(this);
  settings_transition_animation_->set_continuous(false);
  settings_transition_animation_->Start();

  button_bar_->SetBackArrowVisible(visible);
}

void MessageCenterView::ClearAllNotifications() {
  if (is_closing_)
    return;

  scroller_->SetEnabled(false);
  button_bar_->SetAllButtonsEnabled(false);
  message_list_view_->ClearAllNotifications(scroller_->GetVisibleRect());
}

void MessageCenterView::OnAllNotificationsCleared() {
  scroller_->SetEnabled(true);
  button_bar_->SetAllButtonsEnabled(true);
  button_bar_->SetCloseAllButtonEnabled(false);
  message_center_->RemoveAllVisibleNotifications(true);  // Action by user.
}

size_t MessageCenterView::NumMessageViewsForTest() const {
  return message_list_view_->child_count();
}

void MessageCenterView::OnSettingsChanged() {
  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetIsClosing(bool is_closing) {
  is_closing_ = is_closing;
  if (is_closing)
    message_center_->RemoveObserver(this);
  else
    message_center_->AddObserver(this);
}

void MessageCenterView::Layout() {
  if (is_closing_)
    return;

  int button_height = button_bar_->GetHeightForWidth(width()) +
                      button_bar_->GetInsets().height();
  // Skip unnecessary re-layout of contents during the resize animation.
  bool animating = settings_transition_animation_ &&
                   settings_transition_animation_->is_animating();
  if (animating && settings_transition_animation_->current_part_index() == 0) {
    if (!top_down_) {
      button_bar_->SetBounds(
          0, height() - button_height, width(), button_height);
    }
    return;
  }

  scroller_->SetBounds(0,
                       top_down_ ? button_height : 0,
                       width(),
                       height() - button_height);
  settings_view_->SetBounds(0,
                            top_down_ ? button_height : 0,
                            width(),
                            height() - button_height);

  bool is_scrollable = false;
  if (scroller_->visible())
    is_scrollable = scroller_->height() < message_list_view_->height();
  else
    is_scrollable = settings_view_->IsScrollable();

  if (!animating) {
    if (is_scrollable) {
      // Draw separator line on the top of the button bar if it is on the bottom
      // or draw it at the bottom if the bar is on the top.
      button_bar_->SetBorder(views::Border::CreateSolidSidedBorder(
          top_down_ ? 0 : 1, 0, top_down_ ? 1 : 0, 0, kFooterDelimiterColor));
    } else {
      button_bar_->SetBorder(views::Border::CreateEmptyBorder(
          top_down_ ? 0 : 1, 0, top_down_ ? 1 : 0, 0));
    }
    button_bar_->SchedulePaint();
  }
  button_bar_->SetBounds(0,
                         top_down_ ? 0 : height() - button_height,
                         width(),
                         button_height);
  if (GetWidget())
    GetWidget()->GetRootView()->SchedulePaint();
}

gfx::Size MessageCenterView::GetPreferredSize() const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating()) {
    int content_width = std::max(source_view_->GetPreferredSize().width(),
                                 target_view_->GetPreferredSize().width());
    int width = std::max(content_width,
                         button_bar_->GetPreferredSize().width());
    return gfx::Size(width, GetHeightForWidth(width));
  }

  int width = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* child = child_at(0);
    if (child->visible())
      width = std::max(width, child->GetPreferredSize().width());
  }
  return gfx::Size(width, GetHeightForWidth(width));
}

int MessageCenterView::GetHeightForWidth(int width) const {
  if (settings_transition_animation_ &&
      settings_transition_animation_->is_animating()) {
    int content_height = target_height_;
    if (settings_transition_animation_->current_part_index() == 0) {
      content_height = settings_transition_animation_->CurrentValueBetween(
          source_height_, target_height_);
    }
    return button_bar_->GetHeightForWidth(width) + content_height;
  }

  int content_height = 0;
  if (scroller_->visible())
    content_height += scroller_->GetHeightForWidth(width);
  else
    content_height += settings_view_->GetHeightForWidth(width);
  return button_bar_->GetHeightForWidth(width) +
         button_bar_->GetInsets().height() + content_height;
}

bool MessageCenterView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do not rely on the default scroll event handler of ScrollView because
  // the scroll happens only when the focus is on the ScrollView. The
  // notification center will allow the scrolling even when the focus is on
  // the buttons.
  if (scroller_->bounds().Contains(event.location()))
    return scroller_->OnMouseWheel(event);
  return views::View::OnMouseWheel(event);
}

void MessageCenterView::OnMouseExited(const ui::MouseEvent& event) {
  if (is_closing_)
    return;

  message_list_view_->ResetRepositionSession();
  NotificationsChanged();
}

void MessageCenterView::OnNotificationAdded(const std::string& id) {
  int index = 0;
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end();
       ++iter, ++index) {
    if ((*iter)->id() == id) {
      AddNotificationAt(*(*iter), index);
      break;
    }
    if (notification_views_.size() >= kMaxVisibleMessageCenterNotifications)
      break;
  }
  NotificationsChanged();
}

void MessageCenterView::OnNotificationRemoved(const std::string& id,
                                              bool by_user) {
  NotificationViewsMap::iterator view_iter = notification_views_.find(id);
  if (view_iter == notification_views_.end())
    return;
  NotificationView* view = view_iter->second;
  int index = message_list_view_->GetIndexOf(view);
  DCHECK_LE(0, index);
  if (by_user) {
    message_list_view_->SetRepositionTarget(view->bounds());
    // Moves the keyboard focus to the next notification if the removed
    // notification is focused so that the user can dismiss notifications
    // without re-focusing by tab key.
    if (view->IsCloseButtonFocused() ||
        view == GetFocusManager()->GetFocusedView()) {
      views::View* next_focused_view = NULL;
      if (message_list_view_->child_count() > index + 1)
        next_focused_view = message_list_view_->child_at(index + 1);
      else if (index > 0)
        next_focused_view = message_list_view_->child_at(index - 1);

      if (next_focused_view) {
        if (view->IsCloseButtonFocused())
          // Safe cast since all views in MessageListView are MessageViews.
          static_cast<MessageView*>(
              next_focused_view)->RequestFocusOnCloseButton();
        else
          next_focused_view->RequestFocus();
      }
    }
  }
  message_list_view_->RemoveNotification(view);
  notification_views_.erase(view_iter);
  NotificationsChanged();
}

void MessageCenterView::OnNotificationUpdated(const std::string& id) {
  NotificationViewsMap::const_iterator view_iter = notification_views_.find(id);
  if (view_iter == notification_views_.end())
    return;
  NotificationView* view = view_iter->second;
  // TODO(dimich): add MessageCenter::GetVisibleNotificationById(id)
  const NotificationList::Notifications& notifications =
      message_center_->GetVisibleNotifications();
  for (NotificationList::Notifications::const_iterator iter =
           notifications.begin(); iter != notifications.end(); ++iter) {
    if ((*iter)->id() == id) {
      int old_width = view->width();
      int old_height = view->GetHeightForWidth(old_width);
      message_list_view_->UpdateNotification(view, **iter);
      if (view->GetHeightForWidth(old_width) != old_height)
        NotificationsChanged();
      break;
    }
  }
}

void MessageCenterView::ClickOnNotification(
    const std::string& notification_id) {
  message_center_->ClickOnNotification(notification_id);
}

void MessageCenterView::RemoveNotification(const std::string& notification_id,
                                           bool by_user) {
  message_center_->RemoveNotification(notification_id, by_user);
}

scoped_ptr<ui::MenuModel> MessageCenterView::CreateMenuModel(
    const NotifierId& notifier_id,
    const base::string16& display_source) {
  return tray_->CreateNotificationMenuModel(notifier_id, display_source);
}

bool MessageCenterView::HasClickedListener(const std::string& notification_id) {
  return message_center_->HasClickedListener(notification_id);
}

void MessageCenterView::ClickOnNotificationButton(
    const std::string& notification_id,
    int button_index) {
  message_center_->ClickOnNotificationButton(notification_id, button_index);
}

void MessageCenterView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());

  Visibility visibility = target_view_ == settings_view_
                              ? VISIBILITY_SETTINGS
                              : VISIBILITY_MESSAGE_CENTER;
  message_center_->SetVisibility(visibility);

  source_view_->SetVisible(false);
  target_view_->SetVisible(true);
  if (source_view_->layer())
    source_view_->layer()->SetOpacity(1.0);
  if (target_view_->layer())
    target_view_->layer()->SetOpacity(1.0);
  settings_transition_animation_.reset();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  PreferredSizeChanged();
  if (settings_transition_animation_->current_part_index() == 1 &&
      source_view_->layer()) {
    source_view_->layer()->SetOpacity(
        1.0 - settings_transition_animation_->GetCurrentValue());
    SchedulePaint();
  } else if (settings_transition_animation_->current_part_index() == 2 &&
             target_view_->layer()) {
    target_view_->layer()->SetOpacity(
        settings_transition_animation_->GetCurrentValue());
    SchedulePaint();
  }
}

void MessageCenterView::AnimationCanceled(const gfx::Animation* animation) {
  DCHECK_EQ(animation, settings_transition_animation_.get());
  AnimationEnded(animation);
}

void MessageCenterView::AddNotificationAt(const Notification& notification,
                                          int index) {
  NotificationView* view =
      NotificationView::Create(this, notification, false);  // Not top-level.
  view->set_context_menu_controller(context_menu_controller_.get());
  notification_views_[notification.id()] = view;
  view->set_scroller(scroller_);
  message_list_view_->AddNotificationAt(view, index);
}

void MessageCenterView::NotificationsChanged() {
  bool no_message_views = notification_views_.empty();

  // When the child view is removed from the hierarchy, its focus is cleared.
  // In this case we want to save which view has focus so that the user can
  // continue to interact with notifications in the order they were expecting.
  views::FocusManager* focus_manager = scroller_->GetFocusManager();
  View* focused_view = NULL;
  // |focus_manager| can be NULL in tests.
  if (focus_manager)
    focused_view = focus_manager->GetFocusedView();

  // All the children of this view are owned by |this|.
  scroller_->contents()->RemoveAllChildViews(/*delete_children=*/false);
  scroller_->contents()->AddChildView(
      no_message_views ? empty_list_view_.get() : message_list_view_.get());

  button_bar_->SetCloseAllButtonEnabled(!no_message_views);
  scroller_->SetFocusable(!no_message_views);

  if (focus_manager && focused_view)
    focus_manager->SetFocusedView(focused_view);

  scroller_->InvalidateLayout();
  PreferredSizeChanged();
  Layout();
}

void MessageCenterView::SetNotificationViewForTest(MessageView* view) {
  message_list_view_->AddNotificationAt(view, 0);
}

}  // namespace message_center
