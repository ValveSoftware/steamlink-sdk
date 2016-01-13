// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_targeter.h"

#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/test/test_event_handler.h"

namespace aura {

// Always returns the same window.
class StaticWindowTargeter : public ui::EventTargeter {
 public:
  explicit StaticWindowTargeter(aura::Window* window)
      : window_(window) {}
  virtual ~StaticWindowTargeter() {}

 private:
  // ui::EventTargeter:
  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) OVERRIDE {
    return window_;
  }

  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(StaticWindowTargeter);
};

class WindowTargeterTest : public test::AuraTestBase {
 public:
  WindowTargeterTest() {}
  virtual ~WindowTargeterTest() {}

  Window* root_window() { return AuraTestBase::root_window(); }
};

gfx::RectF GetEffectiveVisibleBoundsInRootWindow(Window* window) {
  gfx::RectF bounds = gfx::Rect(window->bounds().size());
  Window* root = window->GetRootWindow();
  CHECK(window->layer());
  CHECK(root->layer());
  gfx::Transform transform;
  if (!window->layer()->GetTargetTransformRelativeTo(root->layer(), &transform))
    return gfx::RectF();
  transform.TransformRect(&bounds);
  return bounds;
}

TEST_F(WindowTargeterTest, Basic) {
  test::TestWindowDelegate delegate;
  scoped_ptr<Window> window(CreateNormalWindow(1, root_window(), &delegate));
  Window* one = CreateNormalWindow(2, window.get(), &delegate);
  Window* two = CreateNormalWindow(3, window.get(), &delegate);

  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  one->SetBounds(gfx::Rect(0, 0, 500, 100));
  two->SetBounds(gfx::Rect(501, 0, 500, 1000));

  root_window()->Show();

  ui::test::TestEventHandler handler;
  one->AddPreTargetHandler(&handler);

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED,
                       gfx::Point(20, 20),
                       gfx::Point(20, 20),
                       ui::EF_NONE,
                       ui::EF_NONE);
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(1, handler.num_mouse_events());

  handler.Reset();
  DispatchEventUsingWindowDispatcher(&press);
  EXPECT_EQ(1, handler.num_mouse_events());

  one->RemovePreTargetHandler(&handler);
}

TEST_F(WindowTargeterTest, ScopedWindowTargeter) {
  test::TestWindowDelegate delegate;
  scoped_ptr<Window> window(CreateNormalWindow(1, root_window(), &delegate));
  Window* child = CreateNormalWindow(2, window.get(), &delegate);

  window->SetBounds(gfx::Rect(30, 30, 100, 100));
  child->SetBounds(gfx::Rect(20, 20, 50, 50));
  root_window()->Show();

  ui::EventTarget* root = root_window();
  ui::EventTargeter* targeter = root->GetEventTargeter();

  gfx::Point event_location(60, 60);
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(child, targeter->FindTargetForEvent(root, &mouse));
  }

  // Install a targeter on |window| so that the events never reach the child.
  scoped_ptr<ScopedWindowTargeter> scoped_targeter(
      new ScopedWindowTargeter(window.get(), scoped_ptr<ui::EventTargeter>(
          new StaticWindowTargeter(window.get()))));
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root, &mouse));
  }
  scoped_targeter.reset();
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(child, targeter->FindTargetForEvent(root, &mouse));
  }
}

TEST_F(WindowTargeterTest, TargetTransformedWindow) {
  root_window()->Show();

  test::TestWindowDelegate delegate;
  scoped_ptr<Window> window(CreateNormalWindow(2, root_window(), &delegate));

  const gfx::Rect window_bounds(100, 20, 400, 80);
  window->SetBounds(window_bounds);

  ui::EventTarget* root_target = root_window();
  ui::EventTargeter* targeter = root_target->GetEventTargeter();
  gfx::Point event_location(490, 50);
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &mouse));
  }

  // Scale |window| by 50%. This should move it away from underneath
  // |event_location|, so an event in that location will not be targeted to it.
  gfx::Transform transform;
  transform.Scale(0.5, 0.5);
  window->SetTransform(transform);
  EXPECT_EQ(gfx::RectF(100, 20, 200, 40).ToString(),
            GetEffectiveVisibleBoundsInRootWindow(window.get()).ToString());
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(root_window(), targeter->FindTargetForEvent(root_target, &mouse));
  }

  transform = gfx::Transform();
  transform.Translate(200, 10);
  transform.Scale(0.5, 0.5);
  window->SetTransform(transform);
  EXPECT_EQ(gfx::RectF(300, 30, 200, 40).ToString(),
            GetEffectiveVisibleBoundsInRootWindow(window.get()).ToString());
  {
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, event_location, event_location,
                         ui::EF_NONE, ui::EF_NONE);
    EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &mouse));
  }
}

}  // namespace aura
