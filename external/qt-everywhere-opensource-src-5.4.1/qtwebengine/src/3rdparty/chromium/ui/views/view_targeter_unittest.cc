// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_targeter.h"

#include "ui/events/event_targeter.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/path.h"
#include "ui/views/masked_view_targeter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/root_view.h"

namespace views {

// A class used to define a triangular-shaped hit test mask on a View.
class TestMaskedViewTargeter : public MaskedViewTargeter {
 public:
  explicit TestMaskedViewTargeter(View* masked_view)
      : MaskedViewTargeter(masked_view) {}
  virtual ~TestMaskedViewTargeter() {}

 private:
  virtual bool GetHitTestMask(const View* view,
                              gfx::Path* mask) const OVERRIDE {
    SkScalar w = SkIntToScalar(view->width());
    SkScalar h = SkIntToScalar(view->height());

    // Create a triangular mask within the bounds of |view|.
    mask->moveTo(w / 2, 0);
    mask->lineTo(w, h);
    mask->lineTo(0, h);
    mask->close();

    return true;
  }

  DISALLOW_COPY_AND_ASSIGN(TestMaskedViewTargeter);
};

// A derived class of View used for testing purposes.
class TestingView : public View {
 public:
  TestingView() : can_process_events_within_subtree_(true) {}
  virtual ~TestingView() {}

  // Reset all test state.
  void Reset() { can_process_events_within_subtree_ = true; }

  void set_can_process_events_within_subtree(bool can_process) {
    can_process_events_within_subtree_ = can_process;
  }

  // View:
  virtual bool CanProcessEventsWithinSubtree() const OVERRIDE {
    return can_process_events_within_subtree_;
  }

 private:
  // Value to return from CanProcessEventsWithinSubtree().
  bool can_process_events_within_subtree_;

  DISALLOW_COPY_AND_ASSIGN(TestingView);
};

namespace test {

typedef ViewsTestBase ViewTargeterTest;

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for key events.
TEST_F(ViewTargeterTest, ViewTargeterForKeyEvents) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget.Init(init_params);

  View* content = new View;
  View* child = new View;
  View* grandchild = new View;

  widget.SetContentsView(content);
  content->AddChildView(child);
  child->AddChildView(grandchild);

  grandchild->SetFocusable(true);
  grandchild->RequestFocus();

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);

  // The focused view should be the initial target of the event.
  ui::EventTarget* current_target = targeter->FindTargetForEvent(root_view,
                                                                 &key_event);
  EXPECT_EQ(grandchild, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(grandchild, &key_event);
  EXPECT_EQ(child, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(child, &key_event);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &key_event);
  EXPECT_EQ(widget.GetRootView(), static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(widget.GetRootView(),
                                                &key_event);
  EXPECT_EQ(NULL, static_cast<View*>(current_target));
}

// Verifies that the the functions ViewTargeter::FindTargetForEvent()
// and ViewTargeter::FindNextBestTarget() are implemented correctly
// for scroll events.
TEST_F(ViewTargeterTest, ViewTargeterForScrollEvents) {
  Widget widget;
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  init_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  init_params.bounds = gfx::Rect(0, 0, 200, 200);
  widget.Init(init_params);

  // The coordinates used for SetBounds() are in the parent coordinate space.
  View* content = new View;
  content->SetBounds(0, 0, 100, 100);
  View* child = new View;
  child->SetBounds(50, 50, 20, 20);
  View* grandchild = new View;
  grandchild->SetBounds(0, 0, 5, 5);

  widget.SetContentsView(content);
  content->AddChildView(child);
  child->AddChildView(grandchild);

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  // The event falls within the bounds of |child| and |content| but not
  // |grandchild|, so |child| should be the initial target for the event.
  ui::ScrollEvent scroll(ui::ET_SCROLL,
                         gfx::Point(60, 60),
                         ui::EventTimeForNow(),
                         0,
                         0, 3,
                         0, 3,
                         2);
  ui::EventTarget* current_target = targeter->FindTargetForEvent(root_view,
                                                                 &scroll);
  EXPECT_EQ(child, static_cast<View*>(current_target));

  // Verify that FindNextBestTarget() will return the parent view of the
  // argument (and NULL if the argument has no parent view).
  current_target = targeter->FindNextBestTarget(child, &scroll);
  EXPECT_EQ(content, static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(content, &scroll);
  EXPECT_EQ(widget.GetRootView(), static_cast<View*>(current_target));
  current_target = targeter->FindNextBestTarget(widget.GetRootView(),
                                                &scroll);
  EXPECT_EQ(NULL, static_cast<View*>(current_target));

  // The event falls outside of the original specified bounds of |content|,
  // |child|, and |grandchild|. But since |content| is the contents view,
  // and contents views are resized to fill the entire area of the root
  // view, the event's initial target should still be |content|.
  scroll = ui::ScrollEvent(ui::ET_SCROLL,
                           gfx::Point(150, 150),
                           ui::EventTimeForNow(),
                           0,
                           0, 3,
                           0, 3,
                           2);
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(content, static_cast<View*>(current_target));
}

// Tests the basic functionality of the method
// ViewTargeter::SubtreeShouldBeExploredForEvent().
TEST_F(ViewTargeterTest, SubtreeShouldBeExploredForEvent) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget.Init(params);

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  View v1, v2, v3;
  v1.SetBounds(0, 0, 300, 300);
  v2.SetBounds(100, 100, 100, 100);
  v3.SetBounds(0, 0, 10, 10);
  v3.SetVisible(false);
  root_view->AddChildView(&v1);
  v1.AddChildView(&v2);
  v2.AddChildView(&v3);

  // Note that the coordinates used below are in |v1|'s coordinate space,
  // and that SubtreeShouldBeExploredForEvent() expects the event location
  // to be in the coordinate space of the target's parent. |v1| and
  // its parent share a common coordinate space.

  // Event located within |v1| only.
  gfx::Point point(10, 10);
  ui::MouseEvent event(ui::ET_MOUSE_PRESSED, point, point,
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // Event located within |v1| and |v2| only.
  event.set_location(gfx::Point(150, 150));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // Event located within |v1|, |v2|, and |v3|. Note that |v3| is not
  // visible, so it cannot handle the event.
  event.set_location(gfx::Point(105, 105));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_TRUE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // Event located outside the bounds of all views.
  event.set_location(gfx::Point(400, 400));
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v1, event));
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v2, event));
  v1.ConvertEventToTarget(&v2, &event);
  EXPECT_FALSE(targeter->SubtreeShouldBeExploredForEvent(&v3, event));

  // TODO(tdanderson): Move the hit-testing unit tests out of view_unittest
  // and into here. See crbug.com/355425.
}

// Tests that FindTargetForEvent() returns the correct target when some
// views in the view tree return false when CanProcessEventsWithinSubtree()
// is called on them.
TEST_F(ViewTargeterTest, CanProcessEventsWithinSubtree) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget.Init(params);

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  TestingView v1, v2, v3;
  v1.SetBounds(0, 0, 300, 300);
  v2.SetBounds(100, 100, 100, 100);
  v3.SetBounds(0, 0, 10, 10);
  root_view->AddChildView(&v1);
  v1.AddChildView(&v2);
  v2.AddChildView(&v3);

  // Note that the coordinates used below are in the coordinate space of
  // the root view.

  // Define |scroll| to be (105, 105) (in the coordinate space of the root
  // view). This is located within all of |v1|, |v2|, and |v3|.
  gfx::Point scroll_point(105, 105);
  ui::ScrollEvent scroll(
      ui::ET_SCROLL, scroll_point, ui::EventTimeForNow(), 0, 0, 3, 0, 3, 2);

  // If CanProcessEventsWithinSubtree() returns true for each view,
  // |scroll| should be targeted at the deepest view in the hierarchy,
  // which is |v3|.
  ui::EventTarget* current_target =
      targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(&v3, current_target);

  // If CanProcessEventsWithinSubtree() returns |false| when called
  // on |v3|, then |v3| cannot be the target of |scroll| (this should
  // instead be |v2|). Note we need to reset the location of |scroll|
  // because it may have been mutated by the previous call to
  // FindTargetForEvent().
  scroll.set_location(scroll_point);
  v3.set_can_process_events_within_subtree(false);
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(&v2, current_target);

  // If CanProcessEventsWithinSubtree() returns |false| when called
  // on |v2|, then neither |v2| nor |v3| can be the target of |scroll|
  // (this should instead be |v1|).
  scroll.set_location(scroll_point);
  v3.Reset();
  v2.set_can_process_events_within_subtree(false);
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(&v1, current_target);

  // If CanProcessEventsWithinSubtree() returns |false| when called
  // on |v1|, then none of |v1|, |v2| or |v3| can be the target of |scroll|
  // (this should instead be the root view itself).
  scroll.set_location(scroll_point);
  v2.Reset();
  v1.set_can_process_events_within_subtree(false);
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(root_view, current_target);

  // TODO(tdanderson): We should also test that targeting works correctly
  //                   with gestures. See crbug.com/375822.
}

// Tests that FindTargetForEvent() returns the correct target when some
// views in the view tree have a MaskedViewTargeter installed, i.e.,
// they have a custom-shaped hit test mask.
TEST_F(ViewTargeterTest, MaskedViewTargeter) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 650, 650);
  widget.Init(params);

  ui::EventTargeter* targeter = new ViewTargeter();
  internal::RootView* root_view =
      static_cast<internal::RootView*>(widget.GetRootView());
  root_view->SetEventTargeter(make_scoped_ptr(targeter));

  // The coordinates used for SetBounds() are in the parent coordinate space.
  View masked_view, unmasked_view, masked_child;
  masked_view.SetBounds(0, 0, 200, 200);
  unmasked_view.SetBounds(300, 0, 300, 300);
  masked_child.SetBounds(0, 0, 100, 100);
  root_view->AddChildView(&masked_view);
  root_view->AddChildView(&unmasked_view);
  unmasked_view.AddChildView(&masked_child);

  // Install event targeters of type TestMaskedViewTargeter on the two masked
  // views to define their hit test masks.
  ui::EventTargeter* masked_targeter = new TestMaskedViewTargeter(&masked_view);
  masked_view.SetEventTargeter(make_scoped_ptr(masked_targeter));
  masked_targeter = new TestMaskedViewTargeter(&masked_child);
  masked_child.SetEventTargeter(make_scoped_ptr(masked_targeter));

  // Note that the coordinates used below are in the coordinate space of
  // the root view.

  // Event located within the hit test mask of |masked_view|.
  ui::ScrollEvent scroll(ui::ET_SCROLL,
                         gfx::Point(100, 190),
                         ui::EventTimeForNow(),
                         0,
                         0,
                         3,
                         0,
                         3,
                         2);
  ui::EventTarget* current_target =
      targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(&masked_view, static_cast<View*>(current_target));

  // Event located outside the hit test mask of |masked_view|.
  scroll.set_location(gfx::Point(10, 10));
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(root_view, static_cast<View*>(current_target));

  // Event located within the hit test mask of |masked_child|.
  scroll.set_location(gfx::Point(350, 3));
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(&masked_child, static_cast<View*>(current_target));

  // Event located within the hit test mask of |masked_child|.
  scroll.set_location(gfx::Point(300, 12));
  current_target = targeter->FindTargetForEvent(root_view, &scroll);
  EXPECT_EQ(&unmasked_view, static_cast<View*>(current_target));

  // TODO(tdanderson): We should also test that targeting of masked views
  //                   works correctly with gestures. See crbug.com/375822.
}

}  // namespace test
}  // namespace views
