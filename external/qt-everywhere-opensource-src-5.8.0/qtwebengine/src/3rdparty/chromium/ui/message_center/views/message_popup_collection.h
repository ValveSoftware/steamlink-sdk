// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_

#include <stddef.h>

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/toast_contents_view.h"
#include "ui/views/widget/widget_observer.h"

namespace base {
class RunLoop;
}

namespace display {
class Display;
}

namespace views {
class Widget;
}

namespace message_center {
namespace test {
class MessagePopupCollectionTest;
}

class MessageCenter;
class MessageCenterTray;
class MessageViewContextMenuController;
class PopupAlignmentDelegate;

// Container for popup toasts. Because each toast is a frameless window rather
// than a view in a bubble, now the container just manages all of those toasts.
// This is similar to chrome/browser/notifications/balloon_collection, but the
// contents of each toast are for the message center and layout strategy would
// be slightly different.
class MESSAGE_CENTER_EXPORT MessagePopupCollection
    : public MessageCenterController,
      public MessageCenterObserver {
 public:
  MessagePopupCollection(MessageCenter* message_center,
                         MessageCenterTray* tray,
                         PopupAlignmentDelegate* alignment_delegate);
  ~MessagePopupCollection() override;

  // Overridden from MessageCenterController:
  void ClickOnNotification(const std::string& notification_id) override;
  void RemoveNotification(const std::string& notification_id,
                          bool by_user) override;
  std::unique_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) override;
  bool HasClickedListener(const std::string& notification_id) override;
  void ClickOnNotificationButton(const std::string& notification_id,
                                 int button_index) override;
  void ClickOnSettingsButton(const std::string& notification_id) override;

  void MarkAllPopupsShown();

  // Since these events are really coming from individual toast widgets,
  // it helps to be able to keep track of the sender.
  void OnMouseEntered(ToastContentsView* toast_entered);
  void OnMouseExited(ToastContentsView* toast_exited);

  // Invoked by toasts when they start/finish their animations.
  // While "defer counter" is greater then zero, the popup collection does
  // not perform updates. It is used to wait for various animations and user
  // actions like serial closing of the toasts, when the remaining toasts "flow
  // under the mouse".
  void IncrementDeferCounter();
  void DecrementDeferCounter();

  // Runs the next step in update/animate sequence, if the defer counter is not
  // zero. Otherwise, simply waits when it becomes zero.
  void DoUpdateIfPossible();

  // Removes the toast from our internal list of toasts; this is called when the
  // toast is irrevocably closed (such as within RemoveToast).
  void ForgetToast(ToastContentsView* toast);

  // Called when the display bounds has been changed. Used in Windows only.
  void OnDisplayMetricsChanged(const display::Display& display);

 private:
  friend class test::MessagePopupCollectionTest;
  typedef std::list<ToastContentsView*> Toasts;

  // Iterates toasts and starts closing them.
  std::set<std::string> CloseAllWidgets();

  // Called by ToastContentsView when its window is closed.
  void RemoveToast(ToastContentsView* toast, bool mark_as_shown);

  // Creates new widgets for new toast notifications, and updates |toasts_| and
  // |widgets_| correctly.
  void UpdateWidgets();

  // Repositions all of the widgets based on the current work area.
  void RepositionWidgets();

  // Repositions widgets to the top edge of the notification toast that was
  // just removed, so that the user can click close button without mouse moves.
  // See crbug.com/224089
  void RepositionWidgetsWithTarget();

  // The base line is an (imaginary) line that would touch the bottom of the
  // next created notification if bottom-aligned or its top if top-aligned.
  int GetBaseLine(ToastContentsView* last_toast) const;

  // Overridden from MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;
  void OnNotificationUpdated(const std::string& notification_id) override;

  ToastContentsView* FindToast(const std::string& notification_id) const;

  // While the toasts are animated, avoid updating the collection, to reduce
  // user confusion. Instead, update the collection when all animations are
  // done. This method is run when defer counter is zero, may initiate next
  // update/animation step.
  void OnDeferTimerExpired();

  // "ForTest" methods.
  views::Widget* GetWidgetForTest(const std::string& id) const;
  void CreateRunLoopForTest();
  void WaitForTest();
  gfx::Rect GetToastRectAt(size_t index) const;

  MessageCenter* message_center_;
  MessageCenterTray* tray_;
  Toasts toasts_;

  PopupAlignmentDelegate* alignment_delegate_;

  int defer_counter_;

  // This is only used to compare with incoming events, do not assume that
  // the toast will be valid if this pointer is non-NULL.
  ToastContentsView* latest_toast_entered_;

  // Denotes a mode when user is clicking the Close button of toasts in a
  // sequence, w/o moving the mouse. We reposition the toasts so the next one
  // happens to be right under the mouse, and the user can just dispose of
  // multipel toasts by clicking. The mode ends when defer_timer_ expires.
  bool user_is_closing_toasts_by_clicking_;
  std::unique_ptr<base::OneShotTimer> defer_timer_;
  // The top edge to align the position of the next toast during 'close by
  // clicking" mode.
  // Only to be used when user_is_closing_toasts_by_clicking_ is true.
  int target_top_edge_;

  // Weak, only exists temporarily in tests.
  std::unique_ptr<base::RunLoop> run_loop_for_test_;

  std::unique_ptr<MessageViewContextMenuController> context_menu_controller_;

  // Gives out weak pointers to toast contents views which have an unrelated
  // lifetime.  Must remain the last member variable.
  base::WeakPtrFactory<MessagePopupCollection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessagePopupCollection);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
