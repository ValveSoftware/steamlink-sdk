// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_state.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/mus/common/event_matcher_util.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/accelerator.h"
#include "components/mus/ws/display.h"
#include "components/mus/ws/display_binding.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/platform_display_init_params.h"
#include "components/mus/ws/server_window_surface_manager_test_api.h"
#include "components/mus/ws/test_change_tracker.h"
#include "components/mus/ws/test_server_window_delegate.h"
#include "components/mus/ws/test_utils.h"
#include "components/mus/ws/window_manager_access_policy.h"
#include "components/mus/ws/window_manager_display_root.h"
#include "components/mus/ws/window_manager_state.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace test {

class WindowManagerStateTest : public testing::Test {
 public:
  WindowManagerStateTest();
  ~WindowManagerStateTest() override {}

  std::unique_ptr<Accelerator> CreateAccelerator();

  // Creates a child |server_window| with associataed |window_tree| and
  // |test_client|. The window is setup for processing input.
  void CreateSecondaryTree(TestWindowTreeClient** test_client,
                           WindowTree** window_tree,
                           ServerWindow** server_window);

  void DispatchInputEventToWindow(ServerWindow* target,
                                  const ui::Event& event,
                                  Accelerator* accelerator);
  void OnEventAckTimeout(ClientSpecificId client_id);

  WindowTree* tree() {
    return window_event_targeting_helper_.window_server()->GetTreeWithId(1);
  }
  WindowTree* window_tree() { return window_tree_; }
  TestWindowTreeClient* window_tree_client() { return window_tree_client_; }
  ServerWindow* window() { return window_; }
  TestWindowManager* window_manager() { return &window_manager_; }
  TestWindowTreeClient* wm_client() {
    return window_event_targeting_helper_.wm_client();
  }
  TestWindowTreeClient* last_tree_client() {
    return window_event_targeting_helper_.last_window_tree_client();
  }
  WindowTree* last_tree() {
    return window_event_targeting_helper_.last_binding()->tree();
  }
  WindowManagerState* window_manager_state() { return window_manager_state_; }

  void EmbedAt(WindowTree* tree,
               const ClientWindowId& embed_window_id,
               uint32_t embed_flags,
               WindowTree** embed_tree,
               TestWindowTreeClient** embed_client_proxy) {
    mojom::WindowTreeClientPtr embed_client;
    mojom::WindowTreeClientRequest client_request = GetProxy(&embed_client);
    ASSERT_TRUE(
        tree->Embed(embed_window_id, std::move(embed_client), embed_flags));
    TestWindowTreeClient* client =
        window_event_targeting_helper_.last_window_tree_client();
    ASSERT_EQ(1u, client->tracker()->changes()->size());
    EXPECT_EQ(CHANGE_TYPE_EMBED, (*client->tracker()->changes())[0].type);
    client->tracker()->changes()->clear();
    *embed_client_proxy = client;
    *embed_tree = window_event_targeting_helper_.last_binding()->tree();
  }

  // testing::Test:
  void SetUp() override;

 private:
  WindowEventTargetingHelper window_event_targeting_helper_;

  WindowManagerState* window_manager_state_;

  // Handles WindowStateManager ack timeouts.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TestWindowManager window_manager_;
  ServerWindow* window_ = nullptr;
  WindowTree* window_tree_ = nullptr;
  TestWindowTreeClient* window_tree_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerStateTest);
};

WindowManagerStateTest::WindowManagerStateTest()
    : task_runner_(new base::TestSimpleTaskRunner) {}

std::unique_ptr<Accelerator> WindowManagerStateTest::CreateAccelerator() {
  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      ui::mojom::KeyboardCode::W, ui::mojom::kEventFlagControlDown);
  matcher->accelerator_phase = ui::mojom::AcceleratorPhase::POST_TARGET;
  uint32_t accelerator_id = 1;
  std::unique_ptr<Accelerator> accelerator(
      new Accelerator(accelerator_id, *matcher));
  return accelerator;
}

void WindowManagerStateTest::CreateSecondaryTree(
    TestWindowTreeClient** test_client,
    WindowTree** window_tree,
    ServerWindow** server_window) {
  window_event_targeting_helper_.CreateSecondaryTree(
      window_, gfx::Rect(20, 20, 20, 20), test_client, window_tree,
      server_window);
}

void WindowManagerStateTest::DispatchInputEventToWindow(
    ServerWindow* target,
    const ui::Event& event,
    Accelerator* accelerator) {
  WindowManagerStateTestApi test_api(window_manager_state_);
  ClientSpecificId client_id = test_api.GetEventTargetClientId(target, false);
  test_api.DispatchInputEventToWindow(target, client_id, event, accelerator);
}

void WindowManagerStateTest::OnEventAckTimeout(
    ClientSpecificId client_id) {
  WindowManagerStateTestApi test_api(window_manager_state_);
  test_api.OnEventAckTimeout(client_id);
}

void WindowManagerStateTest::SetUp() {
  window_event_targeting_helper_.SetTaskRunner(task_runner_);
  window_manager_state_ = window_event_targeting_helper_.display()
                              ->GetActiveWindowManagerDisplayRoot()
                              ->window_manager_state();
  window_ = window_event_targeting_helper_.CreatePrimaryTree(
      gfx::Rect(0, 0, 100, 100), gfx::Rect(0, 0, 50, 50));
  window_tree_ = window_event_targeting_helper_.last_binding()->tree();
  window_tree_client_ =
      window_event_targeting_helper_.last_window_tree_client();
  DCHECK(window_tree_->HasRoot(window_));

  WindowTreeTestApi(tree()).set_window_manager_internal(&window_manager_);
  wm_client()->tracker()->changes()->clear();
  window_tree_client_->tracker()->changes()->clear();
}

// Tests that when an event is dispatched with no accelerator, that post target
// accelerator is not triggered.
TEST_F(WindowManagerStateTest, NullAccelerator) {
  WindowManagerState* state = window_manager_state();
  EXPECT_TRUE(state);

  ServerWindow* target = window();
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  DispatchInputEventToWindow(target, key, nullptr);
  WindowTree* target_tree = window_tree();
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  WindowTreeTestApi(target_tree).AckOldestEvent();
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that when a post target accelerator is provided on an event, that it is
// called on ack.
TEST_F(WindowManagerStateTest, PostTargetAccelerator) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, key, accelerator.get());
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  WindowTreeTestApi(window_tree()).AckOldestEvent();
  EXPECT_TRUE(window_manager()->on_accelerator_called());
  EXPECT_EQ(accelerator->id(), window_manager()->on_accelerator_id());
}

// Tests that when a client handles an event that post target accelerators are
// not called.
TEST_F(WindowManagerStateTest, ClientHandlesEvent) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, key, accelerator.get());
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  window_manager_state()->OnEventAck(tree(), mojom::EventResult::HANDLED);
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that when an accelerator is deleted before an ack, that it is not
// called.
TEST_F(WindowManagerStateTest, AcceleratorDeleted) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator(CreateAccelerator());

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, key, accelerator.get());
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  accelerator.reset();
  window_manager_state()->OnEventAck(tree(), mojom::EventResult::UNHANDLED);
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that a events arriving before an ack don't notify the tree until the
// ack arrives, and that the correct accelerator is called.
TEST_F(WindowManagerStateTest, EnqueuedAccelerators) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator(CreateAccelerator());

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, key, accelerator.get());
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  tracker->changes()->clear();
  ui::KeyEvent key2(ui::ET_KEY_PRESSED, ui::VKEY_Y, ui::EF_CONTROL_DOWN);
  mojom::EventMatcherPtr matcher = mus::CreateKeyMatcher(
      ui::mojom::KeyboardCode::Y, ui::mojom::kEventFlagControlDown);
  matcher->accelerator_phase = ui::mojom::AcceleratorPhase::POST_TARGET;
  uint32_t accelerator_id = 2;
  std::unique_ptr<Accelerator> accelerator2(
      new Accelerator(accelerator_id, *matcher));
  DispatchInputEventToWindow(target, key2, accelerator2.get());
  EXPECT_TRUE(tracker->changes()->empty());

  WindowTreeTestApi(window_tree()).AckOldestEvent();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);
  EXPECT_TRUE(window_manager()->on_accelerator_called());
  EXPECT_EQ(accelerator->id(), window_manager()->on_accelerator_id());
}

// Tests that the accelerator is not sent when the tree is dying.
TEST_F(WindowManagerStateTest, DeleteTree) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();

  ServerWindow* target = window();
  DispatchInputEventToWindow(target, key, accelerator.get());
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  window_manager_state()->OnWillDestroyTree(tree());
  EXPECT_FALSE(window_manager()->on_accelerator_called());
}

// Tests that if a tree is destroyed before acking, that the accelerator is
// still sent if it is not the root tree.
TEST_F(WindowManagerStateTest, DeleteNonRootTree) {
  TestWindowTreeClient* embed_connection = nullptr;
  WindowTree* target_tree = nullptr;
  ServerWindow* target = nullptr;
  CreateSecondaryTree(&embed_connection, &target_tree, &target);
  TestWindowManager target_window_manager;
  WindowTreeTestApi(target_tree)
      .set_window_manager_internal(&target_window_manager);

  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();
  DispatchInputEventToWindow(target, key, accelerator.get());
  TestChangeTracker* tracker = embed_connection->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=2,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);
  EXPECT_TRUE(wm_client()->tracker()->changes()->empty());

  window_manager_state()->OnWillDestroyTree(target_tree);
  EXPECT_FALSE(target_window_manager.on_accelerator_called());
  EXPECT_TRUE(window_manager()->on_accelerator_called());
}

// Tests that when an ack times out that the accelerator is notified.
TEST_F(WindowManagerStateTest, AckTimeout) {
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_W, ui::EF_CONTROL_DOWN);
  std::unique_ptr<Accelerator> accelerator = CreateAccelerator();
  DispatchInputEventToWindow(window(), key, accelerator.get());
  TestChangeTracker* tracker = window_tree_client()->tracker();
  ASSERT_EQ(1u, tracker->changes()->size());
  EXPECT_EQ("InputEvent window=1,1 event_action=7",
            ChangesToDescription1(*tracker->changes())[0]);

  OnEventAckTimeout(window()->id().client_id);
  EXPECT_TRUE(window_manager()->on_accelerator_called());
  EXPECT_EQ(accelerator->id(), window_manager()->on_accelerator_id());
}

TEST_F(WindowManagerStateTest, InterceptingEmbedderReceivesEvents) {
  WindowTree* embedder_tree = tree();
  ServerWindow* embedder_root = window();
  const ClientWindowId embed_window_id(
      WindowIdToTransportId(WindowId(embedder_tree->id(), 12)));
  embedder_tree->NewWindow(embed_window_id, ServerWindow::Properties());
  ServerWindow* embedder_window =
      embedder_tree->GetWindowByClientId(embed_window_id);
  ASSERT_TRUE(embedder_tree->AddWindow(
      ClientWindowId(WindowIdToTransportId(embedder_root->id())),
      embed_window_id));

  TestWindowTreeClient* embedder_client = wm_client();

  {
    // Do a normal embed.
    const uint32_t embed_flags = 0;
    WindowTree* embed_tree = nullptr;
    TestWindowTreeClient* embed_client_proxy = nullptr;
    EmbedAt(embedder_tree, embed_window_id, embed_flags, &embed_tree,
            &embed_client_proxy);
    ASSERT_TRUE(embed_client_proxy);

    // Send an event to the embed window. It should go to the embedded client.
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                         base::TimeTicks(), 0, 0);
    DispatchInputEventToWindow(embedder_window, mouse, nullptr);
    ASSERT_EQ(1u, embed_client_proxy->tracker()->changes()->size());
    EXPECT_EQ(CHANGE_TYPE_INPUT_EVENT,
              (*embed_client_proxy->tracker()->changes())[0].type);
    WindowTreeTestApi(embed_tree).AckLastEvent(mojom::EventResult::UNHANDLED);
    embed_client_proxy->tracker()->changes()->clear();
  }

  {
    // Do an embed where the embedder wants to intercept events to the embedded
    // tree.
    const uint32_t embed_flags = mojom::kEmbedFlagEmbedderInterceptsEvents;
    WindowTree* embed_tree = nullptr;
    TestWindowTreeClient* embed_client_proxy = nullptr;
    EmbedAt(embedder_tree, embed_window_id, embed_flags, &embed_tree,
            &embed_client_proxy);
    ASSERT_TRUE(embed_client_proxy);
    embedder_client->tracker()->changes()->clear();

    // Send an event to the embed window. But this time, it should reach the
    // embedder.
    ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                         base::TimeTicks(), 0, 0);
    DispatchInputEventToWindow(embedder_window, mouse, nullptr);
    ASSERT_EQ(0u, embed_client_proxy->tracker()->changes()->size());
    ASSERT_EQ(1u, embedder_client->tracker()->changes()->size());
    EXPECT_EQ(CHANGE_TYPE_INPUT_EVENT,
              (*embedder_client->tracker()->changes())[0].type);
    WindowTreeTestApi(embedder_tree)
        .AckLastEvent(mojom::EventResult::UNHANDLED);
    embedder_client->tracker()->changes()->clear();

    // Embed another tree in the embedded tree.
    const ClientWindowId nested_embed_window_id(
        WindowIdToTransportId(WindowId(embed_tree->id(), 23)));
    embed_tree->NewWindow(nested_embed_window_id, ServerWindow::Properties());
    const ClientWindowId embed_root_id(
        WindowIdToTransportId((*embed_tree->roots().begin())->id()));
    ASSERT_TRUE(embed_tree->AddWindow(embed_root_id, nested_embed_window_id));

    WindowTree* nested_embed_tree = nullptr;
    TestWindowTreeClient* nested_embed_client_proxy = nullptr;
    EmbedAt(embed_tree, nested_embed_window_id, embed_flags, &nested_embed_tree,
            &nested_embed_client_proxy);
    ASSERT_TRUE(nested_embed_client_proxy);
    embed_client_proxy->tracker()->changes()->clear();
    embedder_client->tracker()->changes()->clear();

    // Send an event to the nested embed window. The event should still reach
    // the outermost embedder.
    ServerWindow* nested_embed_window =
        embed_tree->GetWindowByClientId(nested_embed_window_id);
    DCHECK(nested_embed_window->parent());
    mouse = ui::MouseEvent(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                           base::TimeTicks(), 0, 0);
    DispatchInputEventToWindow(nested_embed_window, mouse, nullptr);
    ASSERT_EQ(0u, nested_embed_client_proxy->tracker()->changes()->size());
    ASSERT_EQ(0u, embed_client_proxy->tracker()->changes()->size());

    ASSERT_EQ(1u, embedder_client->tracker()->changes()->size());
    EXPECT_EQ(CHANGE_TYPE_INPUT_EVENT,
              (*embedder_client->tracker()->changes())[0].type);
    WindowTreeTestApi(embedder_tree)
        .AckLastEvent(mojom::EventResult::UNHANDLED);
  }
}

}  // namespace test
}  // namespace ws
}  // namespace mus
