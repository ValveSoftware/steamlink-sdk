// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_

#include <list>
#include <map>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/toast_contents_view.h"
#include "ui/views/widget/widget_observer.h"

namespace base {
class RunLoop;
}

namespace views {
class Widget;
}

namespace ash {
class WebNotificationTrayTest;
FORWARD_DECLARE_TEST(WebNotificationTrayTest, ManyPopupNotifications);
}

namespace gfx {
class Screen;
}

namespace message_center {
namespace test {
class MessagePopupCollectionTest;
}

class MessageCenter;
class MessageCenterTray;
class MessageViewContextMenuController;

enum PopupAlignment {
  POPUP_ALIGNMENT_TOP = 1 << 0,
  POPUP_ALIGNMENT_LEFT = 1 << 1,
  POPUP_ALIGNMENT_BOTTOM = 1 << 2,
  POPUP_ALIGNMENT_RIGHT = 1 << 3,
};

// Container for popup toasts. Because each toast is a frameless window rather
// than a view in a bubble, now the container just manages all of those toasts.
// This is similar to chrome/browser/notifications/balloon_collection, but the
// contents of each toast are for the message center and layout strategy would
// be slightly different.
class MESSAGE_CENTER_EXPORT MessagePopupCollection
    : public MessageCenterController,
      public MessageCenterObserver,
      public gfx::DisplayObserver {
 public:
  // |parent| specifies the parent widget of the toast windows. The default
  // parent will be used for NULL. Usually each icon is spacing against its
  // predecessor. If |first_item_has_no_margin| is set however the first item
  // does not space against the tray.
  MessagePopupCollection(gfx::NativeView parent,
                         MessageCenter* message_center,
                         MessageCenterTray* tray,
                         bool first_item_has_no_margin);
  virtual ~MessagePopupCollection();

  // Overridden from MessageCenterController:
  virtual void ClickOnNotification(const std::string& notification_id) OVERRIDE;
  virtual void RemoveNotification(const std::string& notification_id,
                                  bool by_user) OVERRIDE;
  virtual scoped_ptr<ui::MenuModel> CreateMenuModel(
      const NotifierId& notifier_id,
      const base::string16& display_source) OVERRIDE;
  virtual bool HasClickedListener(const std::string& notification_id) OVERRIDE;
  virtual void ClickOnNotificationButton(const std::string& notification_id,
                                         int button_index) OVERRIDE;

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

  // Updates |work_area_| and re-calculates the alignment of notification toasts
  // rearranging them if necessary.
  // This is separated from methods from OnDisplayMetricsChanged(), since
  // sometimes the display info has to be specified directly. One example is
  // shelf's auto-hide change. When the shelf in ChromeOS is temporarily shown
  // from auto hide status, it doesn't change the display's work area but the
  // actual work area for toasts should be resized.
  void SetDisplayInfo(const gfx::Rect& work_area,
                      const gfx::Rect& screen_bounds);

  // Overridden from gfx::DislayObserver:
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

  // Used by ToastContentsView to locate itself.
  gfx::NativeView parent() const { return parent_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ash::WebNotificationTrayTest,
                           ManyPopupNotifications);
  friend class test::MessagePopupCollectionTest;
  friend class ash::WebNotificationTrayTest;
  typedef std::list<ToastContentsView*> Toasts;

  // Iterates toasts and starts closing them.
  std::set<std::string> CloseAllWidgets();

  // Called by ToastContentsView when its window is closed.
  void RemoveToast(ToastContentsView* toast, bool mark_as_shown);

  // Returns the x-origin for the given toast bounds in the current work area.
  int GetToastOriginX(const gfx::Rect& toast_bounds) const;

  // Creates new widgets for new toast notifications, and updates |toasts_| and
  // |widgets_| correctly.
  void UpdateWidgets();

  // Repositions all of the widgets based on the current work area.
  void RepositionWidgets();

  // Repositions widgets to the top edge of the notification toast that was
  // just removed, so that the user can click close button without mouse moves.
  // See crbug.com/224089
  void RepositionWidgetsWithTarget();

  void ComputePopupAlignment(gfx::Rect work_area, gfx::Rect screen_bounds);

  // The base line is an (imaginary) line that would touch the bottom of the
  // next created notification if bottom-aligned or its top if top-aligned.
  int GetBaseLine(ToastContentsView* last_toast) const;

  // Overridden from MessageCenterObserver:
  virtual void OnNotificationAdded(const std::string& notification_id) OVERRIDE;
  virtual void OnNotificationRemoved(const std::string& notification_id,
                                     bool by_user) OVERRIDE;
  virtual void OnNotificationUpdated(
      const std::string& notification_id) OVERRIDE;

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

  gfx::NativeView parent_;
  MessageCenter* message_center_;
  MessageCenterTray* tray_;
  Toasts toasts_;
  gfx::Rect work_area_;
  int64 display_id_;
  gfx::Screen* screen_;

  // Specifies which corner of the screen popups should show up. This should
  // ideally be the same corner the notification area (systray) is at.
  PopupAlignment alignment_;

  int defer_counter_;

  // This is only used to compare with incoming events, do not assume that
  // the toast will be valid if this pointer is non-NULL.
  ToastContentsView* latest_toast_entered_;

  // Denotes a mode when user is clicking the Close button of toasts in a
  // sequence, w/o moving the mouse. We reposition the toasts so the next one
  // happens to be right under the mouse, and the user can just dispose of
  // multipel toasts by clicking. The mode ends when defer_timer_ expires.
  bool user_is_closing_toasts_by_clicking_;
  scoped_ptr<base::OneShotTimer<MessagePopupCollection> > defer_timer_;
  // The top edge to align the position of the next toast during 'close by
  // clicking" mode.
  // Only to be used when user_is_closing_toasts_by_clicking_ is true.
  int target_top_edge_;

  // Weak, only exists temporarily in tests.
  scoped_ptr<base::RunLoop> run_loop_for_test_;

  // True if the first item should not have spacing against the tray.
  bool first_item_has_no_margin_;

  scoped_ptr<MessageViewContextMenuController> context_menu_controller_;

  // Gives out weak pointers to toast contents views which have an unrelated
  // lifetime.  Must remain the last member variable.
  base::WeakPtrFactory<MessagePopupCollection> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessagePopupCollection);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_MESSAGE_POPUP_COLLECTION_H_
