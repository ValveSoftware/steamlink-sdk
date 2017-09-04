// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/test_utils.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/output/copy_output_request.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/service_manager/public/interfaces/connector.mojom.h"
#include "services/ui/public/interfaces/cursor.mojom.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/platform_display_init_params.h"
#include "services/ui/ws/server_window_compositor_frame_sink_manager_test_api.h"
#include "services/ui/ws/window_manager_access_policy.h"
#include "services/ui/ws/window_manager_window_tree_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ws {
namespace test {
namespace {

// -----------------------------------------------------------------------------
// Empty implementation of PlatformDisplay.
class TestPlatformDisplay : public PlatformDisplay {
 public:
  explicit TestPlatformDisplay(int64_t id,
                               bool is_primary,
                               mojom::Cursor* cursor_storage)
      : id_(id),
        is_primary_(is_primary),
        cursor_storage_(cursor_storage) {
    display_metrics_.bounds = gfx::Rect(0, 0, 400, 300);
    display_metrics_.device_scale_factor = 1.f;
  }
  ~TestPlatformDisplay() override {}

  // PlatformDisplay:
  void Init(PlatformDisplayDelegate* delegate) override {
    delegate->OnAcceleratedWidgetAvailable();
  }
  int64_t GetId() const override { return id_; }
  void SetViewportSize(const gfx::Size& size) override {}
  void SetTitle(const base::string16& title) override {}
  void SetCapture() override {}
  void ReleaseCapture() override {}
  void SetCursorById(mojom::Cursor cursor) override {
    *cursor_storage_ = cursor;
  }
  void UpdateTextInputState(const ui::TextInputState& state) override {}
  void SetImeVisibility(bool visible) override {}
  gfx::Rect GetBounds() const override { return display_metrics_.bounds; }
  bool UpdateViewportMetrics(const display::ViewportMetrics& metrics) override {
    if (display_metrics_ == metrics)
      return false;
    display_metrics_ = metrics;
    return true;
  }
  const display::ViewportMetrics& GetViewportMetrics() const override {
    return display_metrics_;
  }
  bool IsPrimaryDisplay() const override { return is_primary_; }
  void OnGpuChannelEstablished(
      scoped_refptr<gpu::GpuChannelHost> host) override {}

 private:
  display::ViewportMetrics display_metrics_;

  int64_t id_;
  bool is_primary_;
  mojom::Cursor* cursor_storage_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDisplay);
};

ClientWindowId NextUnusedClientWindowId(WindowTree* tree) {
  ClientWindowId client_id;
  for (ClientSpecificId id = 1;; ++id) {
    // Used the id of the client in the upper bits to simplify things.
    const ClientWindowId client_id =
        ClientWindowId(WindowIdToTransportId(WindowId(tree->id(), id)));
    if (!tree->GetWindowByClientId(client_id))
      return client_id;
  }
}

}  // namespace

// TestPlatformDisplayFactory  -------------------------------------------------

const int64_t TestPlatformDisplayFactory::kFirstDisplayId = 1;

TestPlatformDisplayFactory::TestPlatformDisplayFactory(
    mojom::Cursor* cursor_storage)
    : cursor_storage_(cursor_storage),
      next_display_id_(kFirstDisplayId) {}

TestPlatformDisplayFactory::~TestPlatformDisplayFactory() {}

std::unique_ptr<PlatformDisplay>
TestPlatformDisplayFactory::CreatePlatformDisplay() {
  bool is_primary = (next_display_id_ == kFirstDisplayId);
  return base::MakeUnique<TestPlatformDisplay>(next_display_id_++, is_primary,
                                               cursor_storage_);
}

// TestFrameGeneratorDelegate -------------------------------------------------

TestFrameGeneratorDelegate::TestFrameGeneratorDelegate() {}

TestFrameGeneratorDelegate::~TestFrameGeneratorDelegate() {}

bool TestFrameGeneratorDelegate::IsInHighContrastMode() {
  return false;
}

// WindowTreeTestApi  ---------------------------------------------------------

WindowTreeTestApi::WindowTreeTestApi(WindowTree* tree) : tree_(tree) {}
WindowTreeTestApi::~WindowTreeTestApi() {}

void WindowTreeTestApi::StartPointerWatcher(bool want_moves) {
  tree_->StartPointerWatcher(want_moves);
}

void WindowTreeTestApi::StopPointerWatcher() {
  tree_->StopPointerWatcher();
}

// DisplayTestApi  ------------------------------------------------------------

DisplayTestApi::DisplayTestApi(Display* display) : display_(display) {}
DisplayTestApi::~DisplayTestApi() {}

// EventDispatcherTestApi  ----------------------------------------------------

bool EventDispatcherTestApi::IsWindowPointerTarget(
    const ServerWindow* window) const {
  for (const auto& pair : ed_->pointer_targets_) {
    if (pair.second.window == window)
      return true;
  }
  return false;
}

int EventDispatcherTestApi::NumberPointerTargetsForWindow(
    ServerWindow* window) {
  int count = 0;
  for (const auto& pair : ed_->pointer_targets_)
    if (pair.second.window == window)
      count++;
  return count;
}

// TestDisplayBinding ---------------------------------------------------------

WindowTree* TestDisplayBinding::CreateWindowTree(ServerWindow* root) {
  const uint32_t embed_flags = 0;
  WindowTree* tree = window_server_->EmbedAtWindow(
      root, service_manager::mojom::kRootUserID,
      ui::mojom::WindowTreeClientPtr(), embed_flags,
      base::WrapUnique(new WindowManagerAccessPolicy));
  tree->ConfigureWindowManager();
  return tree;
}

// TestWindowManager ----------------------------------------------------------

void TestWindowManager::WmDisplayRemoved(int64_t display_id) {
  got_display_removed_ = true;
  display_removed_id_ = display_id;
}

void TestWindowManager::WmCreateTopLevelWindow(
    uint32_t change_id,
    ClientSpecificId requesting_client_id,
    mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) {
  got_create_top_level_window_ = true;
  change_id_ = change_id;
}

void TestWindowManager::WmClientJankinessChanged(ClientSpecificId client_id,
                                                 bool janky) {}

void TestWindowManager::WmPerformMoveLoop(uint32_t change_id,
                                          uint32_t window_id,
                                          mojom::MoveLoopSource source,
                                          const gfx::Point& cursor_location) {
  on_perform_move_loop_called_ = true;
}

void TestWindowManager::WmCancelMoveLoop(uint32_t window_id) {}

void TestWindowManager::OnAccelerator(uint32_t ack_id,
                                      uint32_t accelerator_id,
                                      std::unique_ptr<ui::Event> event) {
  on_accelerator_called_ = true;
  on_accelerator_id_ = accelerator_id;
}

// TestWindowTreeClient -------------------------------------------------------

TestWindowTreeClient::TestWindowTreeClient()
    : binding_(this), record_on_change_completed_(false) {}
TestWindowTreeClient::~TestWindowTreeClient() {}

void TestWindowTreeClient::Bind(
    mojo::InterfaceRequest<mojom::WindowTreeClient> request) {
  binding_.Bind(std::move(request));
}

void TestWindowTreeClient::OnEmbed(uint16_t client_id,
                                   mojom::WindowDataPtr root,
                                   ui::mojom::WindowTreePtr tree,
                                   int64_t display_id,
                                   Id focused_window_id,
                                   bool drawn) {
  // TODO(sky): add test coverage of |focused_window_id|.
  tracker_.OnEmbed(client_id, std::move(root), drawn);
}

void TestWindowTreeClient::OnEmbeddedAppDisconnected(uint32_t window) {
  tracker_.OnEmbeddedAppDisconnected(window);
}

void TestWindowTreeClient::OnUnembed(Id window_id) {
  tracker_.OnUnembed(window_id);
}

void TestWindowTreeClient::OnCaptureChanged(Id new_capture_window_id,
                                            Id old_capture_window_id) {
  tracker_.OnCaptureChanged(new_capture_window_id, old_capture_window_id);
}

void TestWindowTreeClient::OnTopLevelCreated(uint32_t change_id,
                                             mojom::WindowDataPtr data,
                                             int64_t display_id,
                                             bool drawn) {
  tracker_.OnTopLevelCreated(change_id, std::move(data), drawn);
}

void TestWindowTreeClient::OnWindowBoundsChanged(uint32_t window,
                                                 const gfx::Rect& old_bounds,
                                                 const gfx::Rect& new_bounds) {
  tracker_.OnWindowBoundsChanged(window, std::move(old_bounds),
                                 std::move(new_bounds));
}

void TestWindowTreeClient::OnClientAreaChanged(
    uint32_t window_id,
    const gfx::Insets& new_client_area,
    mojo::Array<gfx::Rect> new_additional_client_areas) {}

void TestWindowTreeClient::OnTransientWindowAdded(
    uint32_t window_id,
    uint32_t transient_window_id) {}

void TestWindowTreeClient::OnTransientWindowRemoved(
    uint32_t window_id,
    uint32_t transient_window_id) {}

void TestWindowTreeClient::OnWindowHierarchyChanged(
    uint32_t window,
    uint32_t old_parent,
    uint32_t new_parent,
    mojo::Array<mojom::WindowDataPtr> windows) {
  tracker_.OnWindowHierarchyChanged(window, old_parent, new_parent,
                                    std::move(windows));
}

void TestWindowTreeClient::OnWindowReordered(uint32_t window_id,
                                             uint32_t relative_window_id,
                                             mojom::OrderDirection direction) {
  tracker_.OnWindowReordered(window_id, relative_window_id, direction);
}

void TestWindowTreeClient::OnWindowDeleted(uint32_t window) {
  tracker_.OnWindowDeleted(window);
}

void TestWindowTreeClient::OnWindowVisibilityChanged(uint32_t window,
                                                     bool visible) {
  tracker_.OnWindowVisibilityChanged(window, visible);
}

void TestWindowTreeClient::OnWindowOpacityChanged(uint32_t window,
                                                  float old_opacity,
                                                  float new_opacity) {
  tracker_.OnWindowOpacityChanged(window, new_opacity);
}

void TestWindowTreeClient::OnWindowParentDrawnStateChanged(uint32_t window,
                                                           bool drawn) {
  tracker_.OnWindowParentDrawnStateChanged(window, drawn);
}

void TestWindowTreeClient::OnWindowSharedPropertyChanged(
    uint32_t window,
    const mojo::String& name,
    mojo::Array<uint8_t> new_data) {
  tracker_.OnWindowSharedPropertyChanged(window, name, std::move(new_data));
}

void TestWindowTreeClient::OnWindowInputEvent(uint32_t event_id,
                                              uint32_t window,
                                              std::unique_ptr<ui::Event> event,
                                              bool matches_pointer_watcher) {
  tracker_.OnWindowInputEvent(window, *event.get(), matches_pointer_watcher);
}

void TestWindowTreeClient::OnPointerEventObserved(
    std::unique_ptr<ui::Event> event,
    uint32_t window_id) {
  tracker_.OnPointerEventObserved(*event.get(), window_id);
}

void TestWindowTreeClient::OnWindowFocused(uint32_t focused_window_id) {
  tracker_.OnWindowFocused(focused_window_id);
}

void TestWindowTreeClient::OnWindowPredefinedCursorChanged(
    uint32_t window_id,
    mojom::Cursor cursor_id) {
  tracker_.OnWindowPredefinedCursorChanged(window_id, cursor_id);
}

void TestWindowTreeClient::OnWindowSurfaceChanged(
    Id window_id,
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float device_scale_factor) {}

void TestWindowTreeClient::OnDragDropStart(
    mojo::Map<mojo::String, mojo::Array<uint8_t>> mime_data) {}

void TestWindowTreeClient::OnDragEnter(uint32_t window,
                                       uint32_t key_state,
                                       const gfx::Point& position,
                                       uint32_t effect_bitmask,
                                       const OnDragEnterCallback& callback) {}

void TestWindowTreeClient::OnDragOver(uint32_t window,
                                      uint32_t key_state,
                                      const gfx::Point& position,
                                      uint32_t effect_bitmask,
                                      const OnDragOverCallback& callback) {}

void TestWindowTreeClient::OnDragLeave(uint32_t window) {}

void TestWindowTreeClient::OnCompleteDrop(
    uint32_t window,
    uint32_t key_state,
    const gfx::Point& position,
    uint32_t effect_bitmask,
    const OnCompleteDropCallback& callback) {}

void TestWindowTreeClient::OnPerformDragDropCompleted(uint32_t window,
                                                      bool success,
                                                      uint32_t action_taken) {}

void TestWindowTreeClient::OnDragDropDone() {}

void TestWindowTreeClient::OnChangeCompleted(uint32_t change_id, bool success) {
  if (record_on_change_completed_)
    tracker_.OnChangeCompleted(change_id, success);
}

void TestWindowTreeClient::RequestClose(uint32_t window_id) {}

void TestWindowTreeClient::GetWindowManager(
    mojo::AssociatedInterfaceRequest<mojom::WindowManager> internal) {}

// TestWindowTreeBinding ------------------------------------------------------

TestWindowTreeBinding::TestWindowTreeBinding(
    WindowTree* tree,
    std::unique_ptr<TestWindowTreeClient> client)
    : WindowTreeBinding(client.get()),
      tree_(tree),
      client_(std::move(client)) {}

TestWindowTreeBinding::~TestWindowTreeBinding() {}

mojom::WindowManager* TestWindowTreeBinding::GetWindowManager() {
  if (!window_manager_.get())
    window_manager_ = base::MakeUnique<TestWindowManager>();
  return window_manager_.get();
}
void TestWindowTreeBinding::SetIncomingMethodCallProcessingPaused(bool paused) {
  is_paused_ = paused;
}

mojom::WindowTreeClient* TestWindowTreeBinding::CreateClientForShutdown() {
  DCHECK(!client_after_reset_);
  client_after_reset_ = base::MakeUnique<TestWindowTreeClient>();
  return client_after_reset_.get();
}

// TestWindowServerDelegate ----------------------------------------------

TestWindowServerDelegate::TestWindowServerDelegate() {}
TestWindowServerDelegate::~TestWindowServerDelegate() {}

void TestWindowServerDelegate::CreateDisplays(int num_displays) {
  DCHECK_GT(num_displays, 0);
  DCHECK(window_server_);

  for (int i = 0; i < num_displays; ++i)
    AddDisplay();
}

Display* TestWindowServerDelegate::AddDisplay() {
  // Display manages its own lifetime.
  Display* display = new Display(window_server_);
  display->Init(PlatformDisplayInitParams(), nullptr);
  return display;
}

void TestWindowServerDelegate::StartDisplayInit() {}

void TestWindowServerDelegate::OnNoMoreDisplays() {
  got_on_no_more_displays_ = true;
}

std::unique_ptr<WindowTreeBinding>
TestWindowServerDelegate::CreateWindowTreeBinding(
    BindingType type,
    ws::WindowServer* window_server,
    ws::WindowTree* tree,
    mojom::WindowTreeRequest* tree_request,
    mojom::WindowTreeClientPtr* client) {
  std::unique_ptr<TestWindowTreeBinding> binding =
      base::MakeUnique<TestWindowTreeBinding>(tree);
  bindings_.push_back(binding.get());
  return std::move(binding);
}

bool TestWindowServerDelegate::IsTestConfig() const {
  return true;
}

// WindowServerTestHelper  ---------------------------------------------------

WindowServerTestHelper::WindowServerTestHelper()
    : cursor_id_(mojom::Cursor::CURSOR_NULL),
      platform_display_factory_(&cursor_id_) {
  PlatformDisplay::set_factory_for_testing(&platform_display_factory_);
  window_server_ = base::MakeUnique<WindowServer>(&window_server_delegate_);
  window_server_delegate_.set_window_server(window_server_.get());
}

WindowServerTestHelper::~WindowServerTestHelper() {
  // Destroy |window_server_| while the message-loop is still alive.
  window_server_.reset();
}

// WindowEventTargetingHelper ------------------------------------------------

WindowEventTargetingHelper::WindowEventTargetingHelper() {
  display_ = new Display(window_server());
  display_binding_ = new TestDisplayBinding(window_server());
  display_->Init(PlatformDisplayInitParams(),
                 base::WrapUnique(display_binding_));
  wm_client_ = ws_test_helper_.window_server_delegate()->last_client();
  wm_client_->tracker()->changes()->clear();
}

WindowEventTargetingHelper::~WindowEventTargetingHelper() {}

ServerWindow* WindowEventTargetingHelper::CreatePrimaryTree(
    const gfx::Rect& root_window_bounds,
    const gfx::Rect& window_bounds) {
  WindowTree* wm_tree = window_server()->GetTreeWithId(1);
  const ClientWindowId embed_window_id(
      WindowIdToTransportId(WindowId(wm_tree->id(), 1)));
  EXPECT_TRUE(wm_tree->NewWindow(embed_window_id, ServerWindow::Properties()));
  EXPECT_TRUE(wm_tree->SetWindowVisibility(embed_window_id, true));
  EXPECT_TRUE(wm_tree->AddWindow(FirstRootId(wm_tree), embed_window_id));
  display_->root_window()->SetBounds(root_window_bounds);
  mojom::WindowTreeClientPtr client;
  mojom::WindowTreeClientRequest client_request = GetProxy(&client);
  wm_client_->Bind(std::move(client_request));
  const uint32_t embed_flags = 0;
  wm_tree->Embed(embed_window_id, std::move(client), embed_flags);
  ServerWindow* embed_window = wm_tree->GetWindowByClientId(embed_window_id);
  WindowTree* tree1 = window_server()->GetTreeWithRoot(embed_window);
  EXPECT_NE(nullptr, tree1);
  EXPECT_NE(tree1, wm_tree);
  WindowTreeTestApi(tree1).set_user_id(wm_tree->user_id());

  embed_window->SetBounds(window_bounds);

  return embed_window;
}

void WindowEventTargetingHelper::CreateSecondaryTree(
    ServerWindow* embed_window,
    const gfx::Rect& window_bounds,
    TestWindowTreeClient** out_client,
    WindowTree** window_tree,
    ServerWindow** window) {
  WindowTree* tree1 = window_server()->GetTreeWithRoot(embed_window);
  ASSERT_TRUE(tree1 != nullptr);
  const ClientWindowId child1_id(
      WindowIdToTransportId(WindowId(tree1->id(), 1)));
  EXPECT_TRUE(tree1->NewWindow(child1_id, ServerWindow::Properties()));
  ServerWindow* child1 = tree1->GetWindowByClientId(child1_id);
  ASSERT_TRUE(child1);
  EXPECT_TRUE(tree1->AddWindow(ClientWindowIdForWindow(tree1, embed_window),
                               child1_id));
  tree1->GetDisplay(embed_window)->AddActivationParent(embed_window);

  child1->SetVisible(true);
  child1->SetBounds(window_bounds);
  EnableHitTest(child1);

  TestWindowTreeClient* embed_client =
      ws_test_helper_.window_server_delegate()->last_client();
  embed_client->tracker()->changes()->clear();
  wm_client_->tracker()->changes()->clear();

  *out_client = embed_client;
  *window_tree = tree1;
  *window = child1;
}

void WindowEventTargetingHelper::SetTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  ws_test_helper_.message_loop()->SetTaskRunner(task_runner);
}

// ----------------------------------------------------------------------------

void AddWindowManager(WindowServer* window_server, const UserId& user_id) {
  window_server->window_manager_window_tree_factory_set()
      ->Add(user_id, nullptr)
      ->CreateWindowTree(nullptr, nullptr);
}

ServerWindow* FirstRoot(WindowTree* tree) {
  return tree->roots().size() == 1u
             ? tree->GetWindow((*tree->roots().begin())->id())
             : nullptr;
}

ClientWindowId FirstRootId(WindowTree* tree) {
  ServerWindow* first_root = FirstRoot(tree);
  return first_root ? ClientWindowIdForWindow(tree, first_root)
                    : ClientWindowId();
}

ClientWindowId ClientWindowIdForWindow(WindowTree* tree,
                                       const ServerWindow* window) {
  ClientWindowId client_window_id;
  // If window isn't known we'll return 0, which should then error out.
  tree->IsWindowKnown(window, &client_window_id);
  return client_window_id;
}

ServerWindow* NewWindowInTree(WindowTree* tree, ClientWindowId* client_id) {
  return NewWindowInTreeWithParent(tree, FirstRoot(tree), client_id);
}

ServerWindow* NewWindowInTreeWithParent(WindowTree* tree,
                                        ServerWindow* parent,
                                        ClientWindowId* client_id) {
  if (!parent)
    return nullptr;
  ClientWindowId parent_client_id;
  if (!tree->IsWindowKnown(parent, &parent_client_id))
    return nullptr;
  ClientWindowId client_window_id = NextUnusedClientWindowId(tree);
  if (!tree->NewWindow(client_window_id, ServerWindow::Properties()))
    return nullptr;
  if (!tree->SetWindowVisibility(client_window_id, true))
    return nullptr;
  if (!tree->AddWindow(parent_client_id, client_window_id))
    return nullptr;
  *client_id = client_window_id;
  return tree->GetWindowByClientId(client_window_id);
}

}  // namespace test
}  // namespace ws
}  // namespace ui
