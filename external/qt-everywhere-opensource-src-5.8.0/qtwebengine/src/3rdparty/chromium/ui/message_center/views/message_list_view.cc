// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/views/message_center_view.h"
#include "ui/message_center/views/message_list_view.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {
const int kAnimateClearingNextNotificationDelayMS = 40;
}  // namespace

MessageListView::MessageListView(MessageCenterView* message_center_view,
                                 bool top_down)
    : message_center_view_(message_center_view),
      reposition_top_(-1),
      fixed_height_(0),
      has_deferred_task_(false),
      clear_all_started_(false),
      top_down_(top_down),
      animator_(this),
      quit_message_loop_after_animation_for_test_(false),
      weak_ptr_factory_(this) {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
  layout->SetDefaultFlex(1);
  SetLayoutManager(layout);

  // Set the margin to 0 for the layout. BoxLayout assumes the same margin
  // for top and bottom, but the bottom margin here should be smaller
  // because of the shadow of message view. Use an empty border instead
  // to provide this margin.
  gfx::Insets shadow_insets = MessageView::GetShadowInsets();
  set_background(
      views::Background::CreateSolidBackground(kMessageCenterBackgroundColor));
  SetBorder(views::Border::CreateEmptyBorder(
      top_down ? 0 : kMarginBetweenItems - shadow_insets.top(),    /* top */
      kMarginBetweenItems - shadow_insets.left(),                  /* left */
      top_down ? kMarginBetweenItems - shadow_insets.bottom() : 0, /* bottom */
      kMarginBetweenItems - shadow_insets.right() /* right */));
  animator_.AddObserver(this);
}

MessageListView::~MessageListView() {
  animator_.RemoveObserver(this);
}

void MessageListView::Layout() {
  if (animator_.IsAnimating())
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
      animator_.StopAnimatingView(view);
      delete view;
    }
    DoUpdateIfPossible();
  }
}

void MessageListView::UpdateNotification(MessageView* view,
                                         const Notification& notification) {
  int index = GetIndexOf(view);
  DCHECK_LE(0, index);  // GetIndexOf is negative if not a child.

  animator_.StopAnimatingView(view);
  if (deleting_views_.find(view) != deleting_views_.end())
    deleting_views_.erase(view);
  if (deleted_when_done_.find(view) != deleted_when_done_.end())
    deleted_when_done_.erase(view);
  view->UpdateWithNotification(notification);
  DoUpdateIfPossible();
}

gfx::Size MessageListView::GetPreferredSize() const {
  // Just returns the current size. All size change must be done in
  // |DoUpdateIfPossible()| with animation , because we don't want to change
  // the size in unexpected timing.
  return size();
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

void MessageListView::PaintChildren(const ui::PaintContext& context) {
  // Paint in the inversed order. Otherwise upper notification may be
  // hidden by the lower one.
  for (int i = child_count() - 1; i >= 0; --i) {
    if (!child_at(i)->layer())
      child_at(i)->Paint(context);
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
  reposition_top_ = std::max(target.y(), 0);
  fixed_height_ = GetHeightForWidth(width());
}

void MessageListView::ResetRepositionSession() {
  // Don't call DoUpdateIfPossible(), but let Layout() do the task without
  // animation. Reset will cause the change of the bubble size itself, and
  // animation from the old location will look weird.
  if (reposition_top_ >= 0) {
    has_deferred_task_ = false;
    // cancel cause OnBoundsAnimatorDone which deletes |deleted_when_done_|.
    animator_.Cancel();
    STLDeleteContainerPointers(deleting_views_.begin(), deleting_views_.end());
    deleting_views_.clear();
    adding_views_.clear();
  }

  reposition_top_ = -1;
  fixed_height_ = 0;
}

void MessageListView::ClearAllClosableNotifications(
    const gfx::Rect& visible_scroll_rect) {
  for (int i = 0; i < child_count(); ++i) {
    // Safe cast since all views in MessageListView are MessageViews.
    MessageView* child = (MessageView*)child_at(i);
    if (!child->visible())
      continue;
    if (gfx::IntersectRects(child->bounds(), visible_scroll_rect).IsEmpty())
      continue;
    if (child->IsPinned())
      continue;
    clearing_all_views_.push_back(child);
  }
  if (clearing_all_views_.empty()) {
    message_center_view()->OnAllNotificationsCleared();
  } else {
    DoUpdateIfPossible();
  }
}

void MessageListView::OnBoundsAnimatorProgressed(
    views::BoundsAnimator* animator) {
  DCHECK_EQ(&animator_, animator);
  for (std::set<views::View*>::iterator iter = deleted_when_done_.begin();
       iter != deleted_when_done_.end(); ++iter) {
    const gfx::SlideAnimation* animation = animator->GetAnimationForView(*iter);
    if (animation)
      (*iter)->layer()->SetOpacity(animation->CurrentValueBetween(1.0, 0.0));
  }
}

void MessageListView::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  STLDeleteContainerPointers(deleted_when_done_.begin(),
                             deleted_when_done_.end());
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

  if (quit_message_loop_after_animation_for_test_)
    base::MessageLoop::current()->QuitWhenIdle();
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

  if (animator_.IsAnimating()) {
    has_deferred_task_ = true;
    return;
  }

  if (!clearing_all_views_.empty()) {
    AnimateClearingOneNotification();
    return;
  }

  int new_height = GetHeightForWidth(child_area.width() + GetInsets().width());
  SetSize(gfx::Size(child_area.width() + GetInsets().width(), new_height));

  if (top_down_ ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMessageCenterAlwaysScrollUpUponNotificationRemoval))
    AnimateNotificationsBelowTarget();
  else
    AnimateNotificationsAboveTarget();

  adding_views_.clear();
  deleting_views_.clear();

  if (!animator_.IsAnimating() && GetWidget())
    GetWidget()->SynthesizeMouseMoveEvent();
}

void MessageListView::AnimateNotificationsBelowTarget() {
  int target_index = -1;
  int padding = kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
  gfx::Rect child_area = GetContentsBounds();
  if (reposition_top_ >= 0) {
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      if (child->y() >= reposition_top_) {
        // Find the target.
        target_index = i;
        break;
      }
    }
  }
  int top;
  if (target_index != -1) {
    // Layout the target.
    int y = reposition_top_;
    views::View* target = child_at(target_index);
    int target_height = target->GetHeightForWidth(child_area.width());
    if (AnimateChild(target, y - target_height, target_height,
                     false /* animate_on_move */)) {
      y -= target_height + padding;
    }

    // Layout the items above the target.
    for (int i = target_index - 1; i >= 0; --i) {
      views::View* child = child_at(i);
      int height = child->GetHeightForWidth(child_area.width());
      if (AnimateChild(child, y - height, height, false /* animate_on_move */))
        y -= height + padding;
    }

    top = reposition_top_ + target_height + padding;
  } else {
    target_index = -1;
    top = GetInsets().top();
  }

  // Layout the items below the target (or all items if target is unavailable).
  for (int i = target_index + 1; i < child_count(); ++i) {
    views::View* child = child_at(i);
    int height = child->GetHeightForWidth(child_area.width());
    if (AnimateChild(child, top, height, true /* animate_on_move */))
      top += height + padding;
  }
}

void MessageListView::AnimateNotificationsAboveTarget() {
  int target_index = -1;
  int padding = kMarginBetweenItems - MessageView::GetShadowInsets().bottom();
  gfx::Rect child_area = GetContentsBounds();
  if (reposition_top_ >= 0) {
    // Find the target item.
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      if (child->y() >= reposition_top_ &&
          deleting_views_.find(child) == deleting_views_.end()) {
        // Find the target.
        target_index = i;
        break;
      }
    }
    // If no items are below |reposition_top_|, use the last item as the target.
    if (target_index == -1) {
      target_index = child_count() - 1;
      for (; target_index != -1; target_index--) {
        views::View* target_view = child_at(target_index);
        if (deleting_views_.find(target_view) == deleting_views_.end())
          break;
      }
    }
  }
  if (target_index != -1) {
    // Cache for the heights of items, since calculating height is heavy
    // operation and the heights shouldn't be changed in this block.
    std::map<views::View*, int> height_cache;

    // Calculate the vertical length between the top of message list and the top
    // of target. This is to shrink or expand the height of the message list
    // when the notifications above the target is changed.
    int vertical_gap_to_target_from_top = GetInsets().height();
    for (int i = 0; i < target_index; i++) {
      views::View* child = child_at(i);
      int height = child->GetHeightForWidth(child_area.width());
      height_cache[child] = height;
      if (deleting_views_.find(child) == deleting_views_.end())
        vertical_gap_to_target_from_top += height + padding;
    }

    // If the calculated length is changed from |repositon_top_|, it means that
    // some of items above the targe are updated and their height are changed.
    // Adjust the vertical length above the target.
    if (reposition_top_ != vertical_gap_to_target_from_top) {
      fixed_height_ -= reposition_top_ - vertical_gap_to_target_from_top;
      reposition_top_ = vertical_gap_to_target_from_top;
    }

    // Match the top with |reposition_top_|.
    int y = reposition_top_;
    // Layout the target and the items below the target.
    for (int i = target_index; i < child_count(); i++) {
      views::View* child = child_at(i);
      int height = child->GetHeightForWidth(child_area.width());
      if (AnimateChild(child, y, height, false /* animate_on_move */))
        y += height + padding;
    }

    // Layout the items above the target.
    y = GetInsets().top();
    for (int i = 0; i < target_index; i++) {
      views::View* child = child_at(i);
      DCHECK(height_cache.find(child) != height_cache.end());
      int height = height_cache[child];
      if (AnimateChild(child, y, height, true /* animate_on_move */))
        y += height + padding;
    }

    DCHECK_EQ(y, reposition_top_);
  } else {
    // Layout all the items.
    int y = GetInsets().top();
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      int height = child->GetHeightForWidth(child_area.width());
      if (AnimateChild(child, y, height, true))
        y += height + padding;
    }
    fixed_height_ = y - padding + GetInsets().bottom();
  }
}

bool MessageListView::AnimateChild(views::View* child,
                                   int top,
                                   int height,
                                   bool animate_on_move) {
  gfx::Rect child_area = GetContentsBounds();
  if (adding_views_.find(child) != adding_views_.end()) {
    child->SetBounds(child_area.right(), top, child_area.width(), height);
    animator_.AnimateViewTo(
        child, gfx::Rect(child_area.x(), top, child_area.width(), height));
  } else if (deleting_views_.find(child) != deleting_views_.end()) {
    DCHECK(child->layer());
    // No moves, but animate to fade-out.
    animator_.AnimateViewTo(child, child->bounds());
    deleted_when_done_.insert(child);
    return false;
  } else {
    gfx::Rect target(child_area.x(), top, child_area.width(), height);
    if (child->bounds().origin() != target.origin() && animate_on_move)
      animator_.AnimateViewTo(child, target);
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
  animator_.AnimateViewTo(child, new_bounds);

  // Schedule to start sliding out next notification after a short delay.
  if (!clearing_all_views_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&MessageListView::AnimateClearingOneNotification,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(
            kAnimateClearingNextNotificationDelayMS));
  }
}

void MessageListView::SetRepositionTargetForTest(const gfx::Rect& target_rect) {
  SetRepositionTarget(target_rect);
}

}  // namespace message_center
