// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_popup_collection.h"

#include <list>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/views/toast_contents_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {
namespace test {

class MessagePopupCollectionTest : public views::ViewsTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    views::ViewsTestBase::SetUp();
    MessageCenter::Initialize();
    MessageCenter::Get()->DisableTimersForTest();
    collection_.reset(new MessagePopupCollection(
        GetContext(), MessageCenter::Get(), NULL, false));
    // This size fits test machines resolution and also can keep a few toasts
    // w/o ill effects of hitting the screen overflow. This allows us to assume
    // and verify normal layout of the toast stack.
    collection_->SetDisplayInfo(gfx::Rect(0, 0, 600, 390),
                                gfx::Rect(0, 0, 600, 400));  // Simulate a
                                                             // taskbar at the
                                                             // bottom.
    id_ = 0;
    PrepareForWait();
  }

  virtual void TearDown() OVERRIDE {
    collection_.reset();
    MessageCenter::Shutdown();
    views::ViewsTestBase::TearDown();
  }

 protected:
  MessagePopupCollection* collection() { return collection_.get(); }

  size_t GetToastCounts() {
    return collection_->toasts_.size();
  }

  bool MouseInCollection() {
    return collection_->latest_toast_entered_ != NULL;
  }

  bool IsToastShown(const std::string& id) {
    views::Widget* widget = collection_->GetWidgetForTest(id);
    return widget && widget->IsVisible();
  }

  views::Widget* GetWidget(const std::string& id) {
    return collection_->GetWidgetForTest(id);
  }

  gfx::Rect GetWorkArea() {
    return collection_->work_area_;
  }

  ToastContentsView* GetToast(const std::string& id) {
    for (MessagePopupCollection::Toasts::iterator iter =
             collection_->toasts_.begin();
         iter != collection_->toasts_.end(); ++iter) {
      if ((*iter)->id() == id)
        return *iter;
    }
    return NULL;
  }

  std::string AddNotification() {
    std::string id = base::IntToString(id_++);
    scoped_ptr<Notification> notification(
        new Notification(NOTIFICATION_TYPE_BASE_FORMAT,
                         id,
                         base::UTF8ToUTF16("test title"),
                         base::UTF8ToUTF16("test message"),
                         gfx::Image(),
                         base::string16() /* display_source */,
                         NotifierId(),
                         message_center::RichNotificationData(),
                         NULL /* delegate */));
    MessageCenter::Get()->AddNotification(notification.Pass());
    return id;
  }

  void PrepareForWait() { collection_->CreateRunLoopForTest(); }

  // Assumes there is non-zero pending work.
  void WaitForTransitionsDone() {
    collection_->WaitForTest();
    collection_->CreateRunLoopForTest();
  }

  void CloseAllToasts() {
    // Assumes there is at least one toast to close.
    EXPECT_TRUE(GetToastCounts() > 0);
    MessageCenter::Get()->RemoveAllNotifications(false);
  }

  gfx::Rect GetToastRectAt(size_t index) {
    return collection_->GetToastRectAt(index);
  }

 private:
  scoped_ptr<MessagePopupCollection> collection_;
  int id_;
};

TEST_F(MessagePopupCollectionTest, DismissOnClick) {

  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  WaitForTransitionsDone();

  EXPECT_EQ(2u, GetToastCounts());
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_TRUE(IsToastShown(id2));

  MessageCenter::Get()->ClickOnNotification(id2);
  WaitForTransitionsDone();

  EXPECT_EQ(1u, GetToastCounts());
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));

  MessageCenter::Get()->ClickOnNotificationButton(id1, 0);
  WaitForTransitionsDone();
  EXPECT_EQ(0u, GetToastCounts());
  EXPECT_FALSE(IsToastShown(id1));
  EXPECT_FALSE(IsToastShown(id2));
}

TEST_F(MessagePopupCollectionTest, ShutdownDuringShowing) {
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  WaitForTransitionsDone();
  EXPECT_EQ(2u, GetToastCounts());
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_TRUE(IsToastShown(id2));

  // Finish without cleanup of notifications, which may cause use-after-free.
  // See crbug.com/236448
  GetWidget(id1)->CloseNow();
  collection()->OnMouseExited(GetToast(id2));
}

TEST_F(MessagePopupCollectionTest, DefaultPositioning) {
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  std::string id2 = AddNotification();
  std::string id3 = AddNotification();
  WaitForTransitionsDone();

  gfx::Rect r0 = GetToastRectAt(0);
  gfx::Rect r1 = GetToastRectAt(1);
  gfx::Rect r2 = GetToastRectAt(2);
  gfx::Rect r3 = GetToastRectAt(3);

  // 3 toasts are shown, equal size, vertical stack.
  EXPECT_TRUE(IsToastShown(id0));
  EXPECT_TRUE(IsToastShown(id1));
  EXPECT_TRUE(IsToastShown(id2));

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r1.width(), r2.width());

  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_EQ(r1.height(), r2.height());

  EXPECT_GT(r0.y(), r1.y());
  EXPECT_GT(r1.y(), r2.y());

  EXPECT_EQ(r0.x(), r1.x());
  EXPECT_EQ(r1.x(), r2.x());

  // The 4th toast is not shown yet.
  EXPECT_FALSE(IsToastShown(id3));
  EXPECT_EQ(0, r3.width());
  EXPECT_EQ(0, r3.height());

  CloseAllToasts();
  EXPECT_EQ(0u, GetToastCounts());
}

TEST_F(MessagePopupCollectionTest, DefaultPositioningWithRightTaskbar) {
  // If taskbar is on the right we show the toasts bottom to top as usual.

  // Simulate a taskbar at the right.
  collection()->SetDisplayInfo(gfx::Rect(0, 0, 590, 400),   // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  gfx::Rect r0 = GetToastRectAt(0);
  gfx::Rect r1 = GetToastRectAt(1);

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_TRUE(IsToastShown(id0));
  EXPECT_TRUE(IsToastShown(id1));

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_GT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());

  CloseAllToasts();
  EXPECT_EQ(0u, GetToastCounts());

  // Restore simulated taskbar position to bottom.
  collection()->SetDisplayInfo(gfx::Rect(0, 0, 600, 390),  // Work-area.
                               gfx::Rect(0, 0, 600, 400)); // Display-bounds.
}

TEST_F(MessagePopupCollectionTest, TopDownPositioningWithTopTaskbar) {
  // Simulate a taskbar at the top.
  collection()->SetDisplayInfo(gfx::Rect(0, 10, 600, 390),  // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  gfx::Rect r0 = GetToastRectAt(0);
  gfx::Rect r1 = GetToastRectAt(1);

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_TRUE(IsToastShown(id0));
  EXPECT_TRUE(IsToastShown(id1));

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_LT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());

  CloseAllToasts();
  EXPECT_EQ(0u, GetToastCounts());

  // Restore simulated taskbar position to bottom.
  collection()->SetDisplayInfo(gfx::Rect(0, 0, 600, 390),   // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
}

TEST_F(MessagePopupCollectionTest, TopDownPositioningWithLeftAndTopTaskbar) {
  // If there "seems" to be a taskbar on left and top (like in Unity), it is
  // assumed that the actual taskbar is the top one.

  // Simulate a taskbar at the top and left.
  collection()->SetDisplayInfo(gfx::Rect(10, 10, 590, 390),  // Work-area.
                               gfx::Rect(0, 0, 600, 400));   // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  gfx::Rect r0 = GetToastRectAt(0);
  gfx::Rect r1 = GetToastRectAt(1);

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_TRUE(IsToastShown(id0));
  EXPECT_TRUE(IsToastShown(id1));

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_LT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());

  CloseAllToasts();
  EXPECT_EQ(0u, GetToastCounts());

  // Restore simulated taskbar position to bottom.
  collection()->SetDisplayInfo(gfx::Rect(0, 0, 600, 390),   // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
}

TEST_F(MessagePopupCollectionTest, TopDownPositioningWithBottomAndTopTaskbar) {
  // If there "seems" to be a taskbar on bottom and top (like in Gnome), it is
  // assumed that the actual taskbar is the top one.

  // Simulate a taskbar at the top and bottom.
  collection()->SetDisplayInfo(gfx::Rect(0, 10, 580, 400),  // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  gfx::Rect r0 = GetToastRectAt(0);
  gfx::Rect r1 = GetToastRectAt(1);

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_TRUE(IsToastShown(id0));
  EXPECT_TRUE(IsToastShown(id1));

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_LT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());

  CloseAllToasts();
  EXPECT_EQ(0u, GetToastCounts());

  // Restore simulated taskbar position to bottom.
  collection()->SetDisplayInfo(gfx::Rect(0, 0, 600, 390),   // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
}

TEST_F(MessagePopupCollectionTest, LeftPositioningWithLeftTaskbar) {
  // Simulate a taskbar at the left.
  collection()->SetDisplayInfo(gfx::Rect(10, 0, 590, 400),  // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  gfx::Rect r0 = GetToastRectAt(0);
  gfx::Rect r1 = GetToastRectAt(1);

  // 2 toasts are shown, equal size, vertical stack.
  EXPECT_TRUE(IsToastShown(id0));
  EXPECT_TRUE(IsToastShown(id1));

  EXPECT_EQ(r0.width(), r1.width());
  EXPECT_EQ(r0.height(), r1.height());
  EXPECT_GT(r0.y(), r1.y());
  EXPECT_EQ(r0.x(), r1.x());

  // Ensure that toasts are on the left.
  EXPECT_LT(r1.x(), GetWorkArea().CenterPoint().x());

  CloseAllToasts();
  EXPECT_EQ(0u, GetToastCounts());

  // Restore simulated taskbar position to bottom.
  collection()->SetDisplayInfo(gfx::Rect(0, 0, 600, 390),   // Work-area.
                               gfx::Rect(0, 0, 600, 400));  // Display-bounds.
}

TEST_F(MessagePopupCollectionTest, DetectMouseHover) {
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  views::WidgetDelegateView* toast0 = GetToast(id0);
  EXPECT_TRUE(toast0 != NULL);
  views::WidgetDelegateView* toast1 = GetToast(id1);
  EXPECT_TRUE(toast1 != NULL);

  ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(), 0, 0);

  // Test that mouse detection logic works in presence of out-of-order events.
  toast0->OnMouseEntered(event);
  EXPECT_TRUE(MouseInCollection());
  toast1->OnMouseEntered(event);
  EXPECT_TRUE(MouseInCollection());
  toast0->OnMouseExited(event);
  EXPECT_TRUE(MouseInCollection());
  toast1->OnMouseExited(event);
  EXPECT_FALSE(MouseInCollection());

  // Test that mouse detection logic works in presence of WindowClosing events.
  toast0->OnMouseEntered(event);
  EXPECT_TRUE(MouseInCollection());
  toast1->OnMouseEntered(event);
  EXPECT_TRUE(MouseInCollection());
  toast0->WindowClosing();
  EXPECT_TRUE(MouseInCollection());
  toast1->WindowClosing();
  EXPECT_FALSE(MouseInCollection());
}

// TODO(dimich): Test repositioning - both normal one and when user is closing
// the toasts.
TEST_F(MessagePopupCollectionTest, DetectMouseHoverWithUserClose) {
  std::string id0 = AddNotification();
  std::string id1 = AddNotification();
  WaitForTransitionsDone();

  views::WidgetDelegateView* toast0 = GetToast(id0);
  EXPECT_TRUE(toast0 != NULL);
  views::WidgetDelegateView* toast1 = GetToast(id1);
  ASSERT_TRUE(toast1 != NULL);

  ui::MouseEvent event(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(), 0, 0);
  toast1->OnMouseEntered(event);
  static_cast<MessageCenterObserver*>(collection())->OnNotificationRemoved(
      id1, true);

  EXPECT_FALSE(MouseInCollection());
  std::string id2 = AddNotification();

  WaitForTransitionsDone();
  views::WidgetDelegateView* toast2 = GetToast(id2);
  EXPECT_TRUE(toast2 != NULL);
}


}  // namespace test
}  // namespace message_center
