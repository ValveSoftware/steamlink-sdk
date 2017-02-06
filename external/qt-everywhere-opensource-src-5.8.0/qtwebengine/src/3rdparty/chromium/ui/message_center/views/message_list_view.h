// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_LIST_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_LIST_VIEW_H_

#include <list>
#include <set>

#include "base/macros.h"
#include "ui/compositor/paint_context.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/bounds_animator_observer.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace ui {
class Layer;
}

namespace message_center {

class MessageCenterView;
class MessageView;

// Displays a list of messages for rich notifications. Functions as an array of
// MessageViews and animates them on transitions. It also supports
// repositioning.
class MessageListView : public views::View,
                        public views::BoundsAnimatorObserver {
 public:
  explicit MessageListView(MessageCenterView* message_center_view,
                           bool top_down);
  ~MessageListView() override;

  void AddNotificationAt(MessageView* view, int i);
  void RemoveNotification(MessageView* view);
  void UpdateNotification(MessageView* view, const Notification& notification);
  void SetRepositionTarget(const gfx::Rect& target_rect);
  void ResetRepositionSession();
  void ClearAllClosableNotifications(const gfx::Rect& visible_scroll_rect);

  MESSAGE_CENTER_EXPORT void SetRepositionTargetForTest(
      const gfx::Rect& target_rect);

 protected:
  // Overridden from views::View.
  void Layout() override;
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void PaintChildren(const ui::PaintContext& context) override;
  void ReorderChildLayers(ui::Layer* parent_layer) override;

  // Overridden from views::BoundsAnimatorObserver.
  void OnBoundsAnimatorProgressed(views::BoundsAnimator* animator) override;
  void OnBoundsAnimatorDone(views::BoundsAnimator* animator) override;

 private:
  friend class MessageCenterViewTest;

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
  bool AnimateChild(views::View* child,
                    int top,
                    int height,
                    bool animate_even_on_move);

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
  views::BoundsAnimator animator_;

  // If true, the message loop will be quitted after the animation finishes.
  // This is just for tests and has no setter.
  bool quit_message_loop_after_animation_for_test_;

  base::WeakPtrFactory<MessageListView> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(MessageListView);
};

}  // namespace message_center
#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_LIST_VIEW_H_
