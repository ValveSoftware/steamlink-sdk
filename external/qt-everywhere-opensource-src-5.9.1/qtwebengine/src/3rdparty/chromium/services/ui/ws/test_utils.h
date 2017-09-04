// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_TEST_UTILS_H_
#define SERVICES_UI_WS_TEST_UTILS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "services/ui/public/interfaces/display_manager.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws/display.h"
#include "services/ui/ws/display_binding.h"
#include "services/ui/ws/drag_controller.h"
#include "services/ui/ws/event_dispatcher.h"
#include "services/ui/ws/frame_generator_delegate.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/platform_display_factory.h"
#include "services/ui/ws/test_change_tracker.h"
#include "services/ui/ws/user_activity_monitor.h"
#include "services/ui/ws/user_display_manager.h"
#include "services/ui/ws/user_id.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_manager_window_tree_factory_set.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"

namespace ui {
namespace ws {
namespace test {

// Collection of utilities useful in creating mus tests.

class UserDisplayManagerTestApi {
 public:
  explicit UserDisplayManagerTestApi(UserDisplayManager* udm) : udm_(udm) {}
  ~UserDisplayManagerTestApi() {}

  void SetTestObserver(mojom::DisplayManagerObserver* observer) {
    udm_->test_observer_ = observer;
    if (observer)
      udm_->OnObserverAdded(observer);
  }

 private:
  UserDisplayManager* udm_;

  DISALLOW_COPY_AND_ASSIGN(UserDisplayManagerTestApi);
};

// -----------------------------------------------------------------------------

class UserActivityMonitorTestApi {
 public:
  explicit UserActivityMonitorTestApi(UserActivityMonitor* monitor)
      : monitor_(monitor) {}

  void SetTimerTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    monitor_->idle_timer_.SetTaskRunner(task_runner);
  }

 private:
  UserActivityMonitor* monitor_;
  DISALLOW_COPY_AND_ASSIGN(UserActivityMonitorTestApi);
};

// -----------------------------------------------------------------------------

class WindowTreeTestApi {
 public:
  explicit WindowTreeTestApi(WindowTree* tree);
  ~WindowTreeTestApi();

  void set_user_id(const UserId& user_id) { tree_->user_id_ = user_id; }
  void set_window_manager_internal(mojom::WindowManager* wm_internal) {
    tree_->window_manager_internal_ = wm_internal;
  }
  void SetCanAcceptEvents(Id transport_window_id, bool can_accept_events) {
    tree_->SetCanAcceptEvents(transport_window_id, can_accept_events);
  }
  void AckOldestEvent() {
    tree_->OnWindowInputEventAck(tree_->event_ack_id_,
                                 mojom::EventResult::UNHANDLED);
  }
  void EnableCapture() { tree_->event_ack_id_ = 1u; }
  void AckLastEvent(mojom::EventResult result) {
    tree_->OnWindowInputEventAck(tree_->event_ack_id_, result);
  }
  void AckLastAccelerator(mojom::EventResult result) {
    tree_->OnAcceleratorAck(tree_->event_ack_id_, result);
  }

  void StartPointerWatcher(bool want_moves);
  void StopPointerWatcher();

 private:
  WindowTree* tree_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeTestApi);
};

// -----------------------------------------------------------------------------

class DisplayTestApi {
 public:
  explicit DisplayTestApi(Display* display);
  ~DisplayTestApi();

  void OnEvent(const ui::Event& event) { display_->OnEvent(event); }

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(DisplayTestApi);
};

// -----------------------------------------------------------------------------

class EventDispatcherTestApi {
 public:
  explicit EventDispatcherTestApi(EventDispatcher* ed) : ed_(ed) {}
  ~EventDispatcherTestApi() {}

  bool AreAnyPointersDown() const { return ed_->AreAnyPointersDown(); }
  bool is_mouse_button_down() const { return ed_->mouse_button_down_; }
  bool IsWindowPointerTarget(const ServerWindow* window) const;
  int NumberPointerTargetsForWindow(ServerWindow* window);
  ModalWindowController* modal_window_controller() const {
    return &ed_->modal_window_controller_;
  }
  ServerWindow* capture_window() { return ed_->capture_window_; }

 private:
  EventDispatcher* ed_;

  DISALLOW_COPY_AND_ASSIGN(EventDispatcherTestApi);
};

// -----------------------------------------------------------------------------

class ModalWindowControllerTestApi {
 public:
  explicit ModalWindowControllerTestApi(ModalWindowController* mwc)
      : mwc_(mwc) {}
  ~ModalWindowControllerTestApi() {}

  ServerWindow* GetActiveSystemModalWindow() const {
    return mwc_->GetActiveSystemModalWindow();
  }

 private:
  ModalWindowController* mwc_;

  DISALLOW_COPY_AND_ASSIGN(ModalWindowControllerTestApi);
};

// -----------------------------------------------------------------------------

class WindowManagerStateTestApi {
 public:
  explicit WindowManagerStateTestApi(WindowManagerState* wms) : wms_(wms) {}
  ~WindowManagerStateTestApi() {}

  void DispatchInputEventToWindow(ServerWindow* target,
                                  ClientSpecificId client_id,
                                  const ui::Event& event,
                                  Accelerator* accelerator) {
    wms_->DispatchInputEventToWindow(target, client_id, event, accelerator);
  }

  ClientSpecificId GetEventTargetClientId(ServerWindow* window,
                                          bool in_nonclient_area) {
    return wms_->GetEventTargetClientId(window, in_nonclient_area);
  }

  void ProcessEvent(const ui::Event& event) { wms_->ProcessEvent(event); }

  void OnEventAckTimeout(ClientSpecificId client_id) {
    wms_->OnEventAckTimeout(client_id);
  }

  ClientSpecificId GetEventTargetClientId(const ServerWindow* window,
                                          bool in_nonclient_area) {
    return wms_->GetEventTargetClientId(window, in_nonclient_area);
  }

  mojom::WindowTree* tree_awaiting_input_ack() {
    return wms_->tree_awaiting_input_ack_;
  }

 private:
  WindowManagerState* wms_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerStateTestApi);
};

// -----------------------------------------------------------------------------

class DragControllerTestApi {
 public:
  explicit DragControllerTestApi(DragController* op) : op_(op) {}
  ~DragControllerTestApi() {}

  size_t GetSizeOfQueueForWindow(ServerWindow* window) {
    return op_->GetSizeOfQueueForWindow(window);
  }

  ServerWindow* GetCurrentTarget() { return op_->current_target_window_; }

 private:
  DragController* op_;

  DISALLOW_COPY_AND_ASSIGN(DragControllerTestApi);
};

// -----------------------------------------------------------------------------

// Factory that always embeds the new WindowTree as the root user id.
class TestDisplayBinding : public DisplayBinding {
 public:
  explicit TestDisplayBinding(WindowServer* window_server)
      : window_server_(window_server) {}
  ~TestDisplayBinding() override {}

 private:
  // DisplayBinding:
  WindowTree* CreateWindowTree(ServerWindow* root) override;

  WindowServer* window_server_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayBinding);
};

// -----------------------------------------------------------------------------

// Factory that dispenses TestPlatformDisplays.
class TestPlatformDisplayFactory : public PlatformDisplayFactory {
 public:
  static const int64_t kFirstDisplayId;

  explicit TestPlatformDisplayFactory(mojom::Cursor* cursor_storage);
  ~TestPlatformDisplayFactory();

  // PlatformDisplayFactory:
  std::unique_ptr<PlatformDisplay> CreatePlatformDisplay() override;

 private:
  mojom::Cursor* cursor_storage_;
  int64_t next_display_id_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformDisplayFactory);
};

// -----------------------------------------------------------------------------

// A stub implementation of FrameGeneratorDelegate.
class TestFrameGeneratorDelegate : public FrameGeneratorDelegate {
 public:
  TestFrameGeneratorDelegate();
  ~TestFrameGeneratorDelegate() override;

  // FrameGeneratorDelegate:
  bool IsInHighContrastMode() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFrameGeneratorDelegate);
};

// -----------------------------------------------------------------------------

class TestWindowManager : public mojom::WindowManager {
 public:
  TestWindowManager()
      : got_create_top_level_window_(false),
        change_id_(0u),
        on_accelerator_called_(false),
        on_accelerator_id_(0u) {}
  ~TestWindowManager() override {}

  bool did_call_create_top_level_window(uint32_t* change_id) {
    if (!got_create_top_level_window_)
      return false;

    got_create_top_level_window_ = false;
    *change_id = change_id_;
    return true;
  }

  void ClearAcceleratorCalled() {
    on_accelerator_id_ = 0u;
    on_accelerator_called_ = false;
  }

  bool on_perform_move_loop_called() { return on_perform_move_loop_called_; }
  bool on_accelerator_called() { return on_accelerator_called_; }
  uint32_t on_accelerator_id() { return on_accelerator_id_; }
  bool got_display_removed() const { return got_display_removed_; }
  int64_t display_removed_id() const { return display_removed_id_; }

 private:
  // WindowManager:
  void OnConnect(uint16_t client_id) override {}
  void WmNewDisplayAdded(const display::Display& display,
                         ui::mojom::WindowDataPtr root,
                         bool drawn) override {}
  void WmDisplayRemoved(int64_t display_id) override;
  void WmDisplayModified(const display::Display& display) override {}
  void WmSetBounds(uint32_t change_id,
                   uint32_t window_id,
                   const gfx::Rect& bounds) override {}
  void WmSetProperty(uint32_t change_id,
                     uint32_t window_id,
                     const mojo::String& name,
                     mojo::Array<uint8_t> value) override {}
  void WmCreateTopLevelWindow(
      uint32_t change_id,
      ClientSpecificId requesting_client_id,
      mojo::Map<mojo::String, mojo::Array<uint8_t>> properties) override;
  void WmClientJankinessChanged(ClientSpecificId client_id,
                                bool janky) override;
  void WmPerformMoveLoop(uint32_t change_id,
                         uint32_t window_id,
                         mojom::MoveLoopSource source,
                         const gfx::Point& cursor_location) override;
  void WmCancelMoveLoop(uint32_t window_id) override;
  void OnAccelerator(uint32_t ack_id,
                     uint32_t accelerator_id,
                     std::unique_ptr<ui::Event> event) override;

  bool on_perform_move_loop_called_ = false;

  bool got_create_top_level_window_;
  uint32_t change_id_;

  bool on_accelerator_called_;
  uint32_t on_accelerator_id_;

  bool got_display_removed_ = false;
  int64_t display_removed_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManager);
};

// -----------------------------------------------------------------------------

// WindowTreeClient implementation that logs all calls to a TestChangeTracker.
class TestWindowTreeClient : public ui::mojom::WindowTreeClient {
 public:
  TestWindowTreeClient();
  ~TestWindowTreeClient() override;

  TestChangeTracker* tracker() { return &tracker_; }

  void Bind(mojo::InterfaceRequest<mojom::WindowTreeClient> request);

  void set_record_on_change_completed(bool value) {
    record_on_change_completed_ = value;
  }

 private:
  // WindowTreeClient:
  void OnEmbed(uint16_t client_id,
               mojom::WindowDataPtr root,
               ui::mojom::WindowTreePtr tree,
               int64_t display_id,
               Id focused_window_id,
               bool drawn) override;
  void OnEmbeddedAppDisconnected(uint32_t window) override;
  void OnUnembed(Id window_id) override;
  void OnCaptureChanged(Id new_capture_window_id,
                        Id old_capture_window_id) override;
  void OnTopLevelCreated(uint32_t change_id,
                         mojom::WindowDataPtr data,
                         int64_t display_id,
                         bool drawn) override;
  void OnWindowBoundsChanged(uint32_t window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnClientAreaChanged(
      uint32_t window_id,
      const gfx::Insets& new_client_area,
      mojo::Array<gfx::Rect> new_additional_client_areas) override;
  void OnTransientWindowAdded(uint32_t window_id,
                              uint32_t transient_window_id) override;
  void OnTransientWindowRemoved(uint32_t window_id,
                                uint32_t transient_window_id) override;
  void OnWindowHierarchyChanged(
      uint32_t window,
      uint32_t old_parent,
      uint32_t new_parent,
      mojo::Array<mojom::WindowDataPtr> windows) override;
  void OnWindowReordered(uint32_t window_id,
                         uint32_t relative_window_id,
                         mojom::OrderDirection direction) override;
  void OnWindowDeleted(uint32_t window) override;
  void OnWindowVisibilityChanged(uint32_t window, bool visible) override;
  void OnWindowOpacityChanged(uint32_t window,
                              float old_opacity,
                              float new_opacity) override;
  void OnWindowParentDrawnStateChanged(uint32_t window, bool drawn) override;
  void OnWindowSharedPropertyChanged(uint32_t window,
                                     const mojo::String& name,
                                     mojo::Array<uint8_t> new_data) override;
  void OnWindowInputEvent(uint32_t event_id,
                          uint32_t window,
                          std::unique_ptr<ui::Event> event,
                          bool matches_pointer_watcher) override;
  void OnPointerEventObserved(std::unique_ptr<ui::Event> event,
                              uint32_t window_id) override;
  void OnWindowFocused(uint32_t focused_window_id) override;
  void OnWindowPredefinedCursorChanged(uint32_t window_id,
                                       mojom::Cursor cursor_id) override;
  void OnWindowSurfaceChanged(Id window_id,
                              const cc::SurfaceId& surface_id,
                              const gfx::Size& frame_size,
                              float device_scale_factor) override;
  void OnDragDropStart(
      mojo::Map<mojo::String, mojo::Array<uint8_t>> mime_data) override;
  void OnDragEnter(uint32_t window,
                   uint32_t key_state,
                   const gfx::Point& position,
                   uint32_t effect_bitmask,
                   const OnDragEnterCallback& callback) override;
  void OnDragOver(uint32_t window,
                  uint32_t key_state,
                  const gfx::Point& position,
                  uint32_t effect_bitmask,
                  const OnDragOverCallback& callback) override;
  void OnDragLeave(uint32_t window) override;
  void OnCompleteDrop(uint32_t window,
                      uint32_t key_state,
                      const gfx::Point& position,
                      uint32_t effect_bitmask,
                      const OnCompleteDropCallback& callback) override;
  void OnPerformDragDropCompleted(uint32_t window,
                                  bool success,
                                  uint32_t action_taken) override;
  void OnDragDropDone() override;
  void OnChangeCompleted(uint32_t change_id, bool success) override;
  void RequestClose(uint32_t window_id) override;
  void GetWindowManager(
      mojo::AssociatedInterfaceRequest<mojom::WindowManager> internal) override;

  TestChangeTracker tracker_;
  mojo::Binding<mojom::WindowTreeClient> binding_;
  bool record_on_change_completed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeClient);
};

// -----------------------------------------------------------------------------

// WindowTreeBinding implementation that vends TestWindowTreeBinding.
class TestWindowTreeBinding : public WindowTreeBinding {
 public:
  TestWindowTreeBinding(WindowTree* tree,
                        std::unique_ptr<TestWindowTreeClient> client =
                            base::MakeUnique<TestWindowTreeClient>());
  ~TestWindowTreeBinding() override;

  std::unique_ptr<TestWindowTreeClient> ReleaseClient() {
    return std::move(client_);
  }

  WindowTree* tree() { return tree_; }
  TestWindowTreeClient* client() { return client_.get(); }
  TestWindowManager* window_manager() { return window_manager_.get(); }

  bool is_paused() const { return is_paused_; }

  // WindowTreeBinding:
  mojom::WindowManager* GetWindowManager() override;
  void SetIncomingMethodCallProcessingPaused(bool paused) override;

 protected:
  // WindowTreeBinding:
  mojom::WindowTreeClient* CreateClientForShutdown() override;

 private:
  WindowTree* tree_;
  std::unique_ptr<TestWindowTreeClient> client_;
  // This is the client created once ResetClientForShutdown() is called.
  std::unique_ptr<TestWindowTreeClient> client_after_reset_;
  bool is_paused_ = false;
  std::unique_ptr<TestWindowManager> window_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowTreeBinding);
};

// -----------------------------------------------------------------------------

// WindowServerDelegate that creates TestWindowTreeClients.
class TestWindowServerDelegate : public WindowServerDelegate {
 public:
  TestWindowServerDelegate();
  ~TestWindowServerDelegate() override;

  void set_window_server(WindowServer* window_server) {
    window_server_ = window_server;
  }

  TestWindowTreeClient* last_client() {
    return last_binding() ? last_binding()->client() : nullptr;
  }
  TestWindowTreeBinding* last_binding() {
    return bindings_.empty() ? nullptr : bindings_.back();
  }

  std::vector<TestWindowTreeBinding*>* bindings() { return &bindings_; }

  bool got_on_no_more_displays() const { return got_on_no_more_displays_; }

  // Creates |num_displays| displays.
  void CreateDisplays(int num_displays);

  Display* AddDisplay();

  // WindowServerDelegate:
  void StartDisplayInit() override;
  void OnNoMoreDisplays() override;
  std::unique_ptr<WindowTreeBinding> CreateWindowTreeBinding(
      BindingType type,
      ws::WindowServer* window_server,
      ws::WindowTree* tree,
      mojom::WindowTreeRequest* tree_request,
      mojom::WindowTreeClientPtr* client) override;
  bool IsTestConfig() const override;
  void UpdateTouchTransforms() override {}

 private:
  WindowServer* window_server_ = nullptr;
  bool got_on_no_more_displays_ = false;
  // All TestWindowTreeBinding objects created via CreateWindowTreeBinding.
  // These are owned by the corresponding WindowTree.
  std::vector<TestWindowTreeBinding*> bindings_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowServerDelegate);
};

// -----------------------------------------------------------------------------

// Helper class which creates and sets up the necessary objects for tests that
// use the WindowServer.
class WindowServerTestHelper {
 public:
  WindowServerTestHelper();
  ~WindowServerTestHelper();

  WindowServer* window_server() { return window_server_.get(); }
  mojom::Cursor cursor() const { return cursor_id_; }

  TestWindowServerDelegate* window_server_delegate() {
    return &window_server_delegate_;
  }
  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  mojom::Cursor cursor_id_;
  TestPlatformDisplayFactory platform_display_factory_;
  TestWindowServerDelegate window_server_delegate_;
  std::unique_ptr<WindowServer> window_server_;
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerTestHelper);
};

// -----------------------------------------------------------------------------

// Helper class which owns all of the necessary objects to test event targeting
// of ServerWindow objects.
class WindowEventTargetingHelper {
 public:
  WindowEventTargetingHelper();
  ~WindowEventTargetingHelper();

  // Creates |window| as an embeded window of the primary tree. This window is a
  // root window of its own tree, with bounds |window_bounds|. The bounds of the
  // root window of |display_| are defined by |root_window_bounds|.
  ServerWindow* CreatePrimaryTree(const gfx::Rect& root_window_bounds,
                                  const gfx::Rect& window_bounds);
  // Creates a secondary tree, embedded as a child of |embed_window|. The
  // resulting |window| is setup for event targeting, with bounds
  // |window_bounds|.
  void CreateSecondaryTree(ServerWindow* embed_window,
                           const gfx::Rect& window_bounds,
                           TestWindowTreeClient** out_client,
                           WindowTree** window_tree,
                           ServerWindow** window);
  // Sets the task runner for |message_loop_|
  void SetTaskRunner(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  mojom::Cursor cursor() const { return ws_test_helper_.cursor(); }
  Display* display() { return display_; }
  TestWindowTreeBinding* last_binding() {
    return ws_test_helper_.window_server_delegate()->last_binding();
  }
  TestWindowTreeClient* last_window_tree_client() {
    return ws_test_helper_.window_server_delegate()->last_client();
  }
  TestWindowTreeClient* wm_client() { return wm_client_; }
  WindowServer* window_server() { return ws_test_helper_.window_server(); }

 private:
  WindowServerTestHelper ws_test_helper_;
  // TestWindowTreeClient that is used for the WM client. Owned by
  // |window_server_delegate_|
  TestWindowTreeClient* wm_client_ = nullptr;
  // Owned by WindowServer
  TestDisplayBinding* display_binding_ = nullptr;
  // Owned by WindowServer's DisplayManager.
  Display* display_ = nullptr;
  scoped_refptr<DisplayCompositor> display_compositor_;

  DISALLOW_COPY_AND_ASSIGN(WindowEventTargetingHelper);
};

// -----------------------------------------------------------------------------

// Adds a new WM to |window_server| for |user_id|. Creates
// WindowManagerWindowTreeFactory and associated WindowTree for the WM.
void AddWindowManager(WindowServer* window_server, const UserId& user_id);

// Returns the first and only root of |tree|. If |tree| has zero or more than
// one root returns null.
ServerWindow* FirstRoot(WindowTree* tree);

// Returns the ClientWindowId of the first root of |tree|, or an empty
// ClientWindowId if |tree| has zero or more than one root.
ClientWindowId FirstRootId(WindowTree* tree);

// Returns |tree|s ClientWindowId for |window|.
ClientWindowId ClientWindowIdForWindow(WindowTree* tree,
                                       const ServerWindow* window);

// Creates a new visible window as a child of the single root of |tree|.
// |client_id| is set to the ClientWindowId of the new window.
ServerWindow* NewWindowInTree(WindowTree* tree, ClientWindowId* client_id);
ServerWindow* NewWindowInTreeWithParent(WindowTree* tree,
                                        ServerWindow* parent,
                                        ClientWindowId* client_id);

}  // namespace test
}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_TEST_UTILS_H_
