// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/transform.h"
#include "ui/views/background.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

using base::ASCIIToUTF16;

namespace {

// Returns true if |ancestor| is an ancestor of |layer|.
bool LayerIsAncestor(const ui::Layer* ancestor, const ui::Layer* layer) {
  while (layer && layer != ancestor)
    layer = layer->parent();
  return layer == ancestor;
}

// Convenience functions for walking a View tree.
const views::View* FirstView(const views::View* view) {
  const views::View* v = view;
  while (v->has_children())
    v = v->child_at(0);
  return v;
}

const views::View* NextView(const views::View* view) {
  const views::View* v = view;
  const views::View* parent = v->parent();
  if (!parent)
    return NULL;
  int next = parent->GetIndexOf(v) + 1;
  if (next != parent->child_count())
    return FirstView(parent->child_at(next));
  return parent;
}

// Convenience functions for walking a Layer tree.
const ui::Layer* FirstLayer(const ui::Layer* layer) {
  const ui::Layer* l = layer;
  while (l->children().size() > 0)
    l = l->children()[0];
  return l;
}

const ui::Layer* NextLayer(const ui::Layer* layer) {
  const ui::Layer* parent = layer->parent();
  if (!parent)
    return NULL;
  const std::vector<ui::Layer*> children = parent->children();
  size_t index;
  for (index = 0; index < children.size(); index++) {
    if (children[index] == layer)
      break;
  }
  size_t next = index + 1;
  if (next < children.size())
    return FirstLayer(children[next]);
  return parent;
}

// Given the root nodes of a View tree and a Layer tree, makes sure the two
// trees are in sync.
bool ViewAndLayerTreeAreConsistent(const views::View* view,
                                   const ui::Layer* layer) {
  const views::View* v = FirstView(view);
  const ui::Layer* l = FirstLayer(layer);
  while (v && l) {
    // Find the view with a layer.
    while (v && !v->layer())
      v = NextView(v);
    EXPECT_TRUE(v);
    if (!v)
      return false;

    // Check if the View tree and the Layer tree are in sync.
    EXPECT_EQ(l, v->layer());
    if (v->layer() != l)
      return false;

    // Check if the visibility states of the View and the Layer are in sync.
    EXPECT_EQ(l->IsDrawn(), v->IsDrawn());
    if (v->IsDrawn() != l->IsDrawn()) {
      for (const views::View* vv = v; vv; vv = vv->parent())
        LOG(ERROR) << "V: " << vv << " " << vv->visible() << " "
                   << vv->IsDrawn() << " " << vv->layer();
      for (const ui::Layer* ll = l; ll; ll = ll->parent())
        LOG(ERROR) << "L: " << ll << " " << ll->IsDrawn();
      return false;
    }

    // Check if the size of the View and the Layer are in sync.
    EXPECT_EQ(l->bounds(), v->bounds());
    if (v->bounds() != l->bounds())
      return false;

    if (v == view || l == layer)
      return v == view && l == layer;

    v = NextView(v);
    l = NextLayer(l);
  }

  return false;
}

// Constructs a View tree with the specified depth.
void ConstructTree(views::View* view, int depth) {
  if (depth == 0)
    return;
  int count = base::RandInt(1, 5);
  for (int i = 0; i < count; i++) {
    views::View* v = new views::View;
    view->AddChildView(v);
    if (base::RandDouble() > 0.5)
      v->SetPaintToLayer(true);
    if (base::RandDouble() < 0.2)
      v->SetVisible(false);

    ConstructTree(v, depth - 1);
  }
}

void ScrambleTree(views::View* view) {
  int count = view->child_count();
  if (count == 0)
    return;
  for (int i = 0; i < count; i++) {
    ScrambleTree(view->child_at(i));
  }

  if (count > 1) {
    int a = base::RandInt(0, count - 1);
    int b = base::RandInt(0, count - 1);

    views::View* view_a = view->child_at(a);
    views::View* view_b = view->child_at(b);
    view->ReorderChildView(view_a, b);
    view->ReorderChildView(view_b, a);
  }

  if (!view->layer() && base::RandDouble() < 0.1)
    view->SetPaintToLayer(true);

  if (base::RandDouble() < 0.1)
    view->SetVisible(!view->visible());
}

// Convenience to make constructing a GestureEvent simpler.
class GestureEventForTest : public ui::GestureEvent {
 public:
  GestureEventForTest(ui::EventType type, int x, int y, int flags)
      : GestureEvent(type, x, y, flags, base::TimeDelta(),
                     ui::GestureEventDetails(type, 0.0f, 0.0f), 0) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureEventForTest);
};

}  // namespace

namespace views {

typedef ViewsTestBase ViewTest;

// A derived class for testing purpose.
class TestView : public View {
 public:
  TestView()
      : View(),
        delete_on_pressed_(false),
        native_theme_(NULL),
        can_process_events_within_subtree_(true) {}
  virtual ~TestView() {}

  // Reset all test state
  void Reset() {
    did_change_bounds_ = false;
    last_mouse_event_type_ = 0;
    location_.SetPoint(0, 0);
    received_mouse_enter_ = false;
    received_mouse_exit_ = false;
    last_gesture_event_type_ = 0;
    last_gesture_event_was_handled_ = false;
    last_clip_.setEmpty();
    accelerator_count_map_.clear();
    can_process_events_within_subtree_ = true;
  }

  // Exposed as public for testing.
  void DoFocus() {
    views::View::Focus();
  }

  void DoBlur() {
    views::View::Blur();
  }

  bool focusable() const { return View::focusable(); }

  void set_can_process_events_within_subtree(bool can_process) {
    can_process_events_within_subtree_ = can_process;
  }

  virtual bool CanProcessEventsWithinSubtree() const OVERRIDE {
    return can_process_events_within_subtree_;
  }

  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  // Ignores GestureEvent by default.
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  virtual void Paint(gfx::Canvas* canvas, const CullSet& cull_set) OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  virtual void OnNativeThemeChanged(const ui::NativeTheme* native_theme)
      OVERRIDE;

  // OnBoundsChanged.
  bool did_change_bounds_;
  gfx::Rect new_bounds_;

  // MouseEvent.
  int last_mouse_event_type_;
  gfx::Point location_;
  bool received_mouse_enter_;
  bool received_mouse_exit_;
  bool delete_on_pressed_;

  // Painting.
  std::vector<gfx::Rect> scheduled_paint_rects_;

  // GestureEvent
  int last_gesture_event_type_;
  bool last_gesture_event_was_handled_;

  // Painting.
  SkRect last_clip_;

  // Accelerators.
  std::map<ui::Accelerator, int> accelerator_count_map_;

  // Native theme.
  const ui::NativeTheme* native_theme_;

  // Value to return from CanProcessEventsWithinSubtree().
  bool can_process_events_within_subtree_;
};

// A view subclass that consumes all Gesture events for testing purposes.
class TestViewConsumeGesture : public TestView {
 public:
  TestViewConsumeGesture() : TestView() {}
  virtual ~TestViewConsumeGesture() {}

 protected:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    last_gesture_event_type_ = event->type();
    location_.SetPoint(event->x(), event->y());
    event->StopPropagation();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestViewConsumeGesture);
};

// A view subclass that ignores all Gesture events.
class TestViewIgnoreGesture: public TestView {
 public:
  TestViewIgnoreGesture() : TestView() {}
  virtual ~TestViewIgnoreGesture() {}

 private:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(TestViewIgnoreGesture);
};

// A view subclass that ignores all scroll-gesture events, but consume all other
// gesture events.
class TestViewIgnoreScrollGestures : public TestViewConsumeGesture {
 public:
  TestViewIgnoreScrollGestures() {}
  virtual ~TestViewIgnoreScrollGestures() {}

 private:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->IsScrollGestureEvent())
      return;
    TestViewConsumeGesture::OnGestureEvent(event);
  }

  DISALLOW_COPY_AND_ASSIGN(TestViewIgnoreScrollGestures);
};

////////////////////////////////////////////////////////////////////////////////
// OnBoundsChanged
////////////////////////////////////////////////////////////////////////////////

void TestView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  did_change_bounds_ = true;
  new_bounds_ = bounds();
}

TEST_F(ViewTest, OnBoundsChanged) {
  TestView v;

  gfx::Rect prev_rect(0, 0, 200, 200);
  gfx::Rect new_rect(100, 100, 250, 250);

  v.SetBoundsRect(prev_rect);
  v.Reset();
  v.SetBoundsRect(new_rect);

  EXPECT_TRUE(v.did_change_bounds_);
  EXPECT_EQ(v.new_bounds_, new_rect);
  EXPECT_EQ(v.bounds(), new_rect);
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent
////////////////////////////////////////////////////////////////////////////////

bool TestView::OnMousePressed(const ui::MouseEvent& event) {
  last_mouse_event_type_ = event.type();
  location_.SetPoint(event.x(), event.y());
  if (delete_on_pressed_)
    delete this;
  return true;
}

bool TestView::OnMouseDragged(const ui::MouseEvent& event) {
  last_mouse_event_type_ = event.type();
  location_.SetPoint(event.x(), event.y());
  return true;
}

void TestView::OnMouseReleased(const ui::MouseEvent& event) {
  last_mouse_event_type_ = event.type();
  location_.SetPoint(event.x(), event.y());
}

void TestView::OnMouseEntered(const ui::MouseEvent& event) {
  received_mouse_enter_ = true;
}

void TestView::OnMouseExited(const ui::MouseEvent& event) {
  received_mouse_exit_ = true;
}

TEST_F(ViewTest, MouseEvent) {
  TestView* v1 = new TestView();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  TestView* v2 = new TestView();
  v2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  internal::RootView* root =
      static_cast<internal::RootView*>(widget->GetRootView());

  root->AddChildView(v1);
  v1->AddChildView(v2);

  v1->Reset();
  v2->Reset();

  gfx::Point p1(110, 120);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, p1, p1,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::ET_MOUSE_PRESSED);
  EXPECT_EQ(v2->location_.x(), 10);
  EXPECT_EQ(v2->location_.y(), 20);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, 0);

  // Drag event out of bounds. Should still go to v2
  v1->Reset();
  v2->Reset();
  gfx::Point p2(50, 40);
  ui::MouseEvent dragged(ui::ET_MOUSE_DRAGGED, p2, p2,
                         ui::EF_LEFT_MOUSE_BUTTON, 0);
  root->OnMouseDragged(dragged);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::ET_MOUSE_DRAGGED);
  EXPECT_EQ(v2->location_.x(), -50);
  EXPECT_EQ(v2->location_.y(), -60);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, 0);

  // Releasted event out of bounds. Should still go to v2
  v1->Reset();
  v2->Reset();
  ui::MouseEvent released(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), 0,
                          0);
  root->OnMouseDragged(released);
  EXPECT_EQ(v2->last_mouse_event_type_, ui::ET_MOUSE_RELEASED);
  EXPECT_EQ(v2->location_.x(), -100);
  EXPECT_EQ(v2->location_.y(), -100);
  // Make sure v1 did not receive the event
  EXPECT_EQ(v1->last_mouse_event_type_, 0);

  widget->CloseNow();
}

// Confirm that a view can be deleted as part of processing a mouse press.
TEST_F(ViewTest, DeleteOnPressed) {
  TestView* v1 = new TestView();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  TestView* v2 = new TestView();
  v2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  v1->Reset();
  v2->Reset();

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();

  root->AddChildView(v1);
  v1->AddChildView(v2);

  v2->delete_on_pressed_ = true;
  gfx::Point point(110, 120);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);
  EXPECT_EQ(0, v1->child_count());

  widget->CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// GestureEvent
////////////////////////////////////////////////////////////////////////////////

void TestView::OnGestureEvent(ui::GestureEvent* event) {
}

TEST_F(ViewTest, GestureEvent) {
  // Views hierarchy for non delivery of GestureEvent.
  TestView* v1 = new TestViewConsumeGesture();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  TestView* v2 = new TestViewConsumeGesture();
  v2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  TestView* v3 = new TestViewIgnoreGesture();
  v3->SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  internal::RootView* root =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventDispatchDetails details;

  root->AddChildView(v1);
  v1->AddChildView(v2);
  v2->AddChildView(v3);

  // |v3| completely obscures |v2|, but all the gesture events on |v3| should
  // reach |v2| because |v3| doesn't process any gesture events. However, since
  // |v2| does process gesture events, gesture events on |v3| or |v2| should not
  // reach |v1|.

  v1->Reset();
  v2->Reset();
  v3->Reset();

  // Gesture on |v3|
  GestureEventForTest g1(ui::ET_GESTURE_TAP, 110, 110, 0);
  details = root->OnEventFromSource(&g1);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v2->last_gesture_event_type_);
  EXPECT_EQ(gfx::Point(10, 10), v2->location_);
  EXPECT_EQ(ui::ET_UNKNOWN, v1->last_gesture_event_type_);

  // Simulate an up so that RootView is no longer targetting |v3|.
  GestureEventForTest g1_up(ui::ET_GESTURE_END, 110, 110, 0);
  details = root->OnEventFromSource(&g1_up);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  v1->Reset();
  v2->Reset();
  v3->Reset();

  // Gesture on |v1|
  GestureEventForTest g2(ui::ET_GESTURE_TAP, 80, 80, 0);
  details = root->OnEventFromSource(&g2);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v1->last_gesture_event_type_);
  EXPECT_EQ(gfx::Point(80, 80), v1->location_);
  EXPECT_EQ(ui::ET_UNKNOWN, v2->last_gesture_event_type_);

  // Send event |g1| again. Even though the coordinates target |v3| it should go
  // to |v1| as that is the view the touch was initially down on.
  v1->last_gesture_event_type_ = ui::ET_UNKNOWN;
  v3->last_gesture_event_type_ = ui::ET_UNKNOWN;
  details = root->OnEventFromSource(&g1);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v1->last_gesture_event_type_);
  EXPECT_EQ(ui::ET_UNKNOWN, v3->last_gesture_event_type_);
  EXPECT_EQ("110,110", v1->location_.ToString());

  widget->CloseNow();
}

TEST_F(ViewTest, ScrollGestureEvent) {
  // Views hierarchy for non delivery of GestureEvent.
  TestView* v1 = new TestViewConsumeGesture();
  v1->SetBoundsRect(gfx::Rect(0, 0, 300, 300));

  TestView* v2 = new TestViewIgnoreScrollGestures();
  v2->SetBoundsRect(gfx::Rect(100, 100, 100, 100));

  TestView* v3 = new TestViewIgnoreGesture();
  v3->SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  scoped_ptr<Widget> widget(new Widget());
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  internal::RootView* root =
      static_cast<internal::RootView*>(widget->GetRootView());
  ui::EventDispatchDetails details;

  root->AddChildView(v1);
  v1->AddChildView(v2);
  v2->AddChildView(v3);

  // |v3| completely obscures |v2|, but all the gesture events on |v3| should
  // reach |v2| because |v3| doesn't process any gesture events. However, since
  // |v2| does process gesture events, gesture events on |v3| or |v2| should not
  // reach |v1|.

  v1->Reset();
  v2->Reset();
  v3->Reset();

  // Gesture on |v3|
  GestureEventForTest g1(ui::ET_GESTURE_TAP, 110, 110, 0);
  details = root->OnEventFromSource(&g1);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v2->last_gesture_event_type_);
  EXPECT_EQ(gfx::Point(10, 10), v2->location_);
  EXPECT_EQ(ui::ET_UNKNOWN, v1->last_gesture_event_type_);

  v2->Reset();

  // Send scroll gestures on |v3|. The gesture should reach |v2|, however,
  // since it does not process scroll-gesture events, these events should reach
  // |v1|.
  GestureEventForTest gscroll_begin(ui::ET_GESTURE_SCROLL_BEGIN, 115, 115, 0);
  details = root->OnEventFromSource(&gscroll_begin);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_UNKNOWN, v2->last_gesture_event_type_);
  EXPECT_EQ(ui::ET_GESTURE_SCROLL_BEGIN, v1->last_gesture_event_type_);
  v1->Reset();

  // Send a second tap on |v1|. The event should reach |v2| since it is the
  // default gesture handler, and not |v1| (even though it is the view under the
  // point, and is the scroll event handler).
  GestureEventForTest second_tap(ui::ET_GESTURE_TAP, 70, 70, 0);
  details = root->OnEventFromSource(&second_tap);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v2->last_gesture_event_type_);
  EXPECT_EQ(ui::ET_UNKNOWN, v1->last_gesture_event_type_);
  v2->Reset();

  GestureEventForTest gscroll_end(ui::ET_GESTURE_SCROLL_END, 50, 50, 0);
  details = root->OnEventFromSource(&gscroll_end);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_SCROLL_END, v1->last_gesture_event_type_);
  v1->Reset();

  // Simulate an up so that RootView is no longer targetting |v3|.
  GestureEventForTest g1_up(ui::ET_GESTURE_END, 110, 110, 0);
  details = root->OnEventFromSource(&g1_up);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_END, v2->last_gesture_event_type_);

  v1->Reset();
  v2->Reset();
  v3->Reset();

  // Gesture on |v1|
  GestureEventForTest g2(ui::ET_GESTURE_TAP, 80, 80, 0);
  details = root->OnEventFromSource(&g2);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v1->last_gesture_event_type_);
  EXPECT_EQ(gfx::Point(80, 80), v1->location_);
  EXPECT_EQ(ui::ET_UNKNOWN, v2->last_gesture_event_type_);

  // Send event |g1| again. Even though the coordinates target |v3| it should go
  // to |v1| as that is the view the touch was initially down on.
  v1->last_gesture_event_type_ = ui::ET_UNKNOWN;
  v3->last_gesture_event_type_ = ui::ET_UNKNOWN;
  details = root->OnEventFromSource(&g1);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_FALSE(details.target_destroyed);

  EXPECT_EQ(ui::ET_GESTURE_TAP, v1->last_gesture_event_type_);
  EXPECT_EQ(ui::ET_UNKNOWN, v3->last_gesture_event_type_);
  EXPECT_EQ("110,110", v1->location_.ToString());

  widget->CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// Painting
////////////////////////////////////////////////////////////////////////////////

void TestView::Paint(gfx::Canvas* canvas, const CullSet& cull_set) {
  canvas->sk_canvas()->getClipBounds(&last_clip_);
}

void TestView::SchedulePaintInRect(const gfx::Rect& rect) {
  scheduled_paint_rects_.push_back(rect);
  View::SchedulePaintInRect(rect);
}

void CheckRect(const SkRect& check_rect, const SkRect& target_rect) {
  EXPECT_EQ(target_rect.fLeft, check_rect.fLeft);
  EXPECT_EQ(target_rect.fRight, check_rect.fRight);
  EXPECT_EQ(target_rect.fTop, check_rect.fTop);
  EXPECT_EQ(target_rect.fBottom, check_rect.fBottom);
}

TEST_F(ViewTest, RemoveNotification) {
  ViewStorage* vs = ViewStorage::GetInstance();
  Widget* widget = new Widget;
  widget->Init(CreateParams(Widget::InitParams::TYPE_POPUP));
  View* root_view = widget->GetRootView();

  View* v1 = new View;
  int s1 = vs->CreateStorageID();
  vs->StoreView(s1, v1);
  root_view->AddChildView(v1);
  View* v11 = new View;
  int s11 = vs->CreateStorageID();
  vs->StoreView(s11, v11);
  v1->AddChildView(v11);
  View* v111 = new View;
  int s111 = vs->CreateStorageID();
  vs->StoreView(s111, v111);
  v11->AddChildView(v111);
  View* v112 = new View;
  int s112 = vs->CreateStorageID();
  vs->StoreView(s112, v112);
  v11->AddChildView(v112);
  View* v113 = new View;
  int s113 = vs->CreateStorageID();
  vs->StoreView(s113, v113);
  v11->AddChildView(v113);
  View* v1131 = new View;
  int s1131 = vs->CreateStorageID();
  vs->StoreView(s1131, v1131);
  v113->AddChildView(v1131);
  View* v12 = new View;
  int s12 = vs->CreateStorageID();
  vs->StoreView(s12, v12);
  v1->AddChildView(v12);

  View* v2 = new View;
  int s2 = vs->CreateStorageID();
  vs->StoreView(s2, v2);
  root_view->AddChildView(v2);
  View* v21 = new View;
  int s21 = vs->CreateStorageID();
  vs->StoreView(s21, v21);
  v2->AddChildView(v21);
  View* v211 = new View;
  int s211 = vs->CreateStorageID();
  vs->StoreView(s211, v211);
  v21->AddChildView(v211);

  size_t stored_views = vs->view_count();

  // Try removing a leaf view.
  v21->RemoveChildView(v211);
  EXPECT_EQ(stored_views - 1, vs->view_count());
  EXPECT_EQ(NULL, vs->RetrieveView(s211));
  delete v211;  // We won't use this one anymore.

  // Now try removing a view with a hierarchy of depth 1.
  v11->RemoveChildView(v113);
  EXPECT_EQ(stored_views - 3, vs->view_count());
  EXPECT_EQ(NULL, vs->RetrieveView(s113));
  EXPECT_EQ(NULL, vs->RetrieveView(s1131));
  delete v113;  // We won't use this one anymore.

  // Now remove even more.
  root_view->RemoveChildView(v1);
  EXPECT_EQ(NULL, vs->RetrieveView(s1));
  EXPECT_EQ(NULL, vs->RetrieveView(s11));
  EXPECT_EQ(NULL, vs->RetrieveView(s12));
  EXPECT_EQ(NULL, vs->RetrieveView(s111));
  EXPECT_EQ(NULL, vs->RetrieveView(s112));

  // Put v1 back for more tests.
  root_view->AddChildView(v1);
  vs->StoreView(s1, v1);

  // Synchronously closing the window deletes the view hierarchy, which should
  // remove all its views from ViewStorage.
  widget->CloseNow();
  EXPECT_EQ(stored_views - 10, vs->view_count());
  EXPECT_EQ(NULL, vs->RetrieveView(s1));
  EXPECT_EQ(NULL, vs->RetrieveView(s12));
  EXPECT_EQ(NULL, vs->RetrieveView(s11));
  EXPECT_EQ(NULL, vs->RetrieveView(s12));
  EXPECT_EQ(NULL, vs->RetrieveView(s21));
  EXPECT_EQ(NULL, vs->RetrieveView(s111));
  EXPECT_EQ(NULL, vs->RetrieveView(s112));
}

namespace {
class HitTestView : public View {
 public:
  explicit HitTestView(bool has_hittest_mask)
      : has_hittest_mask_(has_hittest_mask) {
  }
  virtual ~HitTestView() {}

 protected:
  // Overridden from View:
  virtual bool HasHitTestMask() const OVERRIDE {
    return has_hittest_mask_;
  }
  virtual void GetHitTestMask(HitTestSource source,
                              gfx::Path* mask) const OVERRIDE {
    DCHECK(has_hittest_mask_);
    DCHECK(mask);

    SkScalar w = SkIntToScalar(width());
    SkScalar h = SkIntToScalar(height());

    // Create a triangular mask within the bounds of this View.
    mask->moveTo(w / 2, 0);
    mask->lineTo(w, h);
    mask->lineTo(0, h);
    mask->close();
  }

 private:
  bool has_hittest_mask_;

  DISALLOW_COPY_AND_ASSIGN(HitTestView);
};

gfx::Point ConvertPointToView(View* view, const gfx::Point& p) {
  gfx::Point tmp(p);
  View::ConvertPointToTarget(view->GetWidget()->GetRootView(), view, &tmp);
  return tmp;
}

gfx::Rect ConvertRectToView(View* view, const gfx::Rect& r) {
  gfx::Rect tmp(r);
  tmp.set_origin(ConvertPointToView(view, r.origin()));
  return tmp;
}

void RotateCounterclockwise(gfx::Transform* transform) {
  transform->matrix().set3x3(0, -1, 0,
                             1,  0, 0,
                             0,  0, 1);
}

void RotateClockwise(gfx::Transform* transform) {
  transform->matrix().set3x3( 0, 1, 0,
                             -1, 0, 0,
                              0, 0, 1);
}

}  // namespace

TEST_F(ViewTest, HitTestMasks) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(params);
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  gfx::Rect v1_bounds = gfx::Rect(0, 0, 100, 100);
  HitTestView* v1 = new HitTestView(false);
  v1->SetBoundsRect(v1_bounds);
  root_view->AddChildView(v1);

  gfx::Rect v2_bounds = gfx::Rect(105, 0, 100, 100);
  HitTestView* v2 = new HitTestView(true);
  v2->SetBoundsRect(v2_bounds);
  root_view->AddChildView(v2);

  gfx::Point v1_centerpoint = v1_bounds.CenterPoint();
  gfx::Point v2_centerpoint = v2_bounds.CenterPoint();
  gfx::Point v1_origin = v1_bounds.origin();
  gfx::Point v2_origin = v2_bounds.origin();

  gfx::Rect r1(10, 10, 110, 15);
  gfx::Rect r2(106, 1, 98, 98);
  gfx::Rect r3(0, 0, 300, 300);
  gfx::Rect r4(115, 342, 200, 10);

  // Test HitTestPoint
  EXPECT_TRUE(v1->HitTestPoint(ConvertPointToView(v1, v1_centerpoint)));
  EXPECT_TRUE(v2->HitTestPoint(ConvertPointToView(v2, v2_centerpoint)));

  EXPECT_TRUE(v1->HitTestPoint(ConvertPointToView(v1, v1_origin)));
  EXPECT_FALSE(v2->HitTestPoint(ConvertPointToView(v2, v2_origin)));

  // Test HitTestRect
  EXPECT_TRUE(v1->HitTestRect(ConvertRectToView(v1, r1)));
  EXPECT_FALSE(v2->HitTestRect(ConvertRectToView(v2, r1)));

  EXPECT_FALSE(v1->HitTestRect(ConvertRectToView(v1, r2)));
  EXPECT_TRUE(v2->HitTestRect(ConvertRectToView(v2, r2)));

  EXPECT_TRUE(v1->HitTestRect(ConvertRectToView(v1, r3)));
  EXPECT_TRUE(v2->HitTestRect(ConvertRectToView(v2, r3)));

  EXPECT_FALSE(v1->HitTestRect(ConvertRectToView(v1, r4)));
  EXPECT_FALSE(v2->HitTestRect(ConvertRectToView(v2, r4)));

  // Test GetEventHandlerForPoint
  EXPECT_EQ(v1, root_view->GetEventHandlerForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetEventHandlerForPoint(v2_centerpoint));

  EXPECT_EQ(v1, root_view->GetEventHandlerForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetEventHandlerForPoint(v2_origin));

  // Test GetTooltipHandlerForPoint
  EXPECT_EQ(v1, root_view->GetTooltipHandlerForPoint(v1_centerpoint));
  EXPECT_EQ(v2, root_view->GetTooltipHandlerForPoint(v2_centerpoint));

  EXPECT_EQ(v1, root_view->GetTooltipHandlerForPoint(v1_origin));
  EXPECT_EQ(root_view, root_view->GetTooltipHandlerForPoint(v2_origin));

  EXPECT_FALSE(v1->GetTooltipHandlerForPoint(v2_origin));

  widget->CloseNow();
}

// Tests the correctness of the rect-based targeting algorithm implemented in
// View::GetEventHandlerForRect(). See http://goo.gl/3Jp2BD for a description
// of rect-based targeting.
TEST_F(ViewTest, GetEventHandlerForRect) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(params);
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // Have this hierarchy of views (the coordinates here are all in
  // the root view's coordinate space):
  // v1 (0, 0, 100, 100)
  // v2 (150, 0, 250, 100)
  // v3 (0, 200, 150, 100)
  //     v31 (10, 210, 80, 80)
  //     v32 (110, 210, 30, 80)
  // v4 (300, 200, 100, 100)
  //     v41 (310, 210, 80, 80)
  //         v411 (370, 275, 10, 5)
  // v5 (450, 197, 30, 36)
  //     v51 (450, 200, 30, 30)

  // The coordinates used for SetBounds are in parent coordinates.

  TestView* v1 = new TestView;
  v1->SetBounds(0, 0, 100, 100);
  root_view->AddChildView(v1);

  TestView* v2 = new TestView;
  v2->SetBounds(150, 0, 250, 100);
  root_view->AddChildView(v2);

  TestView* v3 = new TestView;
  v3->SetBounds(0, 200, 150, 100);
  root_view->AddChildView(v3);

  TestView* v4 = new TestView;
  v4->SetBounds(300, 200, 100, 100);
  root_view->AddChildView(v4);

  TestView* v31 = new TestView;
  v31->SetBounds(10, 10, 80, 80);
  v3->AddChildView(v31);

  TestView* v32 = new TestView;
  v32->SetBounds(110, 10, 30, 80);
  v3->AddChildView(v32);

  TestView* v41 = new TestView;
  v41->SetBounds(10, 10, 80, 80);
  v4->AddChildView(v41);

  TestView* v411 = new TestView;
  v411->SetBounds(60, 65, 10, 5);
  v41->AddChildView(v411);

  TestView* v5 = new TestView;
  v5->SetBounds(450, 197, 30, 36);
  root_view->AddChildView(v5);

  TestView* v51 = new TestView;
  v51->SetBounds(0, 3, 30, 30);
  v5->AddChildView(v51);

  // |touch_rect| does not intersect any descendant view of |root_view|.
  gfx::Rect touch_rect(105, 105, 30, 45);
  View* result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;

  // Covers |v1| by at least 60%.
  touch_rect.SetRect(15, 15, 100, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = NULL;

  // Intersects |v1| but does not cover it by at least 60%. The center
  // of |touch_rect| is within |v1|.
  touch_rect.SetRect(50, 50, 5, 10);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = NULL;

  // Intersects |v1| but does not cover it by at least 60%. The center
  // of |touch_rect| is not within |v1|.
  touch_rect.SetRect(95, 96, 21, 22);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;

  // Intersects |v1| and |v2|, but only covers |v2| by at least 60%.
  touch_rect.SetRect(95, 10, 300, 120);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v2, result_view);
  result_view = NULL;

  // Covers both |v1| and |v2| by at least 60%, but the center point
  // of |touch_rect| is closer to the center point of |v2|.
  touch_rect.SetRect(20, 20, 400, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v2, result_view);
  result_view = NULL;

  // Covers both |v1| and |v2| by at least 60%, but the center point
  // of |touch_rect| is closer to the center point of |v1|.
  touch_rect.SetRect(-700, -15, 1050, 110);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = NULL;

  // A mouse click within |v1| will target |v1|.
  touch_rect.SetRect(15, 15, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v1, result_view);
  result_view = NULL;

  // Intersects |v3| and |v31| by at least 60% and the center point
  // of |touch_rect| is closer to the center point of |v31|.
  touch_rect.SetRect(0, 200, 110, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v31, result_view);
  result_view = NULL;

  // Intersects |v3| and |v31|, but neither by at least 60%. The
  // center point of |touch_rect| lies within |v31|.
  touch_rect.SetRect(80, 280, 15, 15);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v31, result_view);
  result_view = NULL;

  // Covers |v3|, |v31|, and |v32| all by at least 60%, and the
  // center point of |touch_rect| is closest to the center point
  // of |v32|.
  touch_rect.SetRect(0, 200, 200, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = NULL;

  // Intersects all of |v3|, |v31|, and |v32|, but only covers
  // |v31| and |v32| by at least 60%. The center point of
  // |touch_rect| is closest to the center point of |v32|.
  touch_rect.SetRect(30, 225, 180, 115);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = NULL;

  // A mouse click at the corner of |v3| will target |v3|.
  touch_rect.SetRect(0, 200, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v3, result_view);
  result_view = NULL;

  // A mouse click within |v32| will target |v32|.
  touch_rect.SetRect(112, 211, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = NULL;

  // Covers all of |v4|, |v41|, and |v411| by at least 60%.
  // The center point of |touch_rect| is equally close to
  // the center points of |v4| and |v41|.
  touch_rect.SetRect(310, 210, 80, 80);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = NULL;

  // Intersects all of |v4|, |v41|, and |v411| but only covers
  // |v411| by at least 60%.
  touch_rect.SetRect(370, 275, 7, 5);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v411, result_view);
  result_view = NULL;

  // Intersects |v4| and |v41| but covers neither by at least 60%.
  // The center point of |touch_rect| is equally close to the center
  // points of |v4| and |v41|.
  touch_rect.SetRect(345, 245, 7, 7);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = NULL;

  // Intersects all of |v4|, |v41|, and |v411| and covers none of
  // them by at least 60%. The center point of |touch_rect| lies
  // within |v411|.
  touch_rect.SetRect(368, 272, 4, 6);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v411, result_view);
  result_view = NULL;

  // Intersects all of |v4|, |v41|, and |v411| and covers none of
  // them by at least 60%. The center point of |touch_rect| lies
  // within |v41|.
  touch_rect.SetRect(365, 270, 7, 7);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = NULL;

  // Intersects all of |v4|, |v41|, and |v411| and covers none of
  // them by at least 60%. The center point of |touch_rect| lies
  // within |v4|.
  touch_rect.SetRect(205, 275, 200, 2);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v4, result_view);
  result_view = NULL;

  // Intersects all of |v4|, |v41|, and |v411| but only covers
  // |v41| by at least 60%.
  touch_rect.SetRect(310, 210, 61, 66);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = NULL;

  // A mouse click within |v411| will target |v411|.
  touch_rect.SetRect(372, 275, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v411, result_view);
  result_view = NULL;

  // A mouse click within |v41| will target |v41|.
  touch_rect.SetRect(350, 215, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v41, result_view);
  result_view = NULL;

  // Covers |v3|, |v4|, and all of their descendants by at
  // least 60%. The center point of |touch_rect| is closest
  // to the center point of |v32|.
  touch_rect.SetRect(0, 200, 400, 100);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = NULL;

  // Intersects all of |v2|, |v3|, |v32|, |v4|, |v41|, and |v411|.
  // Covers |v2|, |v32|, |v4|, |v41|, and |v411| by at least 60%.
  // The center point of |touch_rect| is closest to the center
  // point of |root_view|.
  touch_rect.SetRect(110, 15, 375, 450);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;

  // Covers all views (except |v5| and |v51|) by at least 60%. The
  // center point of |touch_rect| is equally close to the center
  // points of |v2| and |v32|. One is not a descendant of the other,
  // so in this case the view selected is arbitrary (i.e.,
  // it depends only on the ordering of nodes in the views
  // hierarchy).
  touch_rect.SetRect(0, 0, 400, 300);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v32, result_view);
  result_view = NULL;

  // Covers |v5| and |v51| by at least 60%, and the center point of
  // the touch is located within both views. Since both views share
  // the same center point, the child view should be selected.
  touch_rect.SetRect(440, 190, 40, 40);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = NULL;

  // Covers |v5| and |v51| by at least 60%, but the center point of
  // the touch is not located within either view. Since both views
  // share the same center point, the child view should be selected.
  touch_rect.SetRect(455, 187, 60, 60);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = NULL;

  // Covers neither |v5| nor |v51| by at least 60%, but the center
  // of the touch is located within |v51|.
  touch_rect.SetRect(450, 197, 10, 10);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = NULL;

  // Covers neither |v5| nor |v51| by at least 60% but intersects both.
  // The center point is located outside of both views.
  touch_rect.SetRect(433, 180, 24, 24);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;

  // Only intersects |v5| but does not cover it by at least 60%. The
  // center point of the touch region is located within |v5|.
  touch_rect.SetRect(449, 196, 3, 3);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v5, result_view);
  result_view = NULL;

  // A mouse click within |v5| (but not |v51|) should target |v5|.
  touch_rect.SetRect(462, 199, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v5, result_view);
  result_view = NULL;

  // A mouse click |v5| and |v51| should target the child view.
  touch_rect.SetRect(452, 226, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = NULL;

  // A mouse click on the center of |v5| and |v51| should target
  // the child view.
  touch_rect.SetRect(465, 215, 1, 1);
  result_view = root_view->GetEventHandlerForRect(touch_rect);
  EXPECT_EQ(v51, result_view);
  result_view = NULL;

  widget->CloseNow();
}

// Tests that GetEventHandlerForRect() and GetTooltipHandlerForPoint() behave
// as expected when different views in the view hierarchy return false
// when CanProcessEventsWithinSubtree() is called.
TEST_F(ViewTest, CanProcessEventsWithinSubtree) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(params);
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // Have this hierarchy of views (the coords here are in the coordinate
  // space of the root view):
  // v (0, 0, 100, 100)
  //  - v_child (0, 0, 20, 30)
  //    - v_grandchild (5, 5, 5, 15)

  TestView* v = new TestView;
  v->SetBounds(0, 0, 100, 100);
  root_view->AddChildView(v);
  v->set_notify_enter_exit_on_child(true);

  TestView* v_child = new TestView;
  v_child->SetBounds(0, 0, 20, 30);
  v->AddChildView(v_child);

  TestView* v_grandchild = new TestView;
  v_grandchild->SetBounds(5, 5, 5, 15);
  v_child->AddChildView(v_grandchild);

  v->Reset();
  v_child->Reset();
  v_grandchild->Reset();

  // Define rects and points within the views in the hierarchy.
  gfx::Rect rect_in_v_grandchild(7, 7, 3, 3);
  gfx::Point point_in_v_grandchild(rect_in_v_grandchild.origin());
  gfx::Rect rect_in_v_child(12, 3, 5, 5);
  gfx::Point point_in_v_child(rect_in_v_child.origin());
  gfx::Rect rect_in_v(50, 50, 25, 30);
  gfx::Point point_in_v(rect_in_v.origin());

  // When all three views return true when CanProcessEventsWithinSubtree()
  // is called, targeting should behave as expected.

  View* result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(v_grandchild, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(v_grandchild, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(v, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(v, result_view);
  result_view = NULL;

  // When |v_grandchild| returns false when CanProcessEventsWithinSubtree()
  // is called, then |v_grandchild| cannot be returned as a target.

  v_grandchild->set_can_process_events_within_subtree(false);

  result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(v_child, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(v_child, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(v_child, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(v, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(v, result_view);

  // When |v_grandchild| returns false when CanProcessEventsWithinSubtree()
  // is called, then NULL should be returned as a target if we call
  // GetTooltipHandlerForPoint() with |v_grandchild| as the root of the
  // views tree. Note that the location must be in the coordinate space
  // of the root view (|v_grandchild| in this case), so use (1, 1).

  result_view = v_grandchild;
  result_view = v_grandchild->GetTooltipHandlerForPoint(gfx::Point(1, 1));
  EXPECT_EQ(NULL, result_view);
  result_view = NULL;

  // When |v_child| returns false when CanProcessEventsWithinSubtree()
  // is called, then neither |v_child| nor |v_grandchild| can be returned
  // as a target (|v| should be returned as the target for each case).

  v_grandchild->Reset();
  v_child->set_can_process_events_within_subtree(false);

  result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(v, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(v, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(v, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(v, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(v, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(v, result_view);
  result_view = NULL;

  // When |v| returns false when CanProcessEventsWithinSubtree()
  // is called, then none of |v|, |v_child|, and |v_grandchild| can be returned
  // as a target (|root_view| should be returned as the target for each case).

  v_child->Reset();
  v->set_can_process_events_within_subtree(false);

  result_view = root_view->GetEventHandlerForRect(rect_in_v_grandchild);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_grandchild);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v_child);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v_child);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;

  result_view = root_view->GetEventHandlerForRect(rect_in_v);
  EXPECT_EQ(root_view, result_view);
  result_view = NULL;
  result_view = root_view->GetTooltipHandlerForPoint(point_in_v);
  EXPECT_EQ(root_view, result_view);
}

TEST_F(ViewTest, NotifyEnterExitOnChild) {
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  widget->Init(params);
  View* root_view = widget->GetRootView();
  root_view->SetBoundsRect(gfx::Rect(0, 0, 500, 500));

  // Have this hierarchy of views (the coords here are in root coord):
  // v1 (0, 0, 100, 100)
  //  - v11 (0, 0, 20, 30)
  //    - v111 (5, 5, 5, 15)
  //  - v12 (50, 10, 30, 90)
  //    - v121 (60, 20, 10, 10)
  // v2 (105, 0, 100, 100)
  //  - v21 (120, 10, 50, 20)

  TestView* v1 = new TestView;
  v1->SetBounds(0, 0, 100, 100);
  root_view->AddChildView(v1);
  v1->set_notify_enter_exit_on_child(true);

  TestView* v11 = new TestView;
  v11->SetBounds(0, 0, 20, 30);
  v1->AddChildView(v11);

  TestView* v111 = new TestView;
  v111->SetBounds(5, 5, 5, 15);
  v11->AddChildView(v111);

  TestView* v12 = new TestView;
  v12->SetBounds(50, 10, 30, 90);
  v1->AddChildView(v12);

  TestView* v121 = new TestView;
  v121->SetBounds(10, 10, 10, 10);
  v12->AddChildView(v121);

  TestView* v2 = new TestView;
  v2->SetBounds(105, 0, 100, 100);
  root_view->AddChildView(v2);

  TestView* v21 = new TestView;
  v21->SetBounds(15, 10, 50, 20);
  v2->AddChildView(v21);

  v1->Reset();
  v11->Reset();
  v111->Reset();
  v12->Reset();
  v121->Reset();
  v2->Reset();
  v21->Reset();

  // Move the mouse in v111.
  gfx::Point p1(6, 6);
  ui::MouseEvent move1(ui::ET_MOUSE_MOVED, p1, p1, 0, 0);
  root_view->OnMouseMoved(move1);
  EXPECT_TRUE(v111->received_mouse_enter_);
  EXPECT_FALSE(v11->last_mouse_event_type_);
  EXPECT_TRUE(v1->received_mouse_enter_);

  v111->Reset();
  v1->Reset();

  // Now, move into v121.
  gfx::Point p2(65, 21);
  ui::MouseEvent move2(ui::ET_MOUSE_MOVED, p2, p2, 0, 0);
  root_view->OnMouseMoved(move2);
  EXPECT_TRUE(v111->received_mouse_exit_);
  EXPECT_TRUE(v121->received_mouse_enter_);
  EXPECT_FALSE(v1->last_mouse_event_type_);

  v111->Reset();
  v121->Reset();

  // Now, move into v11.
  gfx::Point p3(1, 1);
  ui::MouseEvent move3(ui::ET_MOUSE_MOVED, p3, p3, 0, 0);
  root_view->OnMouseMoved(move3);
  EXPECT_TRUE(v121->received_mouse_exit_);
  EXPECT_TRUE(v11->received_mouse_enter_);
  EXPECT_FALSE(v1->last_mouse_event_type_);

  v121->Reset();
  v11->Reset();

  // Move to v21.
  gfx::Point p4(121, 15);
  ui::MouseEvent move4(ui::ET_MOUSE_MOVED, p4, p4, 0, 0);
  root_view->OnMouseMoved(move4);
  EXPECT_TRUE(v21->received_mouse_enter_);
  EXPECT_FALSE(v2->last_mouse_event_type_);
  EXPECT_TRUE(v11->received_mouse_exit_);
  EXPECT_TRUE(v1->received_mouse_exit_);

  v21->Reset();
  v11->Reset();
  v1->Reset();

  // Move to v1.
  gfx::Point p5(21, 0);
  ui::MouseEvent move5(ui::ET_MOUSE_MOVED, p5, p5, 0, 0);
  root_view->OnMouseMoved(move5);
  EXPECT_TRUE(v21->received_mouse_exit_);
  EXPECT_TRUE(v1->received_mouse_enter_);

  v21->Reset();
  v1->Reset();

  // Now, move into v11.
  gfx::Point p6(15, 15);
  ui::MouseEvent mouse6(ui::ET_MOUSE_MOVED, p6, p6, 0, 0);
  root_view->OnMouseMoved(mouse6);
  EXPECT_TRUE(v11->received_mouse_enter_);
  EXPECT_FALSE(v1->last_mouse_event_type_);

  v11->Reset();
  v1->Reset();

  // Move back into v1. Although |v1| had already received an ENTER for mouse6,
  // and the mouse remains inside |v1| the whole time, it receives another ENTER
  // when the mouse leaves v11.
  gfx::Point p7(21, 0);
  ui::MouseEvent mouse7(ui::ET_MOUSE_MOVED, p7, p7, 0, 0);
  root_view->OnMouseMoved(mouse7);
  EXPECT_TRUE(v11->received_mouse_exit_);
  EXPECT_FALSE(v1->received_mouse_enter_);

  widget->CloseNow();
}

TEST_F(ViewTest, Textfield) {
  const base::string16 kText = ASCIIToUTF16(
      "Reality is that which, when you stop believing it, doesn't go away.");
  const base::string16 kExtraText = ASCIIToUTF16("Pretty deep, Philip!");
  const base::string16 kEmptyString;

  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(params);
  View* root_view = widget->GetRootView();

  Textfield* textfield = new Textfield();
  root_view->AddChildView(textfield);

  // Test setting, appending text.
  textfield->SetText(kText);
  EXPECT_EQ(kText, textfield->text());
  textfield->AppendText(kExtraText);
  EXPECT_EQ(kText + kExtraText, textfield->text());
  textfield->SetText(base::string16());
  EXPECT_EQ(kEmptyString, textfield->text());

  // Test selection related methods.
  textfield->SetText(kText);
  EXPECT_EQ(kEmptyString, textfield->GetSelectedText());
  textfield->SelectAll(false);
  EXPECT_EQ(kText, textfield->text());
  textfield->ClearSelection();
  EXPECT_EQ(kEmptyString, textfield->GetSelectedText());

  widget->CloseNow();
}

// Tests that the Textfield view respond appropiately to cut/copy/paste.
TEST_F(ViewTest, TextfieldCutCopyPaste) {
  const base::string16 kNormalText = ASCIIToUTF16("Normal");
  const base::string16 kReadOnlyText = ASCIIToUTF16("Read only");
  const base::string16 kPasswordText =
      ASCIIToUTF16("Password! ** Secret stuff **");

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();

  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(params);
  View* root_view = widget->GetRootView();

  Textfield* normal = new Textfield();
  Textfield* read_only = new Textfield();
  read_only->SetReadOnly(true);
  Textfield* password = new Textfield();
  password->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);

  root_view->AddChildView(normal);
  root_view->AddChildView(read_only);
  root_view->AddChildView(password);

  normal->SetText(kNormalText);
  read_only->SetText(kReadOnlyText);
  password->SetText(kPasswordText);

  //
  // Test cut.
  //

  normal->SelectAll(false);
  normal->ExecuteCommand(IDS_APP_CUT);
  base::string16 result;
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  EXPECT_EQ(kNormalText, result);
  normal->SetText(kNormalText);  // Let's revert to the original content.

  read_only->SelectAll(false);
  read_only->ExecuteCommand(IDS_APP_CUT);
  result.clear();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  // Cut should have failed, so the clipboard content should not have changed.
  EXPECT_EQ(kNormalText, result);

  password->SelectAll(false);
  password->ExecuteCommand(IDS_APP_CUT);
  result.clear();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  // Cut should have failed, so the clipboard content should not have changed.
  EXPECT_EQ(kNormalText, result);

  //
  // Test copy.
  //

  // Start with |read_only| to observe a change in clipboard text.
  read_only->SelectAll(false);
  read_only->ExecuteCommand(IDS_APP_COPY);
  result.clear();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  EXPECT_EQ(kReadOnlyText, result);

  normal->SelectAll(false);
  normal->ExecuteCommand(IDS_APP_COPY);
  result.clear();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  EXPECT_EQ(kNormalText, result);

  password->SelectAll(false);
  password->ExecuteCommand(IDS_APP_COPY);
  result.clear();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  // Text cannot be copied from an obscured field; the clipboard won't change.
  EXPECT_EQ(kNormalText, result);

  //
  // Test paste.
  //

  // Attempting to paste kNormalText in a read-only text-field should fail.
  read_only->SelectAll(false);
  read_only->ExecuteCommand(IDS_APP_PASTE);
  EXPECT_EQ(kReadOnlyText, read_only->text());

  password->SelectAll(false);
  password->ExecuteCommand(IDS_APP_PASTE);
  EXPECT_EQ(kNormalText, password->text());

  // Copy from |read_only| to observe a change in the normal textfield text.
  read_only->SelectAll(false);
  read_only->ExecuteCommand(IDS_APP_COPY);
  normal->SelectAll(false);
  normal->ExecuteCommand(IDS_APP_PASTE);
  EXPECT_EQ(kReadOnlyText, normal->text());
  widget->CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// Accelerators
////////////////////////////////////////////////////////////////////////////////
bool TestView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  accelerator_count_map_[accelerator]++;
  return true;
}

// TODO: these tests were initially commented out when getting aura to
// run. Figure out if still valuable and either nuke or fix.
#if 0
TEST_F(ViewTest, ActivateAccelerator) {
  // Register a keyboard accelerator before the view is added to a window.
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestView* view = new TestView();
  view->Reset();
  view->AddAccelerator(return_accelerator);
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 0);

  // Create a window and add the view as its child.
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(view);
  widget->Show();

  // Get the focus manager.
  FocusManager* focus_manager = widget->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  // Hit the return key and see if it takes effect.
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);

  // Hit the escape key. Nothing should happen.
  ui::Accelerator escape_accelerator(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 0);

  // Now register the escape key and hit it again.
  view->AddAccelerator(escape_accelerator);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 1);

  // Remove the return key accelerator.
  view->RemoveAccelerator(return_accelerator);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 1);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 1);

  // Add it again. Hit the return key and the escape key.
  view->AddAccelerator(return_accelerator);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 1);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 2);

  // Remove all the accelerators.
  view->ResetAccelerators();
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 2);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(escape_accelerator));
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 2);
  EXPECT_EQ(view->accelerator_count_map_[escape_accelerator], 2);

  widget->CloseNow();
}

TEST_F(ViewTest, HiddenViewWithAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestView* view = new TestView();
  view->Reset();
  view->AddAccelerator(return_accelerator);
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 0);

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(view);
  widget->Show();

  FocusManager* focus_manager = widget->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  view->SetVisible(false);
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));

  view->SetVisible(true);
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));

  widget->CloseNow();
}

TEST_F(ViewTest, ViewInHiddenWidgetWithAccelerator) {
  ui::Accelerator return_accelerator(ui::VKEY_RETURN, ui::EF_NONE);
  TestView* view = new TestView();
  view->Reset();
  view->AddAccelerator(return_accelerator);
  EXPECT_EQ(view->accelerator_count_map_[return_accelerator], 0);

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget->Init(params);
  View* root = widget->GetRootView();
  root->AddChildView(view);

  FocusManager* focus_manager = widget->GetFocusManager();
  ASSERT_TRUE(focus_manager);

  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(0, view->accelerator_count_map_[return_accelerator]);

  widget->Show();
  EXPECT_TRUE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, view->accelerator_count_map_[return_accelerator]);

  widget->Hide();
  EXPECT_FALSE(focus_manager->ProcessAccelerator(return_accelerator));
  EXPECT_EQ(1, view->accelerator_count_map_[return_accelerator]);

  widget->CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// Mouse-wheel message rerouting
////////////////////////////////////////////////////////////////////////////////
class ScrollableTestView : public View {
 public:
  ScrollableTestView() { }

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(100, 10000);
  }

  virtual void Layout() {
    SizeToPreferredSize();
  }
};

class TestViewWithControls : public View {
 public:
  TestViewWithControls() {
    text_field_ = new Textfield();
    AddChildView(text_field_);
  }

  Textfield* text_field_;
};

class SimpleWidgetDelegate : public WidgetDelegate {
 public:
  explicit SimpleWidgetDelegate(View* contents) : contents_(contents) {  }

  virtual void DeleteDelegate() { delete this; }

  virtual View* GetContentsView() { return contents_; }

  virtual Widget* GetWidget() { return contents_->GetWidget(); }
  virtual const Widget* GetWidget() const { return contents_->GetWidget(); }

 private:
  View* contents_;
};

// Tests that the mouse-wheel messages are correctly rerouted to the window
// under the mouse.
// TODO(jcampan): http://crbug.com/10572 Disabled as it fails on the Vista build
//                bot.
// Note that this fails for a variety of reasons:
// - focused view is apparently reset across window activations and never
//   properly restored
// - this test depends on you not having any other window visible open under the
//   area that it opens the test windows. --beng
TEST_F(ViewTest, DISABLED_RerouteMouseWheelTest) {
  TestViewWithControls* view_with_controls = new TestViewWithControls();
  Widget* window1 = Widget::CreateWindowWithBounds(
      new SimpleWidgetDelegate(view_with_controls),
      gfx::Rect(0, 0, 100, 100));
  window1->Show();
  ScrollView* scroll_view = new ScrollView();
  scroll_view->SetContents(new ScrollableTestView());
  Widget* window2 = Widget::CreateWindowWithBounds(
      new SimpleWidgetDelegate(scroll_view),
      gfx::Rect(200, 200, 100, 100));
  window2->Show();
  EXPECT_EQ(0, scroll_view->GetVisibleRect().y());

  // Make the window1 active, as this is what it would be in real-world.
  window1->Activate();

  // Let's send a mouse-wheel message to the different controls and check that
  // it is rerouted to the window under the mouse (effectively scrolling the
  // scroll-view).

  // First to the Window's HWND.
  ::SendMessage(view_with_controls->GetWidget()->GetNativeView(),
                WM_MOUSEWHEEL, MAKEWPARAM(0, -20), MAKELPARAM(250, 250));
  EXPECT_EQ(20, scroll_view->GetVisibleRect().y());

  window1->CloseNow();
  window2->CloseNow();
}
#endif  // 0

////////////////////////////////////////////////////////////////////////////////
// Native view hierachy
////////////////////////////////////////////////////////////////////////////////
class ToplevelWidgetObserverView : public View {
 public:
  ToplevelWidgetObserverView() : toplevel_(NULL) {
  }
  virtual ~ToplevelWidgetObserverView() {
  }

  // View overrides:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    if (details.is_add) {
      toplevel_ = GetWidget() ? GetWidget()->GetTopLevelWidget() : NULL;
    } else {
      toplevel_ = NULL;
    }
  }
  virtual void NativeViewHierarchyChanged() OVERRIDE {
    toplevel_ = GetWidget() ? GetWidget()->GetTopLevelWidget() : NULL;
  }

  Widget* toplevel() { return toplevel_; }

 private:
  Widget* toplevel_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWidgetObserverView);
};

// Test that a view can track the current top level widget by overriding
// View::ViewHierarchyChanged() and View::NativeViewHierarchyChanged().
TEST_F(ViewTest, NativeViewHierarchyChanged) {
  scoped_ptr<Widget> toplevel1(new Widget);
  Widget::InitParams toplevel1_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  toplevel1_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel1->Init(toplevel1_params);

  scoped_ptr<Widget> toplevel2(new Widget);
  Widget::InitParams toplevel2_params =
      CreateParams(Widget::InitParams::TYPE_POPUP);
  toplevel2_params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  toplevel2->Init(toplevel2_params);

  Widget* child = new Widget;
  Widget::InitParams child_params(Widget::InitParams::TYPE_CONTROL);
  child_params.parent = toplevel1->GetNativeView();
  child->Init(child_params);

  ToplevelWidgetObserverView* observer_view =
      new ToplevelWidgetObserverView();
  EXPECT_EQ(NULL, observer_view->toplevel());

  child->SetContentsView(observer_view);
  EXPECT_EQ(toplevel1, observer_view->toplevel());

  Widget::ReparentNativeView(child->GetNativeView(),
                             toplevel2->GetNativeView());
  EXPECT_EQ(toplevel2, observer_view->toplevel());

  observer_view->parent()->RemoveChildView(observer_view);
  EXPECT_EQ(NULL, observer_view->toplevel());

  // Make |observer_view| |child|'s contents view again so that it gets deleted
  // with the widget.
  child->SetContentsView(observer_view);
}

////////////////////////////////////////////////////////////////////////////////
// Transformations
////////////////////////////////////////////////////////////////////////////////

class TransformPaintView : public TestView {
 public:
  TransformPaintView() {}
  virtual ~TransformPaintView() {}

  void ClearScheduledPaintRect() {
    scheduled_paint_rect_ = gfx::Rect();
  }

  gfx::Rect scheduled_paint_rect() const { return scheduled_paint_rect_; }

  // Overridden from View:
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE {
    gfx::Rect xrect = ConvertRectToParent(rect);
    scheduled_paint_rect_.Union(xrect);
  }

 private:
  gfx::Rect scheduled_paint_rect_;

  DISALLOW_COPY_AND_ASSIGN(TransformPaintView);
};

TEST_F(ViewTest, TransformPaint) {
  TransformPaintView* v1 = new TransformPaintView();
  v1->SetBoundsRect(gfx::Rect(0, 0, 500, 300));

  TestView* v2 = new TestView();
  v2->SetBoundsRect(gfx::Rect(100, 100, 200, 100));

  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  widget->Show();
  View* root = widget->GetRootView();

  root->AddChildView(v1);
  v1->AddChildView(v2);

  // At this moment, |v2| occupies (100, 100) to (300, 200) in |root|.
  v1->ClearScheduledPaintRect();
  v2->SchedulePaint();

  EXPECT_EQ(gfx::Rect(100, 100, 200, 100), v1->scheduled_paint_rect());

  // Rotate |v1| counter-clockwise.
  gfx::Transform transform;
  RotateCounterclockwise(&transform);
  transform.matrix().set(1, 3, 500.0);
  v1->SetTransform(transform);

  // |v2| now occupies (100, 200) to (200, 400) in |root|.

  v1->ClearScheduledPaintRect();
  v2->SchedulePaint();

  EXPECT_EQ(gfx::Rect(100, 200, 100, 200), v1->scheduled_paint_rect());

  widget->CloseNow();
}

TEST_F(ViewTest, TransformEvent) {
  TestView* v1 = new TestView();
  v1->SetBoundsRect(gfx::Rect(0, 0, 500, 300));

  TestView* v2 = new TestView();
  v2->SetBoundsRect(gfx::Rect(100, 100, 200, 100));

  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();

  root->AddChildView(v1);
  v1->AddChildView(v2);

  // At this moment, |v2| occupies (100, 100) to (300, 200) in |root|.

  // Rotate |v1| counter-clockwise.
  gfx::Transform transform(v1->GetTransform());
  RotateCounterclockwise(&transform);
  transform.matrix().set(1, 3, 500.0);
  v1->SetTransform(transform);

  // |v2| now occupies (100, 200) to (200, 400) in |root|.
  v1->Reset();
  v2->Reset();

  gfx::Point p1(110, 210);
  ui::MouseEvent pressed(ui::ET_MOUSE_PRESSED, p1, p1,
                         ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(pressed);
  EXPECT_EQ(0, v1->last_mouse_event_type_);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, v2->last_mouse_event_type_);
  EXPECT_EQ(190, v2->location_.x());
  EXPECT_EQ(10, v2->location_.y());

  ui::MouseEvent released(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), 0,
                          0);
  root->OnMouseReleased(released);

  // Now rotate |v2| inside |v1| clockwise.
  transform = v2->GetTransform();
  RotateClockwise(&transform);
  transform.matrix().set(0, 3, 100.f);
  v2->SetTransform(transform);

  // Now, |v2| occupies (100, 100) to (200, 300) in |v1|, and (100, 300) to
  // (300, 400) in |root|.

  v1->Reset();
  v2->Reset();

  gfx::Point point2(110, 320);
  ui::MouseEvent p2(ui::ET_MOUSE_PRESSED, point2, point2,
                    ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(p2);
  EXPECT_EQ(0, v1->last_mouse_event_type_);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, v2->last_mouse_event_type_);
  EXPECT_EQ(10, v2->location_.x());
  EXPECT_EQ(20, v2->location_.y());

  root->OnMouseReleased(released);

  v1->SetTransform(gfx::Transform());
  v2->SetTransform(gfx::Transform());

  TestView* v3 = new TestView();
  v3->SetBoundsRect(gfx::Rect(10, 10, 20, 30));
  v2->AddChildView(v3);

  // Rotate |v3| clockwise with respect to |v2|.
  transform = v1->GetTransform();
  RotateClockwise(&transform);
  transform.matrix().set(0, 3, 30.f);
  v3->SetTransform(transform);

  // Scale |v2| with respect to |v1| along both axis.
  transform = v2->GetTransform();
  transform.matrix().set(0, 0, 0.8f);
  transform.matrix().set(1, 1, 0.5f);
  v2->SetTransform(transform);

  // |v3| occupies (108, 105) to (132, 115) in |root|.

  v1->Reset();
  v2->Reset();
  v3->Reset();

  gfx::Point point(112, 110);
  ui::MouseEvent p3(ui::ET_MOUSE_PRESSED, point, point,
                    ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(p3);

  EXPECT_EQ(ui::ET_MOUSE_PRESSED, v3->last_mouse_event_type_);
  EXPECT_EQ(10, v3->location_.x());
  EXPECT_EQ(25, v3->location_.y());

  root->OnMouseReleased(released);

  v1->SetTransform(gfx::Transform());
  v2->SetTransform(gfx::Transform());
  v3->SetTransform(gfx::Transform());

  v1->Reset();
  v2->Reset();
  v3->Reset();

  // Rotate |v3| clockwise with respect to |v2|, and scale it along both axis.
  transform = v3->GetTransform();
  RotateClockwise(&transform);
  transform.matrix().set(0, 3, 30.f);
  // Rotation sets some scaling transformation. Using SetScale would overwrite
  // that and pollute the rotation. So combine the scaling with the existing
  // transforamtion.
  gfx::Transform scale;
  scale.Scale(0.8f, 0.5f);
  transform.ConcatTransform(scale);
  v3->SetTransform(transform);

  // Translate |v2| with respect to |v1|.
  transform = v2->GetTransform();
  transform.matrix().set(0, 3, 10.f);
  transform.matrix().set(1, 3, 10.f);
  v2->SetTransform(transform);

  // |v3| now occupies (120, 120) to (144, 130) in |root|.

  gfx::Point point3(124, 125);
  ui::MouseEvent p4(ui::ET_MOUSE_PRESSED, point3, point3,
                    ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  root->OnMousePressed(p4);

  EXPECT_EQ(ui::ET_MOUSE_PRESSED, v3->last_mouse_event_type_);
  EXPECT_EQ(10, v3->location_.x());
  EXPECT_EQ(25, v3->location_.y());

  root->OnMouseReleased(released);

  widget->CloseNow();
}

TEST_F(ViewTest, TransformVisibleBound) {
  gfx::Rect viewport_bounds(0, 0, 100, 100);

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = viewport_bounds;
  widget->Init(params);
  widget->GetRootView()->SetBoundsRect(viewport_bounds);

  View* viewport = new View;
  widget->SetContentsView(viewport);
  View* contents = new View;
  viewport->AddChildView(contents);
  viewport->SetBoundsRect(viewport_bounds);
  contents->SetBoundsRect(gfx::Rect(0, 0, 100, 200));

  View* child = new View;
  contents->AddChildView(child);
  child->SetBoundsRect(gfx::Rect(10, 90, 50, 50));
  EXPECT_EQ(gfx::Rect(0, 0, 50, 10), child->GetVisibleBounds());

  // Rotate |child| counter-clockwise
  gfx::Transform transform;
  RotateCounterclockwise(&transform);
  transform.matrix().set(1, 3, 50.f);
  child->SetTransform(transform);
  EXPECT_EQ(gfx::Rect(40, 0, 10, 50), child->GetVisibleBounds());

  widget->CloseNow();
}

////////////////////////////////////////////////////////////////////////////////
// OnVisibleBoundsChanged()

class VisibleBoundsView : public View {
 public:
  VisibleBoundsView() : received_notification_(false) {}
  virtual ~VisibleBoundsView() {}

  bool received_notification() const { return received_notification_; }
  void set_received_notification(bool received) {
    received_notification_ = received;
  }

 private:
  // Overridden from View:
  virtual bool NeedsNotificationWhenVisibleBoundsChange() const OVERRIDE {
     return true;
  }
  virtual void OnVisibleBoundsChanged() OVERRIDE {
    received_notification_ = true;
  }

  bool received_notification_;

  DISALLOW_COPY_AND_ASSIGN(VisibleBoundsView);
};

TEST_F(ViewTest, OnVisibleBoundsChanged) {
  gfx::Rect viewport_bounds(0, 0, 100, 100);

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = viewport_bounds;
  widget->Init(params);
  widget->GetRootView()->SetBoundsRect(viewport_bounds);

  View* viewport = new View;
  widget->SetContentsView(viewport);
  View* contents = new View;
  viewport->AddChildView(contents);
  viewport->SetBoundsRect(viewport_bounds);
  contents->SetBoundsRect(gfx::Rect(0, 0, 100, 200));

  // Create a view that cares about visible bounds notifications, and position
  // it just outside the visible bounds of the viewport.
  VisibleBoundsView* child = new VisibleBoundsView;
  contents->AddChildView(child);
  child->SetBoundsRect(gfx::Rect(10, 110, 50, 50));

  // The child bound should be fully clipped.
  EXPECT_TRUE(child->GetVisibleBounds().IsEmpty());

  // Now scroll the contents, but not enough to make the child visible.
  contents->SetY(contents->y() - 1);

  // We should have received the notification since the visible bounds may have
  // changed (even though they didn't).
  EXPECT_TRUE(child->received_notification());
  EXPECT_TRUE(child->GetVisibleBounds().IsEmpty());
  child->set_received_notification(false);

  // Now scroll the contents, this time by enough to make the child visible by
  // one pixel.
  contents->SetY(contents->y() - 10);
  EXPECT_TRUE(child->received_notification());
  EXPECT_EQ(1, child->GetVisibleBounds().height());
  child->set_received_notification(false);

  widget->CloseNow();
}

TEST_F(ViewTest, SetBoundsPaint) {
  TestView top_view;
  TestView* child_view = new TestView;

  top_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  top_view.scheduled_paint_rects_.clear();
  child_view->SetBoundsRect(gfx::Rect(10, 10, 20, 20));
  top_view.AddChildView(child_view);

  top_view.scheduled_paint_rects_.clear();
  child_view->SetBoundsRect(gfx::Rect(30, 30, 20, 20));
  EXPECT_EQ(2U, top_view.scheduled_paint_rects_.size());

  // There should be 2 rects, spanning from (10, 10) to (50, 50).
  gfx::Rect paint_rect = top_view.scheduled_paint_rects_[0];
  paint_rect.Union(top_view.scheduled_paint_rects_[1]);
  EXPECT_EQ(gfx::Rect(10, 10, 40, 40), paint_rect);
}

// Assertions around painting and focus gain/lost.
TEST_F(ViewTest, FocusBlurPaints) {
  TestView parent_view;
  TestView* child_view1 = new TestView;  // Owned by |parent_view|.

  parent_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  child_view1->SetBoundsRect(gfx::Rect(0, 0, 20, 20));
  parent_view.AddChildView(child_view1);

  parent_view.scheduled_paint_rects_.clear();
  child_view1->scheduled_paint_rects_.clear();

  // Focus change shouldn't trigger paints.
  child_view1->DoFocus();

  EXPECT_TRUE(parent_view.scheduled_paint_rects_.empty());
  EXPECT_TRUE(child_view1->scheduled_paint_rects_.empty());

  child_view1->DoBlur();
  EXPECT_TRUE(parent_view.scheduled_paint_rects_.empty());
  EXPECT_TRUE(child_view1->scheduled_paint_rects_.empty());
}

// Verifies SetBounds(same bounds) doesn't trigger a SchedulePaint().
TEST_F(ViewTest, SetBoundsSameBoundsDoesntSchedulePaint) {
  TestView view;

  view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  view.InvalidateLayout();
  view.scheduled_paint_rects_.clear();
  view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  EXPECT_TRUE(view.scheduled_paint_rects_.empty());
}

// Verifies AddChildView() and RemoveChildView() schedule appropriate paints.
TEST_F(ViewTest, AddAndRemoveSchedulePaints) {
  gfx::Rect viewport_bounds(0, 0, 100, 100);

  // We have to put the View hierarchy into a Widget or no paints will be
  // scheduled.
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = viewport_bounds;
  widget->Init(params);
  widget->GetRootView()->SetBoundsRect(viewport_bounds);

  TestView* parent_view = new TestView;
  widget->SetContentsView(parent_view);
  parent_view->SetBoundsRect(viewport_bounds);
  parent_view->scheduled_paint_rects_.clear();

  View* child_view = new View;
  child_view->SetBoundsRect(gfx::Rect(0, 0, 20, 20));
  parent_view->AddChildView(child_view);
  ASSERT_EQ(1U, parent_view->scheduled_paint_rects_.size());
  EXPECT_EQ(child_view->bounds(), parent_view->scheduled_paint_rects_.front());

  parent_view->scheduled_paint_rects_.clear();
  parent_view->RemoveChildView(child_view);
  scoped_ptr<View> child_deleter(child_view);
  ASSERT_EQ(1U, parent_view->scheduled_paint_rects_.size());
  EXPECT_EQ(child_view->bounds(), parent_view->scheduled_paint_rects_.front());

  widget->CloseNow();
}

// Tests conversion methods with a transform.
TEST_F(ViewTest, ConversionsWithTransform) {
  TestView top_view;

  // View hierarchy used to test scale transforms.
  TestView* child = new TestView;
  TestView* child_child = new TestView;

  // View used to test a rotation transform.
  TestView* child_2 = new TestView;

  top_view.AddChildView(child);
  child->AddChildView(child_child);

  top_view.SetBoundsRect(gfx::Rect(0, 0, 1000, 1000));

  child->SetBoundsRect(gfx::Rect(7, 19, 500, 500));
  gfx::Transform transform;
  transform.Scale(3.0, 4.0);
  child->SetTransform(transform);

  child_child->SetBoundsRect(gfx::Rect(17, 13, 100, 100));
  transform.MakeIdentity();
  transform.Scale(5.0, 7.0);
  child_child->SetTransform(transform);

  top_view.AddChildView(child_2);
  child_2->SetBoundsRect(gfx::Rect(700, 725, 100, 100));
  transform.MakeIdentity();
  RotateClockwise(&transform);
  child_2->SetTransform(transform);

  // Sanity check to make sure basic transforms act as expected.
  {
    gfx::Transform transform;
    transform.Translate(110.0, -110.0);
    transform.Scale(100.0, 55.0);
    transform.Translate(1.0, 1.0);

    // convert to a 3x3 matrix.
    const SkMatrix& matrix = transform.matrix();

    EXPECT_EQ(210, matrix.getTranslateX());
    EXPECT_EQ(-55, matrix.getTranslateY());
    EXPECT_EQ(100, matrix.getScaleX());
    EXPECT_EQ(55, matrix.getScaleY());
    EXPECT_EQ(0, matrix.getSkewX());
    EXPECT_EQ(0, matrix.getSkewY());
  }

  {
    gfx::Transform transform;
    transform.Translate(1.0, 1.0);
    gfx::Transform t2;
    t2.Scale(100.0, 55.0);
    gfx::Transform t3;
    t3.Translate(110.0, -110.0);
    transform.ConcatTransform(t2);
    transform.ConcatTransform(t3);

    // convert to a 3x3 matrix
    const SkMatrix& matrix = transform.matrix();

    EXPECT_EQ(210, matrix.getTranslateX());
    EXPECT_EQ(-55, matrix.getTranslateY());
    EXPECT_EQ(100, matrix.getScaleX());
    EXPECT_EQ(55, matrix.getScaleY());
    EXPECT_EQ(0, matrix.getSkewX());
    EXPECT_EQ(0, matrix.getSkewY());
  }

  // Conversions from child->top and top->child.
  {
    gfx::Point point(5, 5);
    View::ConvertPointToTarget(child, &top_view, &point);
    EXPECT_EQ(22, point.x());
    EXPECT_EQ(39, point.y());

    gfx::RectF rect(5.0f, 5.0f, 10.0f, 20.0f);
    View::ConvertRectToTarget(child, &top_view, &rect);
    EXPECT_FLOAT_EQ(22.0f, rect.x());
    EXPECT_FLOAT_EQ(39.0f, rect.y());
    EXPECT_FLOAT_EQ(30.0f, rect.width());
    EXPECT_FLOAT_EQ(80.0f, rect.height());

    point.SetPoint(22, 39);
    View::ConvertPointToTarget(&top_view, child, &point);
    EXPECT_EQ(5, point.x());
    EXPECT_EQ(5, point.y());

    rect.SetRect(22.0f, 39.0f, 30.0f, 80.0f);
    View::ConvertRectToTarget(&top_view, child, &rect);
    EXPECT_FLOAT_EQ(5.0f, rect.x());
    EXPECT_FLOAT_EQ(5.0f, rect.y());
    EXPECT_FLOAT_EQ(10.0f, rect.width());
    EXPECT_FLOAT_EQ(20.0f, rect.height());
  }

  // Conversions from child_child->top and top->child_child.
  {
    gfx::Point point(5, 5);
    View::ConvertPointToTarget(child_child, &top_view, &point);
    EXPECT_EQ(133, point.x());
    EXPECT_EQ(211, point.y());

    gfx::RectF rect(5.0f, 5.0f, 10.0f, 20.0f);
    View::ConvertRectToTarget(child_child, &top_view, &rect);
    EXPECT_FLOAT_EQ(133.0f, rect.x());
    EXPECT_FLOAT_EQ(211.0f, rect.y());
    EXPECT_FLOAT_EQ(150.0f, rect.width());
    EXPECT_FLOAT_EQ(560.0f, rect.height());

    point.SetPoint(133, 211);
    View::ConvertPointToTarget(&top_view, child_child, &point);
    EXPECT_EQ(5, point.x());
    EXPECT_EQ(5, point.y());

    rect.SetRect(133.0f, 211.0f, 150.0f, 560.0f);
    View::ConvertRectToTarget(&top_view, child_child, &rect);
    EXPECT_FLOAT_EQ(5.0f, rect.x());
    EXPECT_FLOAT_EQ(5.0f, rect.y());
    EXPECT_FLOAT_EQ(10.0f, rect.width());
    EXPECT_FLOAT_EQ(20.0f, rect.height());
  }

  // Conversions from child_child->child and child->child_child
  {
    gfx::Point point(5, 5);
    View::ConvertPointToTarget(child_child, child, &point);
    EXPECT_EQ(42, point.x());
    EXPECT_EQ(48, point.y());

    gfx::RectF rect(5.0f, 5.0f, 10.0f, 20.0f);
    View::ConvertRectToTarget(child_child, child, &rect);
    EXPECT_FLOAT_EQ(42.0f, rect.x());
    EXPECT_FLOAT_EQ(48.0f, rect.y());
    EXPECT_FLOAT_EQ(50.0f, rect.width());
    EXPECT_FLOAT_EQ(140.0f, rect.height());

    point.SetPoint(42, 48);
    View::ConvertPointToTarget(child, child_child, &point);
    EXPECT_EQ(5, point.x());
    EXPECT_EQ(5, point.y());

    rect.SetRect(42.0f, 48.0f, 50.0f, 140.0f);
    View::ConvertRectToTarget(child, child_child, &rect);
    EXPECT_FLOAT_EQ(5.0f, rect.x());
    EXPECT_FLOAT_EQ(5.0f, rect.y());
    EXPECT_FLOAT_EQ(10.0f, rect.width());
    EXPECT_FLOAT_EQ(20.0f, rect.height());
  }

  // Conversions from top_view to child with a value that should be negative.
  // This ensures we don't round up with negative numbers.
  {
    gfx::Point point(6, 18);
    View::ConvertPointToTarget(&top_view, child, &point);
    EXPECT_EQ(-1, point.x());
    EXPECT_EQ(-1, point.y());

    float error = 0.01f;
    gfx::RectF rect(6.0f, 18.0f, 10.0f, 39.0f);
    View::ConvertRectToTarget(&top_view, child, &rect);
    EXPECT_NEAR(-0.33f, rect.x(), error);
    EXPECT_NEAR(-0.25f, rect.y(), error);
    EXPECT_NEAR(3.33f, rect.width(), error);
    EXPECT_NEAR(9.75f, rect.height(), error);
  }

  // Rect conversions from top_view->child_2 and child_2->top_view.
  {
    gfx::RectF rect(50.0f, 55.0f, 20.0f, 30.0f);
    View::ConvertRectToTarget(child_2, &top_view, &rect);
    EXPECT_FLOAT_EQ(615.0f, rect.x());
    EXPECT_FLOAT_EQ(775.0f, rect.y());
    EXPECT_FLOAT_EQ(30.0f, rect.width());
    EXPECT_FLOAT_EQ(20.0f, rect.height());

    rect.SetRect(615.0f, 775.0f, 30.0f, 20.0f);
    View::ConvertRectToTarget(&top_view, child_2, &rect);
    EXPECT_FLOAT_EQ(50.0f, rect.x());
    EXPECT_FLOAT_EQ(55.0f, rect.y());
    EXPECT_FLOAT_EQ(20.0f, rect.width());
    EXPECT_FLOAT_EQ(30.0f, rect.height());
  }
}

// Tests conversion methods to and from screen coordinates.
TEST_F(ViewTest, ConversionsToFromScreen) {
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);

  View* child = new View;
  widget->GetRootView()->AddChildView(child);
  child->SetBounds(10, 10, 100, 200);
  gfx::Transform t;
  t.Scale(0.5, 0.5);
  child->SetTransform(t);

  gfx::Point point_in_screen(100, 90);
  gfx::Point point_in_child(80,60);

  gfx::Point point = point_in_screen;
  View::ConvertPointFromScreen(child, &point);
  EXPECT_EQ(point_in_child.ToString(), point.ToString());

  View::ConvertPointToScreen(child, &point);
  EXPECT_EQ(point_in_screen.ToString(), point.ToString());
}

// Tests conversion methods for rectangles.
TEST_F(ViewTest, ConvertRectWithTransform) {
  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(50, 50, 650, 650);
  widget->Init(params);
  View* root = widget->GetRootView();

  TestView* v1 = new TestView;
  TestView* v2 = new TestView;
  root->AddChildView(v1);
  v1->AddChildView(v2);

  v1->SetBoundsRect(gfx::Rect(10, 10, 500, 500));
  v2->SetBoundsRect(gfx::Rect(20, 20, 100, 200));

  // |v2| now occupies (30, 30) to (130, 230) in |widget|
  gfx::Rect rect(5, 5, 15, 40);
  EXPECT_EQ(gfx::Rect(25, 25, 15, 40), v2->ConvertRectToParent(rect));
  EXPECT_EQ(gfx::Rect(35, 35, 15, 40), v2->ConvertRectToWidget(rect));

  // Rotate |v2|
  gfx::Transform t2;
  RotateCounterclockwise(&t2);
  t2.matrix().set(1, 3, 100.f);
  v2->SetTransform(t2);

  // |v2| now occupies (30, 30) to (230, 130) in |widget|
  EXPECT_EQ(gfx::Rect(25, 100, 40, 15), v2->ConvertRectToParent(rect));
  EXPECT_EQ(gfx::Rect(35, 110, 40, 15), v2->ConvertRectToWidget(rect));

  // Scale down |v1|
  gfx::Transform t1;
  t1.Scale(0.5, 0.5);
  v1->SetTransform(t1);

  // The rectangle should remain the same for |v1|.
  EXPECT_EQ(gfx::Rect(25, 100, 40, 15), v2->ConvertRectToParent(rect));

  // |v2| now occupies (20, 20) to (120, 70) in |widget|
  EXPECT_EQ(gfx::Rect(22, 60, 21, 8).ToString(),
            v2->ConvertRectToWidget(rect).ToString());

  widget->CloseNow();
}

class ObserverView : public View {
 public:
  ObserverView();
  virtual ~ObserverView();

  void ResetTestState();

  bool has_add_details() const { return has_add_details_; }
  bool has_remove_details() const { return has_remove_details_; }

  const ViewHierarchyChangedDetails& add_details() const {
    return add_details_;
  }

  const ViewHierarchyChangedDetails& remove_details() const {
    return remove_details_;
  }

 private:
  // View:
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;

  bool has_add_details_;
  bool has_remove_details_;
  ViewHierarchyChangedDetails add_details_;
  ViewHierarchyChangedDetails remove_details_;

  DISALLOW_COPY_AND_ASSIGN(ObserverView);
};

ObserverView::ObserverView()
    : has_add_details_(false),
      has_remove_details_(false) {
}

ObserverView::~ObserverView() {}

void ObserverView::ResetTestState() {
  has_add_details_ = false;
  has_remove_details_ = false;
  add_details_ = ViewHierarchyChangedDetails();
  remove_details_ = ViewHierarchyChangedDetails();
}

void ObserverView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add) {
    has_add_details_ = true;
    add_details_ = details;
  } else {
    has_remove_details_ = true;
    remove_details_ = details;
  }
}

// Verifies that the ViewHierarchyChanged() notification is sent correctly when
// a child view is added or removed to all the views in the hierarchy (up and
// down).
// The tree looks like this:
// v1
// +-- v2
//     +-- v3
//     +-- v4 (starts here, then get reparented to v1)
TEST_F(ViewTest, ViewHierarchyChanged) {
  ObserverView v1;

  ObserverView* v3 = new ObserverView();

  // Add |v3| to |v2|.
  scoped_ptr<ObserverView> v2(new ObserverView());
  v2->AddChildView(v3);

  // Make sure both |v2| and |v3| receive the ViewHierarchyChanged()
  // notification.
  EXPECT_TRUE(v2->has_add_details());
  EXPECT_FALSE(v2->has_remove_details());
  EXPECT_EQ(v2.get(), v2->add_details().parent);
  EXPECT_EQ(v3, v2->add_details().child);
  EXPECT_EQ(NULL, v2->add_details().move_view);

  EXPECT_TRUE(v3->has_add_details());
  EXPECT_FALSE(v3->has_remove_details());
  EXPECT_EQ(v2.get(), v3->add_details().parent);
  EXPECT_EQ(v3, v3->add_details().child);
  EXPECT_EQ(NULL, v3->add_details().move_view);

  // Reset everything to the initial state.
  v2->ResetTestState();
  v3->ResetTestState();

  // Add |v2| to v1.
  v1.AddChildView(v2.get());

  // Verifies that |v2| is the child view *added* and the parent view is |v1|.
  // Make sure all the views (v1, v2, v3) received _that_ information.
  EXPECT_TRUE(v1.has_add_details());
  EXPECT_FALSE(v1.has_remove_details());
  EXPECT_EQ(&v1, v1.add_details().parent);
  EXPECT_EQ(v2.get(), v1.add_details().child);
  EXPECT_EQ(NULL, v1.add_details().move_view);

  EXPECT_TRUE(v2->has_add_details());
  EXPECT_FALSE(v2->has_remove_details());
  EXPECT_EQ(&v1, v2->add_details().parent);
  EXPECT_EQ(v2.get(), v2->add_details().child);
  EXPECT_EQ(NULL, v2->add_details().move_view);

  EXPECT_TRUE(v3->has_add_details());
  EXPECT_FALSE(v3->has_remove_details());
  EXPECT_EQ(&v1, v3->add_details().parent);
  EXPECT_EQ(v2.get(), v3->add_details().child);
  EXPECT_EQ(NULL, v3->add_details().move_view);

  // Reset everything to the initial state.
  v1.ResetTestState();
  v2->ResetTestState();
  v3->ResetTestState();

  // Remove |v2| from |v1|.
  v1.RemoveChildView(v2.get());

  // Verifies that |v2| is the child view *removed* and the parent view is |v1|.
  // Make sure all the views (v1, v2, v3) received _that_ information.
  EXPECT_FALSE(v1.has_add_details());
  EXPECT_TRUE(v1.has_remove_details());
  EXPECT_EQ(&v1, v1.remove_details().parent);
  EXPECT_EQ(v2.get(), v1.remove_details().child);
  EXPECT_EQ(NULL, v1.remove_details().move_view);

  EXPECT_FALSE(v2->has_add_details());
  EXPECT_TRUE(v2->has_remove_details());
  EXPECT_EQ(&v1, v2->remove_details().parent);
  EXPECT_EQ(v2.get(), v2->remove_details().child);
  EXPECT_EQ(NULL, v2->remove_details().move_view);

  EXPECT_FALSE(v3->has_add_details());
  EXPECT_TRUE(v3->has_remove_details());
  EXPECT_EQ(&v1, v3->remove_details().parent);
  EXPECT_EQ(v3, v3->remove_details().child);
  EXPECT_EQ(NULL, v3->remove_details().move_view);

  // Verifies notifications when reparenting a view.
  ObserverView* v4 = new ObserverView();
  // Add |v4| to |v2|.
  v2->AddChildView(v4);

  // Reset everything to the initial state.
  v1.ResetTestState();
  v2->ResetTestState();
  v3->ResetTestState();
  v4->ResetTestState();

  // Reparent |v4| to |v1|.
  v1.AddChildView(v4);

  // Verifies that all views receive the correct information for all the child,
  // parent and move views.

  // |v1| is the new parent, |v4| is the child for add, |v2| is the old parent.
  EXPECT_TRUE(v1.has_add_details());
  EXPECT_FALSE(v1.has_remove_details());
  EXPECT_EQ(&v1, v1.add_details().parent);
  EXPECT_EQ(v4, v1.add_details().child);
  EXPECT_EQ(v2.get(), v1.add_details().move_view);

  // |v2| is the old parent, |v4| is the child for remove, |v1| is the new
  // parent.
  EXPECT_FALSE(v2->has_add_details());
  EXPECT_TRUE(v2->has_remove_details());
  EXPECT_EQ(v2.get(), v2->remove_details().parent);
  EXPECT_EQ(v4, v2->remove_details().child);
  EXPECT_EQ(&v1, v2->remove_details().move_view);

  // |v3| is not impacted by this operation, and hence receives no notification.
  EXPECT_FALSE(v3->has_add_details());
  EXPECT_FALSE(v3->has_remove_details());

  // |v4| is the reparented child, so it receives notifications for the remove
  // and then the add.  |v2| is its old parent, |v1| is its new parent.
  EXPECT_TRUE(v4->has_remove_details());
  EXPECT_TRUE(v4->has_add_details());
  EXPECT_EQ(v2.get(), v4->remove_details().parent);
  EXPECT_EQ(&v1, v4->add_details().parent);
  EXPECT_EQ(v4, v4->add_details().child);
  EXPECT_EQ(v4, v4->remove_details().child);
  EXPECT_EQ(&v1, v4->remove_details().move_view);
  EXPECT_EQ(v2.get(), v4->add_details().move_view);
}

// Verifies if the child views added under the root are all deleted when calling
// RemoveAllChildViews.
// The tree looks like this:
// root
// +-- child1
//     +-- foo
//         +-- bar0
//         +-- bar1
//         +-- bar2
// +-- child2
// +-- child3
TEST_F(ViewTest, RemoveAllChildViews) {
  View root;

  View* child1 = new View;
  root.AddChildView(child1);

  for (int i = 0; i < 2; ++i)
    root.AddChildView(new View);

  View* foo = new View;
  child1->AddChildView(foo);

  // Add some nodes to |foo|.
  for (int i = 0; i < 3; ++i)
    foo->AddChildView(new View);

  EXPECT_EQ(3, root.child_count());
  EXPECT_EQ(1, child1->child_count());
  EXPECT_EQ(3, foo->child_count());

  // Now remove all child views from root.
  root.RemoveAllChildViews(true);

  EXPECT_EQ(0, root.child_count());
  EXPECT_FALSE(root.has_children());
}

TEST_F(ViewTest, Contains) {
  View v1;
  View* v2 = new View;
  View* v3 = new View;

  v1.AddChildView(v2);
  v2->AddChildView(v3);

  EXPECT_FALSE(v1.Contains(NULL));
  EXPECT_TRUE(v1.Contains(&v1));
  EXPECT_TRUE(v1.Contains(v2));
  EXPECT_TRUE(v1.Contains(v3));

  EXPECT_FALSE(v2->Contains(NULL));
  EXPECT_TRUE(v2->Contains(v2));
  EXPECT_FALSE(v2->Contains(&v1));
  EXPECT_TRUE(v2->Contains(v3));

  EXPECT_FALSE(v3->Contains(NULL));
  EXPECT_TRUE(v3->Contains(v3));
  EXPECT_FALSE(v3->Contains(&v1));
  EXPECT_FALSE(v3->Contains(v2));
}

// Verifies if GetIndexOf() returns the correct index for the specified child
// view.
// The tree looks like this:
// root
// +-- child1
//     +-- foo1
// +-- child2
TEST_F(ViewTest, GetIndexOf) {
  View root;

  View* child1 = new View;
  root.AddChildView(child1);

  View* child2 = new View;
  root.AddChildView(child2);

  View* foo1 = new View;
  child1->AddChildView(foo1);

  EXPECT_EQ(-1, root.GetIndexOf(NULL));
  EXPECT_EQ(-1, root.GetIndexOf(&root));
  EXPECT_EQ(0, root.GetIndexOf(child1));
  EXPECT_EQ(1, root.GetIndexOf(child2));
  EXPECT_EQ(-1, root.GetIndexOf(foo1));

  EXPECT_EQ(-1, child1->GetIndexOf(NULL));
  EXPECT_EQ(-1, child1->GetIndexOf(&root));
  EXPECT_EQ(-1, child1->GetIndexOf(child1));
  EXPECT_EQ(-1, child1->GetIndexOf(child2));
  EXPECT_EQ(0, child1->GetIndexOf(foo1));

  EXPECT_EQ(-1, child2->GetIndexOf(NULL));
  EXPECT_EQ(-1, child2->GetIndexOf(&root));
  EXPECT_EQ(-1, child2->GetIndexOf(child2));
  EXPECT_EQ(-1, child2->GetIndexOf(child1));
  EXPECT_EQ(-1, child2->GetIndexOf(foo1));
}

// Verifies that the child views can be reordered correctly.
TEST_F(ViewTest, ReorderChildren) {
  View root;

  View* child = new View();
  root.AddChildView(child);

  View* foo1 = new View();
  child->AddChildView(foo1);
  View* foo2 = new View();
  child->AddChildView(foo2);
  View* foo3 = new View();
  child->AddChildView(foo3);
  foo1->SetFocusable(true);
  foo2->SetFocusable(true);
  foo3->SetFocusable(true);

  ASSERT_EQ(0, child->GetIndexOf(foo1));
  ASSERT_EQ(1, child->GetIndexOf(foo2));
  ASSERT_EQ(2, child->GetIndexOf(foo3));
  ASSERT_EQ(foo2, foo1->GetNextFocusableView());
  ASSERT_EQ(foo3, foo2->GetNextFocusableView());
  ASSERT_EQ(NULL, foo3->GetNextFocusableView());

  // Move |foo2| at the end.
  child->ReorderChildView(foo2, -1);
  ASSERT_EQ(0, child->GetIndexOf(foo1));
  ASSERT_EQ(1, child->GetIndexOf(foo3));
  ASSERT_EQ(2, child->GetIndexOf(foo2));
  ASSERT_EQ(foo3, foo1->GetNextFocusableView());
  ASSERT_EQ(foo2, foo3->GetNextFocusableView());
  ASSERT_EQ(NULL, foo2->GetNextFocusableView());

  // Move |foo1| at the end.
  child->ReorderChildView(foo1, -1);
  ASSERT_EQ(0, child->GetIndexOf(foo3));
  ASSERT_EQ(1, child->GetIndexOf(foo2));
  ASSERT_EQ(2, child->GetIndexOf(foo1));
  ASSERT_EQ(NULL, foo1->GetNextFocusableView());
  ASSERT_EQ(foo2, foo1->GetPreviousFocusableView());
  ASSERT_EQ(foo2, foo3->GetNextFocusableView());
  ASSERT_EQ(foo1, foo2->GetNextFocusableView());

  // Move |foo2| to the front.
  child->ReorderChildView(foo2, 0);
  ASSERT_EQ(0, child->GetIndexOf(foo2));
  ASSERT_EQ(1, child->GetIndexOf(foo3));
  ASSERT_EQ(2, child->GetIndexOf(foo1));
  ASSERT_EQ(NULL, foo1->GetNextFocusableView());
  ASSERT_EQ(foo3, foo1->GetPreviousFocusableView());
  ASSERT_EQ(foo3, foo2->GetNextFocusableView());
  ASSERT_EQ(foo1, foo3->GetNextFocusableView());
}

// Verifies that GetViewByID returns the correctly child view from the specified
// ID.
// The tree looks like this:
// v1
// +-- v2
//     +-- v3
//     +-- v4
TEST_F(ViewTest, GetViewByID) {
  View v1;
  const int kV1ID = 1;
  v1.set_id(kV1ID);

  View v2;
  const int kV2ID = 2;
  v2.set_id(kV2ID);

  View v3;
  const int kV3ID = 3;
  v3.set_id(kV3ID);

  View v4;
  const int kV4ID = 4;
  v4.set_id(kV4ID);

  const int kV5ID = 5;

  v1.AddChildView(&v2);
  v2.AddChildView(&v3);
  v2.AddChildView(&v4);

  EXPECT_EQ(&v1, v1.GetViewByID(kV1ID));
  EXPECT_EQ(&v2, v1.GetViewByID(kV2ID));
  EXPECT_EQ(&v4, v1.GetViewByID(kV4ID));

  EXPECT_EQ(NULL, v1.GetViewByID(kV5ID));  // No V5 exists.
  EXPECT_EQ(NULL, v2.GetViewByID(kV1ID));  // It can get only from child views.

  const int kGroup = 1;
  v3.SetGroup(kGroup);
  v4.SetGroup(kGroup);

  View::Views views;
  v1.GetViewsInGroup(kGroup, &views);
  EXPECT_EQ(2U, views.size());

  View::Views::const_iterator i(std::find(views.begin(), views.end(), &v3));
  EXPECT_NE(views.end(), i);

  i = std::find(views.begin(), views.end(), &v4);
  EXPECT_NE(views.end(), i);
}

TEST_F(ViewTest, AddExistingChild) {
  View v1, v2, v3;

  v1.AddChildView(&v2);
  v1.AddChildView(&v3);
  EXPECT_EQ(0, v1.GetIndexOf(&v2));
  EXPECT_EQ(1, v1.GetIndexOf(&v3));

  // Check that there's no change in order when adding at same index.
  v1.AddChildViewAt(&v2, 0);
  EXPECT_EQ(0, v1.GetIndexOf(&v2));
  EXPECT_EQ(1, v1.GetIndexOf(&v3));
  v1.AddChildViewAt(&v3, 1);
  EXPECT_EQ(0, v1.GetIndexOf(&v2));
  EXPECT_EQ(1, v1.GetIndexOf(&v3));

  // Add it at a different index and check for change in order.
  v1.AddChildViewAt(&v2, 1);
  EXPECT_EQ(1, v1.GetIndexOf(&v2));
  EXPECT_EQ(0, v1.GetIndexOf(&v3));
  v1.AddChildViewAt(&v2, 0);
  EXPECT_EQ(0, v1.GetIndexOf(&v2));
  EXPECT_EQ(1, v1.GetIndexOf(&v3));

  // Check that calling |AddChildView()| does not change the order.
  v1.AddChildView(&v2);
  EXPECT_EQ(0, v1.GetIndexOf(&v2));
  EXPECT_EQ(1, v1.GetIndexOf(&v3));
  v1.AddChildView(&v3);
  EXPECT_EQ(0, v1.GetIndexOf(&v2));
  EXPECT_EQ(1, v1.GetIndexOf(&v3));
}

////////////////////////////////////////////////////////////////////////////////
// Layers
////////////////////////////////////////////////////////////////////////////////

namespace {

// Test implementation of LayerAnimator.
class TestLayerAnimator : public ui::LayerAnimator {
 public:
  TestLayerAnimator();

  const gfx::Rect& last_bounds() const { return last_bounds_; }

  // LayerAnimator.
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;

 protected:
  virtual ~TestLayerAnimator() { }

 private:
  gfx::Rect last_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimator);
};

TestLayerAnimator::TestLayerAnimator()
    : ui::LayerAnimator(base::TimeDelta::FromMilliseconds(0)) {
}

void TestLayerAnimator::SetBounds(const gfx::Rect& bounds) {
  last_bounds_ = bounds;
}

}  // namespace

class ViewLayerTest : public ViewsTestBase {
 public:
  ViewLayerTest() : widget_(NULL) {}

  virtual ~ViewLayerTest() {
  }

  // Returns the Layer used by the RootView.
  ui::Layer* GetRootLayer() {
    return widget()->GetLayer();
  }

  virtual void SetUp() OVERRIDE {
    ViewTest::SetUp();
    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(50, 50, 200, 200);
    widget_->Init(params);
    widget_->Show();
    widget_->GetRootView()->SetBounds(0, 0, 200, 200);
  }

  virtual void TearDown() OVERRIDE {
    widget_->CloseNow();
    ViewsTestBase::TearDown();
  }

  Widget* widget() { return widget_; }

 private:
  Widget* widget_;
};


TEST_F(ViewLayerTest, LayerToggling) {
  // Because we lazily create textures the calls to DrawTree are necessary to
  // ensure we trigger creation of textures.
  ui::Layer* root_layer = widget()->GetLayer();
  View* content_view = new View;
  widget()->SetContentsView(content_view);

  // Create v1, give it a bounds and verify everything is set up correctly.
  View* v1 = new View;
  v1->SetPaintToLayer(true);
  EXPECT_TRUE(v1->layer() != NULL);
  v1->SetBoundsRect(gfx::Rect(20, 30, 140, 150));
  content_view->AddChildView(v1);
  ASSERT_TRUE(v1->layer() != NULL);
  EXPECT_EQ(root_layer, v1->layer()->parent());
  EXPECT_EQ(gfx::Rect(20, 30, 140, 150), v1->layer()->bounds());

  // Create v2 as a child of v1 and do basic assertion testing.
  View* v2 = new View;
  v1->AddChildView(v2);
  EXPECT_TRUE(v2->layer() == NULL);
  v2->SetBoundsRect(gfx::Rect(10, 20, 30, 40));
  v2->SetPaintToLayer(true);
  ASSERT_TRUE(v2->layer() != NULL);
  EXPECT_EQ(v1->layer(), v2->layer()->parent());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), v2->layer()->bounds());

  // Turn off v1s layer. v2 should still have a layer but its parent should have
  // changed.
  v1->SetPaintToLayer(false);
  EXPECT_TRUE(v1->layer() == NULL);
  EXPECT_TRUE(v2->layer() != NULL);
  EXPECT_EQ(root_layer, v2->layer()->parent());
  ASSERT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(root_layer->children()[0], v2->layer());
  // The bounds of the layer should have changed to be relative to the root view
  // now.
  EXPECT_EQ(gfx::Rect(30, 50, 30, 40), v2->layer()->bounds());

  // Make v1 have a layer again and verify v2s layer is wired up correctly.
  gfx::Transform transform;
  transform.Scale(2.0, 2.0);
  v1->SetTransform(transform);
  EXPECT_TRUE(v1->layer() != NULL);
  EXPECT_TRUE(v2->layer() != NULL);
  EXPECT_EQ(root_layer, v1->layer()->parent());
  EXPECT_EQ(v1->layer(), v2->layer()->parent());
  ASSERT_EQ(1u, root_layer->children().size());
  EXPECT_EQ(root_layer->children()[0], v1->layer());
  ASSERT_EQ(1u, v1->layer()->children().size());
  EXPECT_EQ(v1->layer()->children()[0], v2->layer());
  EXPECT_EQ(gfx::Rect(10, 20, 30, 40), v2->layer()->bounds());
}

// Verifies turning on a layer wires up children correctly.
TEST_F(ViewLayerTest, NestedLayerToggling) {
  View* content_view = new View;
  widget()->SetContentsView(content_view);

  // Create v1, give it a bounds and verify everything is set up correctly.
  View* v1 = new View;
  content_view->AddChildView(v1);
  v1->SetBoundsRect(gfx::Rect(20, 30, 140, 150));

  View* v2 = new View;
  v1->AddChildView(v2);

  View* v3 = new View;
  v3->SetPaintToLayer(true);
  v2->AddChildView(v3);
  ASSERT_TRUE(v3->layer() != NULL);

  // At this point we have v1-v2-v3. v3 has a layer, v1 and v2 don't.

  v1->SetPaintToLayer(true);
  EXPECT_EQ(v1->layer(), v3->layer()->parent());
}

TEST_F(ViewLayerTest, LayerAnimator) {
  View* content_view = new View;
  widget()->SetContentsView(content_view);

  View* v1 = new View;
  content_view->AddChildView(v1);
  v1->SetPaintToLayer(true);
  EXPECT_TRUE(v1->layer() != NULL);

  TestLayerAnimator* animator = new TestLayerAnimator();
  v1->layer()->SetAnimator(animator);

  gfx::Rect bounds(1, 2, 3, 4);
  v1->SetBoundsRect(bounds);
  EXPECT_EQ(bounds, animator->last_bounds());
  // TestLayerAnimator doesn't update the layer.
  EXPECT_NE(bounds, v1->layer()->bounds());
}

// Verifies the bounds of a layer are updated if the bounds of ancestor that
// doesn't have a layer change.
TEST_F(ViewLayerTest, BoundsChangeWithLayer) {
  View* content_view = new View;
  widget()->SetContentsView(content_view);

  View* v1 = new View;
  content_view->AddChildView(v1);
  v1->SetBoundsRect(gfx::Rect(20, 30, 140, 150));

  View* v2 = new View;
  v2->SetBoundsRect(gfx::Rect(10, 11, 40, 50));
  v1->AddChildView(v2);
  v2->SetPaintToLayer(true);
  ASSERT_TRUE(v2->layer() != NULL);
  EXPECT_EQ(gfx::Rect(30, 41, 40, 50), v2->layer()->bounds());

  v1->SetPosition(gfx::Point(25, 36));
  EXPECT_EQ(gfx::Rect(35, 47, 40, 50), v2->layer()->bounds());

  v2->SetPosition(gfx::Point(11, 12));
  EXPECT_EQ(gfx::Rect(36, 48, 40, 50), v2->layer()->bounds());

  // Bounds of the layer should change even if the view is not invisible.
  v1->SetVisible(false);
  v1->SetPosition(gfx::Point(20, 30));
  EXPECT_EQ(gfx::Rect(31, 42, 40, 50), v2->layer()->bounds());

  v2->SetVisible(false);
  v2->SetBoundsRect(gfx::Rect(10, 11, 20, 30));
  EXPECT_EQ(gfx::Rect(30, 41, 20, 30), v2->layer()->bounds());
}

// Make sure layers are positioned correctly in RTL.
TEST_F(ViewLayerTest, BoundInRTL) {
  std::string locale = l10n_util::GetApplicationLocale(std::string());
  base::i18n::SetICUDefaultLocale("he");

  View* view = new View;
  widget()->SetContentsView(view);

  int content_width = view->width();

  // |v1| is initially not attached to anything. So its layer will have the same
  // bounds as the view.
  View* v1 = new View;
  v1->SetPaintToLayer(true);
  v1->SetBounds(10, 10, 20, 10);
  EXPECT_EQ(gfx::Rect(10, 10, 20, 10),
            v1->layer()->bounds());

  // Once |v1| is attached to the widget, its layer will get RTL-appropriate
  // bounds.
  view->AddChildView(v1);
  EXPECT_EQ(gfx::Rect(content_width - 30, 10, 20, 10),
            v1->layer()->bounds());
  gfx::Rect l1bounds = v1->layer()->bounds();

  // Now attach a View to the widget first, then create a layer for it. Make
  // sure the bounds are correct.
  View* v2 = new View;
  v2->SetBounds(50, 10, 30, 10);
  EXPECT_FALSE(v2->layer());
  view->AddChildView(v2);
  v2->SetPaintToLayer(true);
  EXPECT_EQ(gfx::Rect(content_width - 80, 10, 30, 10),
            v2->layer()->bounds());
  gfx::Rect l2bounds = v2->layer()->bounds();

  view->SetPaintToLayer(true);
  EXPECT_EQ(l1bounds, v1->layer()->bounds());
  EXPECT_EQ(l2bounds, v2->layer()->bounds());

  // Move one of the views. Make sure the layer is positioned correctly
  // afterwards.
  v1->SetBounds(v1->x() - 5, v1->y(), v1->width(), v1->height());
  l1bounds.set_x(l1bounds.x() + 5);
  EXPECT_EQ(l1bounds, v1->layer()->bounds());

  view->SetPaintToLayer(false);
  EXPECT_EQ(l1bounds, v1->layer()->bounds());
  EXPECT_EQ(l2bounds, v2->layer()->bounds());

  // Move a view again.
  v2->SetBounds(v2->x() + 5, v2->y(), v2->width(), v2->height());
  l2bounds.set_x(l2bounds.x() - 5);
  EXPECT_EQ(l2bounds, v2->layer()->bounds());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

// Makes sure a transform persists after toggling the visibility.
TEST_F(ViewLayerTest, ToggleVisibilityWithTransform) {
  View* view = new View;
  gfx::Transform transform;
  transform.Scale(2.0, 2.0);
  view->SetTransform(transform);
  widget()->SetContentsView(view);
  EXPECT_EQ(2.0f, view->GetTransform().matrix().get(0, 0));

  view->SetVisible(false);
  EXPECT_EQ(2.0f, view->GetTransform().matrix().get(0, 0));

  view->SetVisible(true);
  EXPECT_EQ(2.0f, view->GetTransform().matrix().get(0, 0));
}

// Verifies a transform persists after removing/adding a view with a transform.
TEST_F(ViewLayerTest, ResetTransformOnLayerAfterAdd) {
  View* view = new View;
  gfx::Transform transform;
  transform.Scale(2.0, 2.0);
  view->SetTransform(transform);
  widget()->SetContentsView(view);
  EXPECT_EQ(2.0f, view->GetTransform().matrix().get(0, 0));
  ASSERT_TRUE(view->layer() != NULL);
  EXPECT_EQ(2.0f, view->layer()->transform().matrix().get(0, 0));

  View* parent = view->parent();
  parent->RemoveChildView(view);
  parent->AddChildView(view);

  EXPECT_EQ(2.0f, view->GetTransform().matrix().get(0, 0));
  ASSERT_TRUE(view->layer() != NULL);
  EXPECT_EQ(2.0f, view->layer()->transform().matrix().get(0, 0));
}

// Makes sure that layer visibility is correct after toggling View visibility.
TEST_F(ViewLayerTest, ToggleVisibilityWithLayer) {
  View* content_view = new View;
  widget()->SetContentsView(content_view);

  // The view isn't attached to a widget or a parent view yet. But it should
  // still have a layer, but the layer should not be attached to the root
  // layer.
  View* v1 = new View;
  v1->SetPaintToLayer(true);
  EXPECT_TRUE(v1->layer());
  EXPECT_FALSE(LayerIsAncestor(widget()->GetCompositor()->root_layer(),
                               v1->layer()));

  // Once the view is attached to a widget, its layer should be attached to the
  // root layer and visible.
  content_view->AddChildView(v1);
  EXPECT_TRUE(LayerIsAncestor(widget()->GetCompositor()->root_layer(),
                              v1->layer()));
  EXPECT_TRUE(v1->layer()->IsDrawn());

  v1->SetVisible(false);
  EXPECT_FALSE(v1->layer()->IsDrawn());

  v1->SetVisible(true);
  EXPECT_TRUE(v1->layer()->IsDrawn());

  widget()->Hide();
  EXPECT_FALSE(v1->layer()->IsDrawn());

  widget()->Show();
  EXPECT_TRUE(v1->layer()->IsDrawn());
}

// Tests that the layers in the subtree are orphaned after a View is removed
// from the parent.
TEST_F(ViewLayerTest, OrphanLayerAfterViewRemove) {
  View* content_view = new View;
  widget()->SetContentsView(content_view);

  View* v1 = new View;
  content_view->AddChildView(v1);

  View* v2 = new View;
  v1->AddChildView(v2);
  v2->SetPaintToLayer(true);
  EXPECT_TRUE(LayerIsAncestor(widget()->GetCompositor()->root_layer(),
                              v2->layer()));
  EXPECT_TRUE(v2->layer()->IsDrawn());

  content_view->RemoveChildView(v1);

  EXPECT_FALSE(LayerIsAncestor(widget()->GetCompositor()->root_layer(),
                               v2->layer()));

  // Reparent |v2|.
  content_view->AddChildView(v2);
  delete v1;
  v1 = NULL;
  EXPECT_TRUE(LayerIsAncestor(widget()->GetCompositor()->root_layer(),
                              v2->layer()));
  EXPECT_TRUE(v2->layer()->IsDrawn());
}

class PaintTrackingView : public View {
 public:
  PaintTrackingView() : painted_(false) {
  }

  bool painted() const { return painted_; }
  void set_painted(bool value) { painted_ = value; }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    painted_ = true;
  }

 private:
  bool painted_;

  DISALLOW_COPY_AND_ASSIGN(PaintTrackingView);
};

// Makes sure child views with layers aren't painted when paint starts at an
// ancestor.
TEST_F(ViewLayerTest, DontPaintChildrenWithLayers) {
  PaintTrackingView* content_view = new PaintTrackingView;
  widget()->SetContentsView(content_view);
  content_view->SetPaintToLayer(true);
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  GetRootLayer()->SchedulePaint(gfx::Rect(0, 0, 10, 10));
  content_view->set_painted(false);
  // content_view no longer has a dirty rect. Paint from the root and make sure
  // PaintTrackingView isn't painted.
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_FALSE(content_view->painted());

  // Make content_view have a dirty rect, paint the layers and make sure
  // PaintTrackingView is painted.
  content_view->layer()->SchedulePaint(gfx::Rect(0, 0, 10, 10));
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_TRUE(content_view->painted());
}

// Tests that the visibility of child layers are updated correctly when a View's
// visibility changes.
TEST_F(ViewLayerTest, VisibilityChildLayers) {
  View* v1 = new View;
  v1->SetPaintToLayer(true);
  widget()->SetContentsView(v1);

  View* v2 = new View;
  v1->AddChildView(v2);

  View* v3 = new View;
  v2->AddChildView(v3);
  v3->SetVisible(false);

  View* v4 = new View;
  v4->SetPaintToLayer(true);
  v3->AddChildView(v4);

  EXPECT_TRUE(v1->layer()->IsDrawn());
  EXPECT_FALSE(v4->layer()->IsDrawn());

  v2->SetVisible(false);
  EXPECT_TRUE(v1->layer()->IsDrawn());
  EXPECT_FALSE(v4->layer()->IsDrawn());

  v2->SetVisible(true);
  EXPECT_TRUE(v1->layer()->IsDrawn());
  EXPECT_FALSE(v4->layer()->IsDrawn());

  v2->SetVisible(false);
  EXPECT_TRUE(v1->layer()->IsDrawn());
  EXPECT_FALSE(v4->layer()->IsDrawn());
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(v1, v1->layer()));

  v3->SetVisible(true);
  EXPECT_TRUE(v1->layer()->IsDrawn());
  EXPECT_FALSE(v4->layer()->IsDrawn());
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(v1, v1->layer()));

  // Reparent |v3| to |v1|.
  v1->AddChildView(v3);
  EXPECT_TRUE(v1->layer()->IsDrawn());
  EXPECT_TRUE(v4->layer()->IsDrawn());
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(v1, v1->layer()));
}

// This test creates a random View tree, and then randomly reorders child views,
// reparents views etc. Unrelated changes can appear to break this test. So
// marking this as FLAKY.
TEST_F(ViewLayerTest, DISABLED_ViewLayerTreesInSync) {
  View* content = new View;
  content->SetPaintToLayer(true);
  widget()->SetContentsView(content);
  widget()->Show();

  ConstructTree(content, 5);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));

  ScrambleTree(content);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));

  ScrambleTree(content);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));

  ScrambleTree(content);
  EXPECT_TRUE(ViewAndLayerTreeAreConsistent(content, content->layer()));
}

// Verifies when views are reordered the layer is also reordered. The widget is
// providing the parent layer.
TEST_F(ViewLayerTest, ReorderUnderWidget) {
  View* content = new View;
  widget()->SetContentsView(content);
  View* c1 = new View;
  c1->SetPaintToLayer(true);
  content->AddChildView(c1);
  View* c2 = new View;
  c2->SetPaintToLayer(true);
  content->AddChildView(c2);

  ui::Layer* parent_layer = c1->layer()->parent();
  ASSERT_TRUE(parent_layer);
  ASSERT_EQ(2u, parent_layer->children().size());
  EXPECT_EQ(c1->layer(), parent_layer->children()[0]);
  EXPECT_EQ(c2->layer(), parent_layer->children()[1]);

  // Move c1 to the front. The layers should have moved too.
  content->ReorderChildView(c1, -1);
  EXPECT_EQ(c1->layer(), parent_layer->children()[1]);
  EXPECT_EQ(c2->layer(), parent_layer->children()[0]);
}

// Verifies that the layer of a view can be acquired properly.
TEST_F(ViewLayerTest, AcquireLayer) {
  View* content = new View;
  widget()->SetContentsView(content);
  scoped_ptr<View> c1(new View);
  c1->SetPaintToLayer(true);
  EXPECT_TRUE(c1->layer());
  content->AddChildView(c1.get());

  scoped_ptr<ui::Layer> layer(c1->AcquireLayer());
  EXPECT_EQ(layer.get(), c1->layer());

  scoped_ptr<ui::Layer> layer2(c1->RecreateLayer());
  EXPECT_NE(c1->layer(), layer2.get());

  // Destroy view before destroying layer.
  c1.reset();
}

// Verify the z-order of the layers as a result of calling RecreateLayer().
TEST_F(ViewLayerTest, RecreateLayerZOrder) {
  scoped_ptr<View> v(new View());
  v->SetPaintToLayer(true);

  View* v1 = new View();
  v1->SetPaintToLayer(true);
  v->AddChildView(v1);
  View* v2 = new View();
  v2->SetPaintToLayer(true);
  v->AddChildView(v2);

  // Test the initial z-order.
  const std::vector<ui::Layer*>& child_layers_pre = v->layer()->children();
  ASSERT_EQ(2u, child_layers_pre.size());
  EXPECT_EQ(v1->layer(), child_layers_pre[0]);
  EXPECT_EQ(v2->layer(), child_layers_pre[1]);

  scoped_ptr<ui::Layer> v1_old_layer(v1->RecreateLayer());

  // Test the new layer order. We expect: |v1| |v1_old_layer| |v2|.
  // for |v1| and |v2|.
  const std::vector<ui::Layer*>& child_layers_post = v->layer()->children();
  ASSERT_EQ(3u, child_layers_post.size());
  EXPECT_EQ(v1->layer(), child_layers_post[0]);
  EXPECT_EQ(v1_old_layer, child_layers_post[1]);
  EXPECT_EQ(v2->layer(), child_layers_post[2]);
}

// Verify the z-order of the layers as a result of calling RecreateLayer when
// the widget is the parent with the layer.
TEST_F(ViewLayerTest, RecreateLayerZOrderWidgetParent) {
  View* v = new View();
  widget()->SetContentsView(v);

  View* v1 = new View();
  v1->SetPaintToLayer(true);
  v->AddChildView(v1);
  View* v2 = new View();
  v2->SetPaintToLayer(true);
  v->AddChildView(v2);

  ui::Layer* root_layer = GetRootLayer();

  // Test the initial z-order.
  const std::vector<ui::Layer*>& child_layers_pre = root_layer->children();
  ASSERT_EQ(2u, child_layers_pre.size());
  EXPECT_EQ(v1->layer(), child_layers_pre[0]);
  EXPECT_EQ(v2->layer(), child_layers_pre[1]);

  scoped_ptr<ui::Layer> v1_old_layer(v1->RecreateLayer());

  // Test the new layer order. We expect: |v1| |v1_old_layer| |v2|.
  const std::vector<ui::Layer*>& child_layers_post = root_layer->children();
  ASSERT_EQ(3u, child_layers_post.size());
  EXPECT_EQ(v1->layer(), child_layers_post[0]);
  EXPECT_EQ(v1_old_layer, child_layers_post[1]);
  EXPECT_EQ(v2->layer(), child_layers_post[2]);
}

// Verifies RecreateLayer() moves all Layers over, even those that don't have
// a View.
TEST_F(ViewLayerTest, RecreateLayerMovesNonViewChildren) {
  View v;
  v.SetPaintToLayer(true);
  View child;
  child.SetPaintToLayer(true);
  v.AddChildView(&child);
  ASSERT_TRUE(v.layer() != NULL);
  ASSERT_EQ(1u, v.layer()->children().size());
  EXPECT_EQ(v.layer()->children()[0], child.layer());

  ui::Layer layer(ui::LAYER_NOT_DRAWN);
  v.layer()->Add(&layer);
  v.layer()->StackAtBottom(&layer);

  scoped_ptr<ui::Layer> old_layer(v.RecreateLayer());

  // All children should be moved from old layer to new layer.
  ASSERT_TRUE(old_layer.get() != NULL);
  EXPECT_TRUE(old_layer->children().empty());

  // And new layer should have the two children.
  ASSERT_TRUE(v.layer() != NULL);
  ASSERT_EQ(2u, v.layer()->children().size());
  EXPECT_EQ(v.layer()->children()[0], &layer);
  EXPECT_EQ(v.layer()->children()[1], child.layer());
}

class BoundsTreeTestView : public View {
 public:
  BoundsTreeTestView() {}

  virtual void PaintChildren(gfx::Canvas* canvas,
                             const CullSet& cull_set) OVERRIDE {
    // Save out a copy of the cull_set before calling the base implementation.
    last_cull_set_.clear();
    if (cull_set.cull_set_) {
      for (base::hash_set<intptr_t>::iterator it = cull_set.cull_set_->begin();
           it != cull_set.cull_set_->end();
           ++it) {
        last_cull_set_.insert(reinterpret_cast<View*>(*it));
      }
    }
    View::PaintChildren(canvas, cull_set);
  }

  std::set<View*> last_cull_set_;
};

TEST_F(ViewLayerTest, BoundsTreePaintUpdatesCullSet) {
  BoundsTreeTestView* test_view = new BoundsTreeTestView;
  widget()->SetContentsView(test_view);

  View* v1 = new View();
  v1->SetBoundsRect(gfx::Rect(10, 15, 150, 151));
  test_view->AddChildView(v1);

  View* v2 = new View();
  v2->SetBoundsRect(gfx::Rect(20, 33, 40, 50));
  v1->AddChildView(v2);

  // Schedule a full-view paint to get everyone's rectangles updated.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Now we have test_view - v1 - v2. Damage to only test_view should only
  // return root_view and test_view.
  test_view->SchedulePaintInRect(gfx::Rect(0, 0, 1, 1));
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_EQ(2U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));

  // Damage to v1 only should only return root_view, test_view, and v1.
  test_view->SchedulePaintInRect(gfx::Rect(11, 16, 1, 1));
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_EQ(3U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v1));

  // A Damage rect inside v2 should get all 3 views back in the |last_cull_set_|
  // on call to TestView::Paint(), along with the widget root view.
  test_view->SchedulePaintInRect(gfx::Rect(31, 49, 1, 1));
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_EQ(4U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v1));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v2));
}

TEST_F(ViewLayerTest, BoundsTreeWithRTL) {
  std::string locale = l10n_util::GetApplicationLocale(std::string());
  base::i18n::SetICUDefaultLocale("ar");

  BoundsTreeTestView* test_view = new BoundsTreeTestView;
  widget()->SetContentsView(test_view);

  // Add child views, which should be in RTL coordinate space of parent view.
  View* v1 = new View;
  v1->SetBoundsRect(gfx::Rect(10, 12, 25, 26));
  test_view->AddChildView(v1);

  View* v2 = new View;
  v2->SetBoundsRect(gfx::Rect(5, 6, 7, 8));
  v1->AddChildView(v2);

  // Schedule a full-view paint to get everyone's rectangles updated.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Damage to the right side of the parent view should touch both child views.
  gfx::Rect rtl_damage(test_view->bounds().width() - 16, 18, 1, 1);
  test_view->SchedulePaintInRect(rtl_damage);
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_EQ(4U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v1));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v2));

  // Damage to the left side of the parent view should only touch the
  // container views.
  gfx::Rect ltr_damage(16, 18, 1, 1);
  test_view->SchedulePaintInRect(ltr_damage);
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_EQ(2U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(ViewLayerTest, BoundsTreeSetBoundsChangesCullSet) {
  BoundsTreeTestView* test_view = new BoundsTreeTestView;
  widget()->SetContentsView(test_view);

  View* v1 = new View;
  v1->SetBoundsRect(gfx::Rect(5, 6, 100, 101));
  test_view->AddChildView(v1);

  View* v2 = new View;
  v2->SetBoundsRect(gfx::Rect(20, 33, 40, 50));
  v1->AddChildView(v2);

  // Schedule a full-view paint to get everyone's rectangles updated.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Move v1 to a new origin out of the way of our next query.
  v1->SetBoundsRect(gfx::Rect(50, 60, 100, 101));
  // The move will force a repaint.
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Schedule a paint with damage rect where v1 used to be.
  test_view->SchedulePaintInRect(gfx::Rect(5, 6, 10, 11));
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Should only have picked up root_view and test_view.
  EXPECT_EQ(2U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
}

TEST_F(ViewLayerTest, BoundsTreeLayerChangeMakesNewTree) {
  BoundsTreeTestView* test_view = new BoundsTreeTestView;
  widget()->SetContentsView(test_view);

  View* v1 = new View;
  v1->SetBoundsRect(gfx::Rect(5, 10, 15, 20));
  test_view->AddChildView(v1);

  View* v2 = new View;
  v2->SetBoundsRect(gfx::Rect(1, 2, 3, 4));
  v1->AddChildView(v2);

  // Schedule a full-view paint to get everyone's rectangles updated.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Set v1 to paint to its own layer, it should remove itself from the
  // test_view heiarchy and no longer intersect with damage rects in that cull
  // set.
  v1->SetPaintToLayer(true);

  // Schedule another full-view paint.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  // v1 and v2 should no longer be present in the test_view cull_set.
  EXPECT_EQ(2U, test_view->last_cull_set_.size());
  EXPECT_EQ(0U, test_view->last_cull_set_.count(v1));
  EXPECT_EQ(0U, test_view->last_cull_set_.count(v2));

  // Now set v1 back to not painting to a layer.
  v1->SetPaintToLayer(false);
  // Schedule another full-view paint.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  // We should be back to the full cull set including v1 and v2.
  EXPECT_EQ(4U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v1));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v2));
}

TEST_F(ViewLayerTest, BoundsTreeRemoveChildRemovesBounds) {
  BoundsTreeTestView* test_view = new BoundsTreeTestView;
  widget()->SetContentsView(test_view);

  View* v1 = new View;
  v1->SetBoundsRect(gfx::Rect(5, 10, 15, 20));
  test_view->AddChildView(v1);

  View* v2 = new View;
  v2->SetBoundsRect(gfx::Rect(1, 2, 3, 4));
  v1->AddChildView(v2);

  // Schedule a full-view paint to get everyone's rectangles updated.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Now remove v1 from the root view.
  test_view->RemoveChildView(v1);

  // Schedule another full-view paint.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  // v1 and v2 should no longer be present in the test_view cull_set.
  EXPECT_EQ(2U, test_view->last_cull_set_.size());
  EXPECT_EQ(0U, test_view->last_cull_set_.count(v1));
  EXPECT_EQ(0U, test_view->last_cull_set_.count(v2));

  // View v1 and v2 are no longer part of view hierarchy and therefore won't be
  // deleted with that hierarchy.
  delete v1;
}

TEST_F(ViewLayerTest, BoundsTreeMoveViewMovesBounds) {
  BoundsTreeTestView* test_view = new BoundsTreeTestView;
  widget()->SetContentsView(test_view);

  // Build hierarchy v1 - v2 - v3.
  View* v1 = new View;
  v1->SetBoundsRect(gfx::Rect(20, 30, 150, 160));
  test_view->AddChildView(v1);

  View* v2 = new View;
  v2->SetBoundsRect(gfx::Rect(5, 10, 40, 50));
  v1->AddChildView(v2);

  View* v3 = new View;
  v3->SetBoundsRect(gfx::Rect(1, 2, 3, 4));
  v2->AddChildView(v3);

  // Schedule a full-view paint and ensure all views are present in the cull.
  test_view->SchedulePaintInRect(test_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());
  EXPECT_EQ(5U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v1));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v2));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v3));

  // Build an unrelated view hierarchy and move v2 in to it.
  scoped_ptr<Widget> test_widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(10, 10, 500, 500);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  test_widget->Init(params);
  test_widget->Show();
  BoundsTreeTestView* widget_view = new BoundsTreeTestView;
  test_widget->SetContentsView(widget_view);
  widget_view->AddChildView(v2);

  // Now schedule full-view paints in both widgets.
  test_view->SchedulePaintInRect(test_view->bounds());
  widget_view->SchedulePaintInRect(widget_view->bounds());
  GetRootLayer()->GetCompositor()->ScheduleDraw();
  ui::DrawWaiterForTest::Wait(GetRootLayer()->GetCompositor());

  // Only v1 should be present in the first cull set.
  EXPECT_EQ(3U, test_view->last_cull_set_.size());
  EXPECT_EQ(1U, test_view->last_cull_set_.count(widget()->GetRootView()));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(test_view));
  EXPECT_EQ(1U, test_view->last_cull_set_.count(v1));

  // We should find v2 and v3 in the widget_view cull_set.
  EXPECT_EQ(4U, widget_view->last_cull_set_.size());
  EXPECT_EQ(1U, widget_view->last_cull_set_.count(test_widget->GetRootView()));
  EXPECT_EQ(1U, widget_view->last_cull_set_.count(widget_view));
  EXPECT_EQ(1U, widget_view->last_cull_set_.count(v2));
  EXPECT_EQ(1U, widget_view->last_cull_set_.count(v3));
}

TEST_F(ViewTest, FocusableAssertions) {
  // View subclasses may change insets based on whether they are focusable,
  // which effects the preferred size. To avoid preferred size changing around
  // these Views need to key off the last value set to SetFocusable(), not
  // whether the View is focusable right now. For this reason it's important
  // that focusable() return the last value passed to SetFocusable and not
  // whether the View is focusable right now.
  TestView view;
  view.SetFocusable(true);
  EXPECT_TRUE(view.focusable());
  view.SetEnabled(false);
  EXPECT_TRUE(view.focusable());
  view.SetFocusable(false);
  EXPECT_FALSE(view.focusable());
}

// Verifies when a view is deleted it is removed from ViewStorage.
TEST_F(ViewTest, UpdateViewStorageOnDelete) {
  ViewStorage* view_storage = ViewStorage::GetInstance();
  const int storage_id = view_storage->CreateStorageID();
  {
    View view;
    view_storage->StoreView(storage_id, &view);
  }
  EXPECT_TRUE(view_storage->RetrieveView(storage_id) == NULL);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTheme
////////////////////////////////////////////////////////////////////////////////

void TestView::OnNativeThemeChanged(const ui::NativeTheme* native_theme) {
  native_theme_ = native_theme;
}

TEST_F(ViewTest, OnNativeThemeChanged) {
  TestView* test_view = new TestView();
  EXPECT_FALSE(test_view->native_theme_);
  TestView* test_view_child = new TestView();
  EXPECT_FALSE(test_view_child->native_theme_);

  // Child view added before the widget hierarchy exists should get the
  // new native theme notification.
  test_view->AddChildView(test_view_child);

  scoped_ptr<Widget> widget(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(params);

  widget->GetRootView()->AddChildView(test_view);
  EXPECT_TRUE(test_view->native_theme_);
  EXPECT_EQ(widget->GetNativeTheme(), test_view->native_theme_);
  EXPECT_TRUE(test_view_child->native_theme_);
  EXPECT_EQ(widget->GetNativeTheme(), test_view_child->native_theme_);

  // Child view added after the widget hierarchy exists should also get the
  // notification.
  TestView* test_view_child_2 = new TestView();
  test_view->AddChildView(test_view_child_2);
  EXPECT_TRUE(test_view_child_2->native_theme_);
  EXPECT_EQ(widget->GetNativeTheme(), test_view_child_2->native_theme_);

  widget->CloseNow();
}

}  // namespace views
