// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_client.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/common/common_type_converters.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/capture_client_observer.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_client_observer.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {

namespace {

DEFINE_WINDOW_PROPERTY_KEY(uint8_t, kTestPropertyKey1, 0);
DEFINE_WINDOW_PROPERTY_KEY(uint16_t, kTestPropertyKey2, 0);
DEFINE_WINDOW_PROPERTY_KEY(bool, kTestPropertyKey3, false);

const char kTestPropertyServerKey1[] = "test-property-server1";
const char kTestPropertyServerKey2[] = "test-property-server2";
const char kTestPropertyServerKey3[] = "test-property-server3";

Id server_id(Window* window) {
  return WindowMus::Get(window)->server_id();
}

void SetWindowVisibility(Window* window, bool visible) {
  if (visible)
    window->Show();
  else
    window->Hide();
}

Window* GetFirstRoot(WindowTreeClient* client) {
  return client->GetRoots().empty() ? nullptr : *client->GetRoots().begin();
}

bool IsWindowHostVisible(Window* window) {
  return window->GetRootWindow()->GetHost()->compositor()->IsVisible();
}

// Register some test window properties for aura/mus conversion.
void RegisterTestProperties(PropertyConverter* converter) {
  converter->RegisterProperty(kTestPropertyKey1, kTestPropertyServerKey1);
  converter->RegisterProperty(kTestPropertyKey2, kTestPropertyServerKey2);
  converter->RegisterProperty(kTestPropertyKey3, kTestPropertyServerKey3);
}

// Convert a primitive aura property value to a mus transport value.
// Note that this implicitly casts arguments to the aura storage type, int64_t.
mojo::Array<uint8_t> ConvertToPropertyTransportValue(int64_t value) {
  return mojo::Array<uint8_t>(mojo::ConvertTo<std::vector<uint8_t>>(value));
}

}  // namespace

using WindowTreeClientWmTest = test::AuraMusWmTestBase;
using WindowTreeClientClientTest = test::AuraMusClientTestBase;

// Verifies bounds are reverted if the server replied that the change failed.
TEST_F(WindowTreeClientWmTest, SetBoundsFailed) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  const gfx::Rect original_bounds(window.bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, window.bounds());
  window.SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, window.bounds());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(original_bounds, window.bounds());
}

// Verifies a new window from the server doesn't result in attempting to add
// the window back to the server.
TEST_F(WindowTreeClientWmTest, AddFromServerDoesntAddAgain) {
  const Id child_window_id = server_id(root_window()) + 11;
  ui::mojom::WindowDataPtr data = ui::mojom::WindowData::New();
  data->parent_id = server_id(root_window());
  data->window_id = child_window_id;
  data->bounds = gfx::Rect(1, 2, 3, 4);
  data->visible = false;
  mojo::Array<ui::mojom::WindowDataPtr> data_array(1);
  data_array[0] = std::move(data);
  ASSERT_TRUE(root_window()->children().empty());
  window_tree_client()->OnWindowHierarchyChanged(
      child_window_id, 0, server_id(root_window()), std::move(data_array));
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(1u, root_window()->children().size());
  Window* child = root_window()->children()[0];
  EXPECT_FALSE(child->TargetVisibility());
}

// Verifies a reparent from the server doesn't attempt signal the server.
TEST_F(WindowTreeClientWmTest, ReparentFromServerDoesntAddAgain) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window1);
  root_window()->AddChild(&window2);

  window_tree()->AckAllChanges();
  // Simulate moving |window1| to be a child of |window2| from the server.
  window_tree_client()->OnWindowHierarchyChanged(server_id(&window1),
                                                 server_id(root_window()),
                                                 server_id(&window2), nullptr);
  ASSERT_FALSE(window_tree()->has_change());
  EXPECT_EQ(&window2, window1.parent());
  EXPECT_EQ(root_window(), window2.parent());
  window1.parent()->RemoveChild(&window1);
}

// Verifies properties passed in OnWindowHierarchyChanged() make there way to
// the new window.
TEST_F(WindowTreeClientWmTest, OnWindowHierarchyChangedWithProperties) {
  RegisterTestProperties(GetPropertyConverter());
  window_tree()->AckAllChanges();
  const Id child_window_id = server_id(root_window()) + 11;
  ui::mojom::WindowDataPtr data = ui::mojom::WindowData::New();
  const uint8_t server_test_property1_value = 91;
  data->properties[kTestPropertyServerKey1] =
      ConvertToPropertyTransportValue(server_test_property1_value);
  data->parent_id = server_id(root_window());
  data->window_id = child_window_id;
  data->bounds = gfx::Rect(1, 2, 3, 4);
  data->visible = false;
  mojo::Array<ui::mojom::WindowDataPtr> data_array(1);
  data_array[0] = std::move(data);
  ASSERT_TRUE(root_window()->children().empty());
  window_tree_client()->OnWindowHierarchyChanged(
      child_window_id, 0, server_id(root_window()), std::move(data_array));
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(1u, root_window()->children().size());
  Window* child = root_window()->children()[0];
  EXPECT_FALSE(child->TargetVisibility());
  EXPECT_EQ(server_test_property1_value, child->GetProperty(kTestPropertyKey1));
}

// Verifies a move from the server doesn't attempt signal the server.
TEST_F(WindowTreeClientWmTest, MoveFromServerDoesntAddAgain) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window1);
  root_window()->AddChild(&window2);

  window_tree()->AckAllChanges();
  // Simulate moving |window1| to be a child of |window2| from the server.
  window_tree_client()->OnWindowReordered(server_id(&window2),
                                          server_id(&window1),
                                          ui::mojom::OrderDirection::BELOW);
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(2u, root_window()->children().size());
  EXPECT_EQ(&window2, root_window()->children()[0]);
  EXPECT_EQ(&window1, root_window()->children()[1]);
}

TEST_F(WindowTreeClientWmTest, FocusFromServer) {
  Window window1(nullptr);
  window1.Init(ui::LAYER_NOT_DRAWN);
  Window window2(nullptr);
  window2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&window1);
  root_window()->AddChild(&window2);

  ASSERT_TRUE(window1.CanFocus());
  window_tree()->AckAllChanges();
  EXPECT_FALSE(window1.HasFocus());
  // Simulate moving |window1| to be a child of |window2| from the server.
  window_tree_client()->OnWindowFocused(server_id(&window1));
  ASSERT_FALSE(window_tree()->has_change());
  EXPECT_TRUE(window1.HasFocus());
}

// Simulates a bounds change, and while the bounds change is in flight the
// server replies with a new bounds and the original bounds change fails.
TEST_F(WindowTreeClientWmTest, SetBoundsFailedWithPendingChange) {
  const gfx::Rect original_bounds(root_window()->bounds());
  const gfx::Rect new_bounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_NE(new_bounds, root_window()->bounds());
  root_window()->SetBounds(new_bounds);
  EXPECT_EQ(new_bounds, root_window()->bounds());

  // Simulate the server responding with a bounds change.
  const gfx::Rect server_changed_bounds(gfx::Rect(0, 0, 101, 102));
  window_tree_client()->OnWindowBoundsChanged(
      server_id(root_window()), original_bounds, server_changed_bounds);

  // This shouldn't trigger the bounds changing yet.
  EXPECT_EQ(new_bounds, root_window()->bounds());

  // Tell the client the change failed, which should trigger failing to the
  // most recent bounds from server.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(server_changed_bounds, root_window()->bounds());

  // Simulate server changing back to original bounds. Should take immediately.
  window_tree_client()->OnWindowBoundsChanged(
      server_id(root_window()), server_changed_bounds, original_bounds);
  EXPECT_EQ(original_bounds, root_window()->bounds());
}

TEST_F(WindowTreeClientWmTest, TwoInFlightBoundsChangesBothCanceled) {
  const gfx::Rect original_bounds(root_window()->bounds());
  const gfx::Rect bounds1(gfx::Rect(0, 0, 100, 100));
  const gfx::Rect bounds2(gfx::Rect(0, 0, 100, 102));
  root_window()->SetBounds(bounds1);
  EXPECT_EQ(bounds1, root_window()->bounds());

  root_window()->SetBounds(bounds2);
  EXPECT_EQ(bounds2, root_window()->bounds());

  // Tell the client the first bounds failed. As there is a still a change in
  // flight nothing should happen.
  ASSERT_TRUE(
      window_tree()->AckFirstChangeOfType(WindowTreeChangeType::BOUNDS, false));
  EXPECT_EQ(bounds2, root_window()->bounds());

  // Tell the client the seconds bounds failed. Should now fallback to original
  // value.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(WindowTreeChangeType::BOUNDS,
                                                   false));
  EXPECT_EQ(original_bounds, root_window()->bounds());
}

// Verifies properties are set if the server replied that the change succeeded.
TEST_F(WindowTreeClientWmTest, SetPropertySucceeded) {
  ASSERT_FALSE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  root_window()->SetProperty(client::kAlwaysOnTopKey, true);
  EXPECT_TRUE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  mojo::Array<uint8_t> value = window_tree()->GetLastPropertyValue();
  ASSERT_FALSE(value.is_null());
  // PropertyConverter uses int64_t values, even for smaller types, like bool.
  ASSERT_EQ(8u, value.size());
  EXPECT_EQ(1, mojo::ConvertTo<int64_t>(value.PassStorage()));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, true));
  EXPECT_TRUE(root_window()->GetProperty(client::kAlwaysOnTopKey));
}

// Verifies properties are reverted if the server replied that the change
// failed.
TEST_F(WindowTreeClientWmTest, SetPropertyFailed) {
  ASSERT_FALSE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  root_window()->SetProperty(client::kAlwaysOnTopKey, true);
  EXPECT_TRUE(root_window()->GetProperty(client::kAlwaysOnTopKey));
  mojo::Array<uint8_t> value = window_tree()->GetLastPropertyValue();
  ASSERT_FALSE(value.is_null());
  // PropertyConverter uses int64_t values, even for smaller types, like bool.
  ASSERT_EQ(8u, value.size());
  EXPECT_EQ(1, mojo::ConvertTo<int64_t>(value.PassStorage()));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_FALSE(root_window()->GetProperty(client::kAlwaysOnTopKey));
}

// Simulates a property change, and while the property change is in flight the
// server replies with a new property and the original property change fails.
TEST_F(WindowTreeClientWmTest, SetPropertyFailedWithPendingChange) {
  RegisterTestProperties(GetPropertyConverter());
  const uint8_t value1 = 11;
  root_window()->SetProperty(kTestPropertyKey1, value1);
  EXPECT_EQ(value1, root_window()->GetProperty(kTestPropertyKey1));

  // Simulate the server responding with a different value.
  const uint8_t server_value = 12;
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey1,
      ConvertToPropertyTransportValue(server_value));

  // This shouldn't trigger the property changing yet.
  EXPECT_EQ(value1, root_window()->GetProperty(kTestPropertyKey1));

  // Tell the client the change failed, which should trigger failing to the
  // most recent value from server.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(server_value, root_window()->GetProperty(kTestPropertyKey1));

  // Simulate server changing back to value1. Should take immediately.
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey1,
      ConvertToPropertyTransportValue(value1));
  EXPECT_EQ(value1, root_window()->GetProperty(kTestPropertyKey1));
}

// Verifies property setting behavior with failures for primitive properties.
TEST_F(WindowTreeClientWmTest, SetPrimitiveProperties) {
  PropertyConverter* property_converter = GetPropertyConverter();
  RegisterTestProperties(property_converter);

  const uint8_t value1_local = UINT8_MAX / 2;
  const uint8_t value1_server = UINT8_MAX / 3;
  root_window()->SetProperty(kTestPropertyKey1, value1_local);
  EXPECT_EQ(value1_local, root_window()->GetProperty(kTestPropertyKey1));
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey1,
      ConvertToPropertyTransportValue(value1_server));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(value1_server, root_window()->GetProperty(kTestPropertyKey1));

  const uint16_t value2_local = UINT16_MAX / 3;
  const uint16_t value2_server = UINT16_MAX / 4;
  root_window()->SetProperty(kTestPropertyKey2, value2_local);
  EXPECT_EQ(value2_local, root_window()->GetProperty(kTestPropertyKey2));
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey2,
      ConvertToPropertyTransportValue(value2_server));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(value2_server, root_window()->GetProperty(kTestPropertyKey2));

  EXPECT_FALSE(root_window()->GetProperty(kTestPropertyKey3));
  root_window()->SetProperty(kTestPropertyKey3, true);
  EXPECT_TRUE(root_window()->GetProperty(kTestPropertyKey3));
  window_tree_client()->OnWindowSharedPropertyChanged(
      server_id(root_window()), kTestPropertyServerKey3,
      ConvertToPropertyTransportValue(false));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_FALSE(root_window()->GetProperty(kTestPropertyKey3));
}

// Verifies property setting behavior for a gfx::Rect* property.
TEST_F(WindowTreeClientWmTest, SetRectProperty) {
  gfx::Rect example(1, 2, 3, 4);
  ASSERT_EQ(nullptr, root_window()->GetProperty(client::kRestoreBoundsKey));
  root_window()->SetProperty(client::kRestoreBoundsKey, new gfx::Rect(example));
  EXPECT_TRUE(root_window()->GetProperty(client::kRestoreBoundsKey));
  mojo::Array<uint8_t> value = window_tree()->GetLastPropertyValue();
  ASSERT_FALSE(value.is_null());
  EXPECT_EQ(example, mojo::ConvertTo<gfx::Rect>(value.PassStorage()));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, true));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kRestoreBoundsKey));

  root_window()->SetProperty(client::kRestoreBoundsKey, new gfx::Rect());
  EXPECT_EQ(gfx::Rect(),
            *root_window()->GetProperty(client::kRestoreBoundsKey));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kRestoreBoundsKey));
}

// Verifies property setting behavior for a std::string* property.
TEST_F(WindowTreeClientWmTest, SetStringProperty) {
  std::string example = "123";
  ASSERT_EQ(nullptr, root_window()->GetProperty(client::kAppIdKey));
  root_window()->SetProperty(client::kAppIdKey, new std::string(example));
  EXPECT_TRUE(root_window()->GetProperty(client::kAppIdKey));
  mojo::Array<uint8_t> value = window_tree()->GetLastPropertyValue();
  ASSERT_FALSE(value.is_null());
  EXPECT_EQ(example, mojo::ConvertTo<std::string>(value.PassStorage()));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, true));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kAppIdKey));

  root_window()->SetProperty(client::kAppIdKey, new std::string());
  EXPECT_EQ(std::string(), *root_window()->GetProperty(client::kAppIdKey));
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(example, *root_window()->GetProperty(client::kAppIdKey));
}

// Verifies visible is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientWmTest, SetVisibleFailed) {
  const bool original_visible = root_window()->TargetVisibility();
  const bool new_visible = !original_visible;
  SetWindowVisibility(root_window(), new_visible);
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, false));
  EXPECT_EQ(original_visible, root_window()->TargetVisibility());
}

// Simulates a visible change, and while the visible change is in flight the
// server replies with a new visible and the original visible change fails.
TEST_F(WindowTreeClientWmTest, SetVisibleFailedWithPendingChange) {
  const bool original_visible = root_window()->TargetVisibility();
  const bool new_visible = !original_visible;
  SetWindowVisibility(root_window(), new_visible);
  EXPECT_EQ(new_visible, root_window()->TargetVisibility());

  // Simulate the server responding with a visible change.
  const bool server_changed_visible = !new_visible;
  window_tree_client()->OnWindowVisibilityChanged(server_id(root_window()),
                                                  server_changed_visible);

  // This shouldn't trigger visible changing yet.
  EXPECT_EQ(new_visible, root_window()->TargetVisibility());

  // Tell the client the change failed, which should trigger failing to the
  // most recent visible from server.
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, false));
  EXPECT_EQ(server_changed_visible, root_window()->TargetVisibility());

  // Simulate server changing back to original visible. Should take immediately.
  window_tree_client()->OnWindowVisibilityChanged(server_id(root_window()),
                                                  original_visible);
  EXPECT_EQ(original_visible, root_window()->TargetVisibility());
}

/*
// Verifies |is_modal| is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientWmTest, SetModalFailed) {
  WindowTreeSetup setup;
  Window* root = GetFirstRoot();
  ASSERT_TRUE(root);
  EXPECT_FALSE(root->is_modal());
  root->SetModal();
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndClearChangeId(&change_id));
  EXPECT_TRUE(root->is_modal());
  window_tree_client()->OnChangeCompleted(change_id, false);
  EXPECT_FALSE(root->is_modal());
}
*/

namespace {

class InputEventBasicTestWindowDelegate : public test::TestWindowDelegate {
 public:
  static uint32_t constexpr kEventId = 1;

  explicit InputEventBasicTestWindowDelegate(TestWindowTree* test_window_tree)
      : test_window_tree_(test_window_tree) {}
  ~InputEventBasicTestWindowDelegate() override {}

  bool got_move() const { return got_move_; }
  bool was_acked() const { return was_acked_; }

  // TestWindowDelegate::
  void OnMouseEvent(ui::MouseEvent* event) override {
    was_acked_ = test_window_tree_->WasEventAcked(kEventId);
    if (event->type() == ui::ET_MOUSE_MOVED)
      got_move_ = true;
  }

 private:
  TestWindowTree* test_window_tree_;
  bool was_acked_ = false;
  bool got_move_ = false;

  DISALLOW_COPY_AND_ASSIGN(InputEventBasicTestWindowDelegate);
};

}  // namespace

TEST_F(WindowTreeClientClientTest, InputEventBasic) {
  InputEventBasicTestWindowDelegate window_delegate(window_tree());
  WindowTreeHostMus window_tree_host(window_tree_client_impl());
  Window* top_level = window_tree_host.window();
  const gfx::Rect bounds(0, 0, 100, 100);
  window_tree_host.SetBounds(bounds);
  window_tree_host.Show();
  EXPECT_EQ(bounds, top_level->bounds());
  EXPECT_EQ(bounds, window_tree_host.GetBounds());
  Window child(&window_delegate);
  child.Init(ui::LAYER_NOT_DRAWN);
  top_level->AddChild(&child);
  child.SetBounds(gfx::Rect(0, 0, 100, 100));
  child.Show();
  EXPECT_FALSE(window_delegate.got_move());
  EXPECT_FALSE(window_delegate.was_acked());
  std::unique_ptr<ui::Event> ui_event(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                         ui::EventTimeForNow(), ui::EF_NONE, 0));
  window_tree_client()->OnWindowInputEvent(
      InputEventBasicTestWindowDelegate::kEventId, server_id(top_level),
      ui::Event::Clone(*ui_event.get()), 0);
  EXPECT_TRUE(window_tree()->WasEventAcked(1));
  EXPECT_TRUE(window_delegate.got_move());
  EXPECT_FALSE(window_delegate.was_acked());
}

class WindowTreeClientPointerObserverTest : public WindowTreeClientClientTest {
 public:
  WindowTreeClientPointerObserverTest() {}
  ~WindowTreeClientPointerObserverTest() override {}

  void DeleteLastEventObserved() { last_event_observed_.reset(); }
  const ui::PointerEvent* last_event_observed() const {
    return last_event_observed_.get();
  }

  // WindowTreeClientClientTest:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              Window* target) override {
    last_event_observed_.reset(new ui::PointerEvent(event));
  }

 private:
  std::unique_ptr<ui::PointerEvent> last_event_observed_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientPointerObserverTest);
};

// Tests pointer watchers triggered by events that did not hit a target in this
// window tree.
TEST_F(WindowTreeClientPointerObserverTest, OnPointerEventObserved) {
  std::unique_ptr<Window> top_level(base::MakeUnique<Window>(nullptr));
  top_level->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  top_level->Init(ui::LAYER_NOT_DRAWN);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  // Start a pointer watcher for all events excluding move events.
  window_tree_client_impl()->StartPointerWatcher(false /* want_moves */);

  // Simulate the server sending an observed event.
  std::unique_ptr<ui::PointerEvent> pointer_event_down(new ui::PointerEvent(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_CONTROL_DOWN, 1,
      0, ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks()));
  window_tree_client()->OnPointerEventObserved(std::move(pointer_event_down),
                                               0u);

  // Delegate sensed the event.
  const ui::PointerEvent* last_event = last_event_observed();
  ASSERT_TRUE(last_event);
  EXPECT_EQ(ui::ET_POINTER_DOWN, last_event->type());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, last_event->flags());
  DeleteLastEventObserved();

  // Stop the pointer watcher.
  window_tree_client_impl()->StopPointerWatcher();

  // Simulate another event from the server.
  std::unique_ptr<ui::PointerEvent> pointer_event_up(new ui::PointerEvent(
      ui::ET_POINTER_UP, gfx::Point(), gfx::Point(), ui::EF_CONTROL_DOWN, 1, 0,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks()));
  window_tree_client()->OnPointerEventObserved(std::move(pointer_event_up), 0u);

  // No event was sensed.
  EXPECT_FALSE(last_event_observed());
}

// Tests pointer watchers triggered by events that hit this window tree.
TEST_F(WindowTreeClientPointerObserverTest,
       OnWindowInputEventWithPointerWatcher) {
  std::unique_ptr<Window> top_level(base::MakeUnique<Window>(nullptr));
  top_level->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  top_level->Init(ui::LAYER_NOT_DRAWN);
  top_level->SetBounds(gfx::Rect(0, 0, 100, 100));
  top_level->Show();

  // Start a pointer watcher for all events excluding move events.
  window_tree_client_impl()->StartPointerWatcher(false /* want_moves */);

  // Simulate the server dispatching an event that also matched the observer.
  std::unique_ptr<ui::PointerEvent> pointer_event_down(new ui::PointerEvent(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_CONTROL_DOWN, 1,
      0, ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks::Now()));
  window_tree_client()->OnWindowInputEvent(1, server_id(top_level.get()),
                                           std::move(pointer_event_down), true);

  // Delegate sensed the event.
  const ui::Event* last_event = last_event_observed();
  ASSERT_TRUE(last_event);
  EXPECT_EQ(ui::ET_POINTER_DOWN, last_event->type());
  EXPECT_EQ(ui::EF_CONTROL_DOWN, last_event->flags());
}

// Verifies focus is reverted if the server replied that the change failed.
TEST_F(WindowTreeClientWmTest, SetFocusFailed) {
  Window child(nullptr);
  child.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child);
  child.Focus();
  ASSERT_TRUE(child.HasFocus());
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::FOCUS, false));
  EXPECT_EQ(nullptr, client::GetFocusClient(&child)->GetFocusedWindow());
}

// Simulates a focus change, and while the focus change is in flight the server
// replies with a new focus and the original focus change fails.
TEST_F(WindowTreeClientWmTest, SetFocusFailedWithPendingChange) {
  Window child1(nullptr);
  child1.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child1);
  Window child2(nullptr);
  child2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child2);
  Window* original_focus = client::GetFocusClient(&child1)->GetFocusedWindow();
  Window* new_focus = &child1;
  ASSERT_NE(new_focus, original_focus);
  new_focus->Focus();
  ASSERT_TRUE(new_focus->HasFocus());

  // Simulate the server responding with a focus change.
  window_tree_client()->OnWindowFocused(server_id(&child2));

  // This shouldn't trigger focus changing yet.
  EXPECT_TRUE(child1.HasFocus());

  // Tell the client the change failed, which should trigger failing to the
  // most recent focus from server.
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::FOCUS, false));
  EXPECT_FALSE(child1.HasFocus());
  EXPECT_TRUE(child2.HasFocus());
  EXPECT_EQ(&child2, client::GetFocusClient(&child1)->GetFocusedWindow());

  // Simulate server changing focus to child1. Should take immediately.
  window_tree_client()->OnWindowFocused(server_id(&child1));
  EXPECT_TRUE(child1.HasFocus());
}

TEST_F(WindowTreeClientWmTest, FocusOnRemovedWindowWithInFlightFocusChange) {
  std::unique_ptr<Window> child1(base::MakeUnique<Window>(nullptr));
  child1->Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(child1.get());
  Window child2(nullptr);
  child2.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child2);

  child1->Focus();

  // Destroy child1, which should set focus to null.
  child1.reset(nullptr);
  EXPECT_EQ(nullptr, client::GetFocusClient(root_window())->GetFocusedWindow());

  // Server changes focus to 2.
  window_tree_client()->OnWindowFocused(server_id(&child2));
  // Shouldn't take immediately.
  EXPECT_FALSE(child2.HasFocus());

  // Ack both changes, focus should still be null.
  ASSERT_TRUE(
      window_tree()->AckFirstChangeOfType(WindowTreeChangeType::FOCUS, true));
  EXPECT_EQ(nullptr, client::GetFocusClient(root_window())->GetFocusedWindow());
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::FOCUS, true));
  EXPECT_EQ(nullptr, client::GetFocusClient(root_window())->GetFocusedWindow());

  // Change to 2 again, this time it should take.
  window_tree_client()->OnWindowFocused(server_id(&child2));
  EXPECT_TRUE(child2.HasFocus());
}

class ToggleVisibilityFromDestroyedObserver : public WindowObserver {
 public:
  explicit ToggleVisibilityFromDestroyedObserver(Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }

  ToggleVisibilityFromDestroyedObserver() { EXPECT_FALSE(window_); }

  // WindowObserver:
  void OnWindowDestroyed(Window* window) override {
    SetWindowVisibility(window, !window->TargetVisibility());
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ToggleVisibilityFromDestroyedObserver);
};

TEST_F(WindowTreeClientWmTest, ToggleVisibilityFromWindowDestroyed) {
  std::unique_ptr<Window> child(base::MakeUnique<Window>(nullptr));
  child->Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(child.get());
  ToggleVisibilityFromDestroyedObserver toggler(child.get());
  // Destroying the window triggers
  // ToggleVisibilityFromDestroyedObserver::OnWindowDestroyed(), which toggles
  // the visibility of the window. Ack the change, which should not crash or
  // trigger DCHECKs.
  child.reset();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, true));
}

TEST_F(WindowTreeClientClientTest, NewTopLevelWindow) {
  const size_t initial_root_count =
      window_tree_client_impl()->GetRoots().size();
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      base::MakeUnique<WindowTreeHostMus>(window_tree_client_impl());
  aura::Window* top_level = window_tree_host->window();
  // TODO: need to check WindowTreeHost visibility.
  // EXPECT_TRUE(WindowPrivate(root2).parent_drawn());
  EXPECT_NE(server_id(top_level), server_id(root_window()));
  EXPECT_EQ(initial_root_count + 1,
            window_tree_client_impl()->GetRoots().size());
  EXPECT_TRUE(window_tree_client_impl()->GetRoots().count(top_level) > 0u);

  // Ack the request to the windowtree to create the new window.
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
  EXPECT_EQ(window_tree()->window_id(), server_id(top_level));

  ui::mojom::WindowDataPtr data = ui::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  const int64_t display_id = 1;
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, false);

  // TODO: need to check WindowTreeHost visibility.
  // EXPECT_FALSE(WindowPrivate(root2).parent_drawn());

  // Should not be able to add a top level as a child of another window.
  // TODO(sky): decide how to handle this.
  // root_window()->AddChild(top_level);
  // ASSERT_EQ(nullptr, top_level->parent());

  // Destroy the first root, shouldn't initiate tear down.
  window_tree_host.reset();
  EXPECT_EQ(initial_root_count, window_tree_client_impl()->GetRoots().size());
}

TEST_F(WindowTreeClientClientTest, NewTopLevelWindowGetsPropertiesFromData) {
  const size_t initial_root_count =
      window_tree_client_impl()->GetRoots().size();
  WindowTreeHostMus window_tree_host(window_tree_client_impl());
  Window* top_level = window_tree_host.window();
  EXPECT_EQ(initial_root_count + 1,
            window_tree_client_impl()->GetRoots().size());

  EXPECT_FALSE(IsWindowHostVisible(top_level));
  EXPECT_FALSE(top_level->TargetVisibility());

  // Ack the request to the windowtree to create the new window.
  EXPECT_EQ(window_tree()->window_id(), server_id(top_level));

  ui::mojom::WindowDataPtr data = ui::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  data->bounds.SetRect(1, 2, 3, 4);
  data->visible = true;
  const int64_t display_id = 10;
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, true);
  EXPECT_EQ(
      0u, window_tree()->GetChangeCountForType(WindowTreeChangeType::VISIBLE));

  // Make sure all the properties took.
  EXPECT_TRUE(IsWindowHostVisible(top_level));
  EXPECT_TRUE(top_level->TargetVisibility());
  // TODO: check display_id.
  EXPECT_EQ(display_id, window_tree_host.display_id());
  EXPECT_EQ(gfx::Rect(0, 0, 3, 4), top_level->bounds());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), top_level->GetHost()->GetBounds());
}

TEST_F(WindowTreeClientClientTest, NewWindowGetsAllChangesInFlight) {
  RegisterTestProperties(GetPropertyConverter());

  WindowTreeHostMus window_tree_host(window_tree_client_impl());
  Window* top_level = window_tree_host.window();

  EXPECT_FALSE(top_level->TargetVisibility());

  // Make visibility go from false->true->false. Don't ack immediately.
  top_level->Show();
  top_level->Hide();

  // Change bounds to 5, 6, 7, 8.
  window_tree_host.SetBounds(gfx::Rect(5, 6, 7, 8));
  EXPECT_EQ(gfx::Rect(0, 0, 7, 8), window_tree_host.window()->bounds());

  const uint8_t explicitly_set_test_property1_value = 2;
  top_level->SetProperty(kTestPropertyKey1,
                         explicitly_set_test_property1_value);

  // Ack the new window top level top_level Vis and bounds shouldn't change.
  ui::mojom::WindowDataPtr data = ui::mojom::WindowData::New();
  data->window_id = server_id(top_level);
  const gfx::Rect bounds_from_server(1, 2, 3, 4);
  data->bounds = bounds_from_server;
  data->visible = true;
  const uint8_t server_test_property1_value = 3;
  data->properties[kTestPropertyServerKey1] =
      ConvertToPropertyTransportValue(server_test_property1_value);
  const uint8_t server_test_property2_value = 4;
  data->properties[kTestPropertyServerKey2] =
      ConvertToPropertyTransportValue(server_test_property2_value);
  const int64_t display_id = 1;
  // Get the id of the in flight change for creating the new top_level.
  uint32_t new_window_in_flight_change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &new_window_in_flight_change_id));
  window_tree_client()->OnTopLevelCreated(new_window_in_flight_change_id,
                                          std::move(data), display_id, true);

  // The only value that should take effect is the property for 'yy' as it was
  // not in flight.
  EXPECT_FALSE(top_level->TargetVisibility());
  EXPECT_EQ(gfx::Rect(5, 6, 7, 8), window_tree_host.GetBounds());
  EXPECT_EQ(gfx::Rect(0, 0, 7, 8), top_level->bounds());
  EXPECT_EQ(explicitly_set_test_property1_value,
            top_level->GetProperty(kTestPropertyKey1));
  EXPECT_EQ(server_test_property2_value,
            top_level->GetProperty(kTestPropertyKey2));

  // Tell the client the changes failed. This should cause the values to change
  // to that of the server.
  ASSERT_TRUE(window_tree()->AckFirstChangeOfType(WindowTreeChangeType::VISIBLE,
                                                  false));
  EXPECT_FALSE(top_level->TargetVisibility());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::VISIBLE, false));
  EXPECT_TRUE(top_level->TargetVisibility());
  window_tree()->AckAllChangesOfType(WindowTreeChangeType::BOUNDS, false);
  // The bounds of the top_level is always at the origin.
  EXPECT_EQ(gfx::Rect(bounds_from_server.size()), top_level->bounds());
  // But the bounds of the WindowTreeHost is display relative.
  EXPECT_EQ(bounds_from_server,
            top_level->GetRootWindow()->GetHost()->GetBounds());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::PROPERTY, false));
  EXPECT_EQ(server_test_property1_value,
            top_level->GetProperty(kTestPropertyKey1));
  EXPECT_EQ(server_test_property2_value,
            top_level->GetProperty(kTestPropertyKey2));
}

TEST_F(WindowTreeClientClientTest, NewWindowGetsProperties) {
  RegisterTestProperties(GetPropertyConverter());
  Window window(nullptr);
  const uint8_t explicitly_set_test_property1_value = 29;
  window.SetProperty(kTestPropertyKey1, explicitly_set_test_property1_value);
  window.Init(ui::LAYER_NOT_DRAWN);
  mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties =
      window_tree()->GetLastNewWindowProperties();
  ASSERT_FALSE(transport_properties.is_null());
  std::map<std::string, std::vector<uint8_t>> properties =
      transport_properties.To<std::map<std::string, std::vector<uint8_t>>>();
  ASSERT_EQ(1u, properties.count(kTestPropertyServerKey1));
  // PropertyConverter uses int64_t values, even for smaller types like uint8_t.
  ASSERT_EQ(8u, properties[kTestPropertyServerKey1].size());
  EXPECT_EQ(static_cast<int64_t>(explicitly_set_test_property1_value),
            mojo::ConvertTo<int64_t>(properties[kTestPropertyServerKey1]));
  ASSERT_EQ(0u, properties.count(kTestPropertyServerKey2));
}

// Assertions around transient windows.
TEST_F(WindowTreeClientClientTest, Transients) {
  client::TransientWindowClient* transient_client =
      client::GetTransientWindowClient();
  Window parent(nullptr);
  parent.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&parent);
  Window transient(nullptr);
  transient.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&transient);
  window_tree()->AckAllChanges();
  transient_client->AddTransientChild(&parent, &transient);
  ASSERT_EQ(1u, window_tree()->GetChangeCountForType(
                    WindowTreeChangeType::ADD_TRANSIENT));
  EXPECT_EQ(server_id(&parent), window_tree()->transient_data().parent_id);
  EXPECT_EQ(server_id(&transient), window_tree()->transient_data().child_id);

  // Remove from the server side.
  window_tree_client()->OnTransientWindowRemoved(server_id(&parent),
                                                 server_id(&transient));
  EXPECT_EQ(nullptr, transient_client->GetTransientParent(&transient));
  window_tree()->AckAllChanges();

  // Add from the server.
  window_tree_client()->OnTransientWindowAdded(server_id(&parent),
                                               server_id(&transient));
  EXPECT_EQ(&parent, transient_client->GetTransientParent(&transient));

  // Remove locally.
  transient_client->RemoveTransientChild(&parent, &transient);
  ASSERT_EQ(1u, window_tree()->GetChangeCountForType(
                    WindowTreeChangeType::REMOVE_TRANSIENT));
  EXPECT_EQ(server_id(&transient), window_tree()->transient_data().child_id);
}

TEST_F(WindowTreeClientClientTest,
       TopLevelWindowDestroyedBeforeCreateComplete) {
  const size_t initial_root_count =
      window_tree_client_impl()->GetRoots().size();
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      base::MakeUnique<WindowTreeHostMus>(window_tree_client_impl());
  EXPECT_EQ(initial_root_count + 1,
            window_tree_client_impl()->GetRoots().size());

  ui::mojom::WindowDataPtr data = ui::mojom::WindowData::New();
  data->window_id = server_id(window_tree_host->window());

  // Destroy the window before the server has a chance to ack the window
  // creation.
  window_tree_host.reset();
  EXPECT_EQ(initial_root_count, window_tree_client_impl()->GetRoots().size());

  // Get the id of the in flight change for creating the new window.
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));

  const int64_t display_id = 1;
  window_tree_client()->OnTopLevelCreated(change_id, std::move(data),
                                          display_id, true);
  EXPECT_EQ(initial_root_count, window_tree_client_impl()->GetRoots().size());
}

TEST_F(WindowTreeClientClientTest, NewTopLevelWindowGetsProperties) {
  RegisterTestProperties(GetPropertyConverter());
  const uint8_t property_value = 11;
  std::map<std::string, std::vector<uint8_t>> properties;
  properties[kTestPropertyServerKey1] =
      ConvertToPropertyTransportValue(property_value);
  std::unique_ptr<WindowTreeHostMus> window_tree_host =
      base::MakeUnique<WindowTreeHostMus>(window_tree_client_impl(),
                                          &properties);
  // Verify the property made it to the window.
  EXPECT_EQ(property_value,
            window_tree_host->window()->GetProperty(kTestPropertyKey1));

  // Get the id of the in flight change for creating the new top level window.
  uint32_t change_id;
  ASSERT_TRUE(window_tree()->GetAndRemoveFirstChangeOfType(
      WindowTreeChangeType::NEW_TOP_LEVEL, &change_id));

  // Verify the property was sent to the server.
  mojo::Map<mojo::String, mojo::Array<uint8_t>> transport_properties =
      window_tree()->GetLastNewWindowProperties();
  ASSERT_FALSE(transport_properties.is_null());
  std::map<std::string, std::vector<uint8_t>> properties2 =
      transport_properties.To<std::map<std::string, std::vector<uint8_t>>>();
  ASSERT_EQ(1u, properties2.count(kTestPropertyServerKey1));
  // PropertyConverter uses int64_t values, even for smaller types like uint8_t.
  ASSERT_EQ(8u, properties2[kTestPropertyServerKey1].size());
  EXPECT_EQ(static_cast<int64_t>(property_value),
            mojo::ConvertTo<int64_t>(properties2[kTestPropertyServerKey1]));
}

// Tests both SetCapture and ReleaseCapture, to ensure that Window is properly
// updated on failures.
TEST_F(WindowTreeClientWmTest, ExplicitCapture) {
  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_FALSE(root_window()->HasCapture());

  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_TRUE(root_window()->HasCapture());

  root_window()->ReleaseCapture();
  EXPECT_FALSE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_TRUE(root_window()->HasCapture());

  root_window()->ReleaseCapture();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_FALSE(root_window()->HasCapture());
}

// Tests that when capture is lost, while there is a release capture request
// inflight, that the revert value of that request is updated correctly.
TEST_F(WindowTreeClientWmTest, LostCaptureDifferentInFlightChange) {
  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_TRUE(root_window()->HasCapture());

  // The ReleaseCapture should be updated to the revert of the SetCapture.
  root_window()->ReleaseCapture();

  window_tree_client()->OnCaptureChanged(0, server_id(root_window()));
  EXPECT_FALSE(root_window()->HasCapture());

  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_FALSE(root_window()->HasCapture());
}

// Tests that while two windows can inflight capture requests, that the
// WindowTreeClient only identifies one as having the current capture.
TEST_F(WindowTreeClientWmTest, TwoWindowsRequestCapture) {
  Window child(nullptr);
  child.Init(ui::LAYER_NOT_DRAWN);
  root_window()->AddChild(&child);
  child.Show();

  root_window()->SetCapture();
  EXPECT_TRUE(root_window()->HasCapture());

  child.SetCapture();
  EXPECT_TRUE(child.HasCapture());
  EXPECT_FALSE(root_window()->HasCapture());

  ASSERT_TRUE(
      window_tree()->AckFirstChangeOfType(WindowTreeChangeType::CAPTURE, true));
  EXPECT_FALSE(root_window()->HasCapture());
  EXPECT_TRUE(child.HasCapture());

  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, false));
  EXPECT_FALSE(child.HasCapture());
  EXPECT_TRUE(root_window()->HasCapture());

  window_tree_client()->OnCaptureChanged(0, server_id(root_window()));
  EXPECT_FALSE(root_window()->HasCapture());
}

TEST_F(WindowTreeClientWmTest, WindowDestroyedWhileTransientChildHasCapture) {
  std::unique_ptr<Window> transient_parent(base::MakeUnique<Window>(nullptr));
  transient_parent->Init(ui::LAYER_NOT_DRAWN);
  // Owned by |transient_parent|.
  Window* transient_child = new Window(nullptr);
  transient_child->Init(ui::LAYER_NOT_DRAWN);
  transient_parent->Show();
  transient_child->Show();
  root_window()->AddChild(transient_parent.get());
  root_window()->AddChild(transient_child);

  client::GetTransientWindowClient()->AddTransientChild(transient_parent.get(),
                                                        transient_child);

  WindowTracker tracker;
  tracker.Add(transient_parent.get());
  tracker.Add(transient_child);
  // Request a capture on the transient child, then destroy the transient
  // parent. That will destroy both windows, and should reset the capture window
  // correctly.
  transient_child->SetCapture();
  transient_parent.reset();
  EXPECT_TRUE(tracker.windows().empty());

  // Create a new Window, and attempt to place capture on that.
  Window child(nullptr);
  child.Init(ui::LAYER_NOT_DRAWN);
  child.Show();
  root_window()->AddChild(&child);
  child.SetCapture();
  EXPECT_TRUE(child.HasCapture());
}

namespace {

class CaptureRecorder : public client::CaptureClientObserver {
 public:
  explicit CaptureRecorder(Window* root_window) : root_window_(root_window) {
    client::GetCaptureClient(root_window)->AddObserver(this);
  }

  ~CaptureRecorder() override {
    client::GetCaptureClient(root_window_)->RemoveObserver(this);
  }

  void reset_capture_captured_count() { capture_changed_count_ = 0; }
  int capture_changed_count() const { return capture_changed_count_; }
  int last_gained_capture_window_id() const {
    return last_gained_capture_window_id_;
  }
  int last_lost_capture_window_id() const {
    return last_lost_capture_window_id_;
  }

  // client::CaptureClientObserver:
  void OnCaptureChanged(Window* lost_capture, Window* gained_capture) override {
    capture_changed_count_++;
    last_gained_capture_window_id_ = gained_capture ? gained_capture->id() : 0;
    last_lost_capture_window_id_ = lost_capture ? lost_capture->id() : 0;
  }

 private:
  Window* root_window_;
  int capture_changed_count_ = 0;
  int last_gained_capture_window_id_ = 0;
  int last_lost_capture_window_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CaptureRecorder);
};

}  // namespace

TEST_F(WindowTreeClientWmTest, OnWindowTreeCaptureChanged) {
  CaptureRecorder capture_recorder(root_window());

  std::unique_ptr<Window> child1(base::MakeUnique<Window>(nullptr));
  const int child1_id = 1;
  child1->Init(ui::LAYER_NOT_DRAWN);
  child1->set_id(child1_id);
  child1->Show();
  root_window()->AddChild(child1.get());

  Window child2(nullptr);
  const int child2_id = 2;
  child2.Init(ui::LAYER_NOT_DRAWN);
  child2.set_id(child2_id);
  child2.Show();
  root_window()->AddChild(&child2);

  EXPECT_EQ(0, capture_recorder.capture_changed_count());
  // Give capture to child1 and ensure everyone is notified correctly.
  child1->SetCapture();
  ASSERT_TRUE(window_tree()->AckSingleChangeOfType(
      WindowTreeChangeType::CAPTURE, true));
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(child1_id, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();

  // Deleting a window with capture should notify observers as well.
  child1.reset();

  // No capture change is sent during deletion (the server side sees the window
  // deletion too and resets internal state).
  EXPECT_EQ(
      0u, window_tree()->GetChangeCountForType(WindowTreeChangeType::CAPTURE));

  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(0, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(child1_id, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();

  // Changes originating from server should notify observers too.
  window_tree_client()->OnCaptureChanged(server_id(&child2), 0);
  EXPECT_EQ(1, capture_recorder.capture_changed_count());
  EXPECT_EQ(child2_id, capture_recorder.last_gained_capture_window_id());
  EXPECT_EQ(0, capture_recorder.last_lost_capture_window_id());
  capture_recorder.reset_capture_captured_count();
}

TEST_F(WindowTreeClientClientTest, ModalFail) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetProperty(client::kModalKey, ui::MODAL_TYPE_WINDOW);
  // Make sure server was told about it, and have the server say it failed.
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, false));
  // Type should be back to MODAL_TYPE_NONE as the server didn't accept the
  // change.
  EXPECT_EQ(ui::MODAL_TYPE_NONE, window.GetProperty(client::kModalKey));
  // There should be no more modal changes.
  EXPECT_FALSE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, false));
}

TEST_F(WindowTreeClientClientTest, ModalSuccess) {
  Window window(nullptr);
  window.Init(ui::LAYER_NOT_DRAWN);
  window.SetProperty(client::kModalKey, ui::MODAL_TYPE_WINDOW);
  // Ack change as succeeding.
  ASSERT_TRUE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, true));
  EXPECT_EQ(ui::MODAL_TYPE_WINDOW, window.GetProperty(client::kModalKey));
  // There should be no more modal changes.
  EXPECT_FALSE(
      window_tree()->AckSingleChangeOfType(WindowTreeChangeType::MODAL, false));
}

// Verifies OnWindowHierarchyChanged() deals correctly with identifying existing
// windows.
TEST_F(WindowTreeClientWmTest, OnWindowHierarchyChangedWithExistingWindow) {
  Window* window1 = new Window(nullptr);
  window1->Init(ui::LAYER_NOT_DRAWN);
  Window* window2 = new Window(nullptr);
  window2->Init(ui::LAYER_NOT_DRAWN);
  window_tree()->AckAllChanges();
  const Id server_window_id = server_id(root_window()) + 11;
  ui::mojom::WindowDataPtr data1 = ui::mojom::WindowData::New();
  ui::mojom::WindowDataPtr data2 = ui::mojom::WindowData::New();
  ui::mojom::WindowDataPtr data3 = ui::mojom::WindowData::New();
  data1->parent_id = server_id(root_window());
  data1->window_id = server_window_id;
  data1->bounds = gfx::Rect(1, 2, 3, 4);
  data2->parent_id = server_window_id;
  data2->window_id = WindowMus::Get(window1)->server_id();
  data2->bounds = gfx::Rect(1, 2, 3, 4);
  data3->parent_id = server_window_id;
  data3->window_id = WindowMus::Get(window2)->server_id();
  data3->bounds = gfx::Rect(1, 2, 3, 4);
  mojo::Array<ui::mojom::WindowDataPtr> data_array(3);
  data_array[0] = std::move(data1);
  data_array[1] = std::move(data2);
  data_array[2] = std::move(data3);
  window_tree_client()->OnWindowHierarchyChanged(
      server_window_id, 0, server_id(root_window()), std::move(data_array));
  ASSERT_FALSE(window_tree()->has_change());
  ASSERT_EQ(1u, root_window()->children().size());
  Window* server_window = root_window()->children()[0];
  EXPECT_EQ(window1->parent(), server_window);
  EXPECT_EQ(window2->parent(), server_window);
  ASSERT_EQ(2u, server_window->children().size());
  EXPECT_EQ(window1, server_window->children()[0]);
  EXPECT_EQ(window2, server_window->children()[1]);
}

}  // namespace aura
