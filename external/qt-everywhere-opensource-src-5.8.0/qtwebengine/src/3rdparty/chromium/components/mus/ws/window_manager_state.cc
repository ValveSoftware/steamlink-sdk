// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/window_manager_state.h"

#include "base/memory/weak_ptr.h"
#include "components/mus/common/event_matcher_util.h"
#include "components/mus/ws/accelerator.h"
#include "components/mus/ws/display_manager.h"
#include "components/mus/ws/platform_display.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/user_display_manager.h"
#include "components/mus/ws/user_id_tracker.h"
#include "components/mus/ws/window_manager_display_root.h"
#include "components/mus/ws/window_server.h"
#include "components/mus/ws/window_tree.h"
#include "services/shell/public/interfaces/connector.mojom.h"
#include "ui/events/event.h"

namespace mus {
namespace ws {
namespace {

// Debug accelerator IDs start far above the highest valid Windows command ID
// (0xDFFF) and Chrome's highest IDC command ID.
const uint32_t kPrintWindowsDebugAcceleratorId = 1u << 31;

base::TimeDelta GetDefaultAckTimerDelay() {
#if defined(NDEBUG)
  return base::TimeDelta::FromMilliseconds(100);
#else
  return base::TimeDelta::FromMilliseconds(1000);
#endif
}

bool EventsCanBeCoalesced(const ui::Event& one, const ui::Event& two) {
  if (one.type() != two.type() || one.flags() != two.flags())
    return false;

  // TODO(sad): wheel events can also be merged.
  if (one.type() != ui::ET_POINTER_MOVED)
    return false;

  return one.AsPointerEvent()->pointer_id() ==
         two.AsPointerEvent()->pointer_id();
}

std::unique_ptr<ui::Event> CoalesceEvents(std::unique_ptr<ui::Event> first,
                                          std::unique_ptr<ui::Event> second) {
  DCHECK(first->type() == ui::ET_POINTER_MOVED)
      << " Non-move events cannot be merged yet.";
  // For mouse moves, the new event just replaces the old event.
  return second;
}

const ServerWindow* GetEmbedRoot(const ServerWindow* window) {
  DCHECK(window);
  const ServerWindow* embed_root = window->parent();
  while (embed_root && embed_root->id().client_id == window->id().client_id)
    embed_root = embed_root->parent();
  return embed_root;
}

}  // namespace

class WindowManagerState::ProcessedEventTarget {
 public:
  ProcessedEventTarget(ServerWindow* window,
                       ClientSpecificId client_id,
                       Accelerator* accelerator)
      : client_id_(client_id) {
    tracker_.Add(window);
    if (accelerator)
      accelerator_ = accelerator->GetWeakPtr();
  }

  ~ProcessedEventTarget() {}

  // Return true if the event is still valid. The event becomes invalid if
  // the window is destroyed while waiting to dispatch.
  bool IsValid() const { return !tracker_.windows().empty(); }

  ServerWindow* window() {
    DCHECK(IsValid());
    return tracker_.windows().front();
  }

  ClientSpecificId client_id() const { return client_id_; }

  base::WeakPtr<Accelerator> accelerator() { return accelerator_; }

 private:
  ServerWindowTracker tracker_;
  const ClientSpecificId client_id_;
  base::WeakPtr<Accelerator> accelerator_;

  DISALLOW_COPY_AND_ASSIGN(ProcessedEventTarget);
};

WindowManagerState::QueuedEvent::QueuedEvent() {}
WindowManagerState::QueuedEvent::~QueuedEvent() {}

WindowManagerState::WindowManagerState(WindowTree* window_tree)
    : window_tree_(window_tree), event_dispatcher_(this), weak_factory_(this) {
  frame_decoration_values_ = mojom::FrameDecorationValues::New();
  frame_decoration_values_->max_title_bar_button_width = 0u;

  AddDebugAccelerators();
}

WindowManagerState::~WindowManagerState() {}

void WindowManagerState::SetFrameDecorationValues(
    mojom::FrameDecorationValuesPtr values) {
  got_frame_decoration_values_ = true;
  frame_decoration_values_ = values.Clone();
  display_manager()
      ->GetUserDisplayManager(user_id())
      ->OnFrameDecorationValuesChanged();
}

bool WindowManagerState::SetCapture(ServerWindow* window,
                                    ClientSpecificId client_id) {
  DCHECK(IsActive());
  if (capture_window() == window &&
      client_id == event_dispatcher_.capture_window_client_id()) {
    return true;
  }
#if !defined(NDEBUG)
  if (window) {
    WindowManagerDisplayRoot* display_root =
        display_manager()->GetWindowManagerDisplayRoot(window);
    DCHECK(display_root && display_root->window_manager_state() == this);
  }
#endif
  return event_dispatcher_.SetCaptureWindow(window, client_id);
}

void WindowManagerState::ReleaseCaptureBlockedByModalWindow(
    const ServerWindow* modal_window) {
  event_dispatcher_.ReleaseCaptureBlockedByModalWindow(modal_window);
}

void WindowManagerState::ReleaseCaptureBlockedByAnyModalWindow() {
  event_dispatcher_.ReleaseCaptureBlockedByAnyModalWindow();
}

void WindowManagerState::AddSystemModalWindow(ServerWindow* window) {
  DCHECK(!window->transient_parent());
  event_dispatcher_.AddSystemModalWindow(window);
}

const UserId& WindowManagerState::user_id() const {
  return window_tree_->user_id();
}

void WindowManagerState::OnWillDestroyTree(WindowTree* tree) {
  if (tree_awaiting_input_ack_ != tree)
    return;

  // The WindowTree is dying. So it's not going to ack the event.
  // If the dying tree matches the root |tree_| marked as handled so we don't
  // notify it of accelerators.
  OnEventAck(tree_awaiting_input_ack_, tree == window_tree_
                                           ? mojom::EventResult::HANDLED
                                           : mojom::EventResult::UNHANDLED);
}

bool WindowManagerState::IsActive() const {
  return window_server()->user_id_tracker()->active_id() == user_id();
}

void WindowManagerState::Activate(const gfx::Point& mouse_location_on_screen) {
  SetAllRootWindowsVisible(true);
  event_dispatcher_.Reset();
  event_dispatcher_.SetMousePointerScreenLocation(mouse_location_on_screen);
}

void WindowManagerState::Deactivate() {
  SetAllRootWindowsVisible(false);
  event_dispatcher_.Reset();
  // The tree is no longer active, so no point in dispatching any further
  // events.
  std::queue<std::unique_ptr<QueuedEvent>> event_queue;
  event_queue.swap(event_queue_);
}

void WindowManagerState::ProcessEvent(const ui::Event& event) {
  // If this is still waiting for an ack from a previously sent event, then
  // queue up the event to be dispatched once the ack is received.
  if (event_ack_timer_.IsRunning()) {
    if (!event_queue_.empty() && !event_queue_.back()->processed_target &&
        EventsCanBeCoalesced(*event_queue_.back()->event, event)) {
      event_queue_.back()->event = CoalesceEvents(
          std::move(event_queue_.back()->event), ui::Event::Clone(event));
      return;
    }
    QueueEvent(event, nullptr);
    return;
  }
  event_dispatcher_.ProcessEvent(event);
}

void WindowManagerState::OnEventAck(mojom::WindowTree* tree,
                                    mojom::EventResult result) {
  if (tree_awaiting_input_ack_ != tree) {
    // TODO(sad): The ack must have arrived after the timeout. We should do
    // something here, and in OnEventAckTimeout().
    return;
  }
  tree_awaiting_input_ack_ = nullptr;
  event_ack_timer_.Stop();

  if (result == mojom::EventResult::UNHANDLED && post_target_accelerator_)
    OnAccelerator(post_target_accelerator_->id(), *event_awaiting_input_ack_);

  ProcessNextEventFromQueue();
}

const WindowServer* WindowManagerState::window_server() const {
  return window_tree_->window_server();
}

WindowServer* WindowManagerState::window_server() {
  return window_tree_->window_server();
}

DisplayManager* WindowManagerState::display_manager() {
  return window_tree_->display_manager();
}

const DisplayManager* WindowManagerState::display_manager() const {
  return window_tree_->display_manager();
}

void WindowManagerState::SetAllRootWindowsVisible(bool value) {
  for (Display* display : display_manager()->displays()) {
    WindowManagerDisplayRoot* display_root =
        display->GetWindowManagerDisplayRootForUser(user_id());
    if (display_root)
      display_root->root()->SetVisible(value);
  }
}

ServerWindow* WindowManagerState::GetWindowManagerRoot(ServerWindow* window) {
  for (Display* display : display_manager()->displays()) {
    WindowManagerDisplayRoot* display_root =
        display->GetWindowManagerDisplayRootForUser(user_id());
    if (display_root && display_root->root()->parent() == window)
      return display_root->root();
  }
  NOTREACHED();
  return nullptr;
}

void WindowManagerState::OnEventAckTimeout(ClientSpecificId client_id) {
  WindowTree* hung_tree = window_server()->GetTreeWithId(client_id);
  if (hung_tree && !hung_tree->janky())
    window_tree_->ClientJankinessChanged(hung_tree);
  OnEventAck(tree_awaiting_input_ack_, mojom::EventResult::UNHANDLED);
}

void WindowManagerState::QueueEvent(
    const ui::Event& event,
    std::unique_ptr<ProcessedEventTarget> processed_event_target) {
  std::unique_ptr<QueuedEvent> queued_event(new QueuedEvent);
  queued_event->event = ui::Event::Clone(event);
  queued_event->processed_target = std::move(processed_event_target);
  event_queue_.push(std::move(queued_event));
}

void WindowManagerState::ProcessNextEventFromQueue() {
  // Loop through |event_queue_| stopping after dispatching the first valid
  // event.
  while (!event_queue_.empty()) {
    std::unique_ptr<QueuedEvent> queued_event = std::move(event_queue_.front());
    event_queue_.pop();
    if (!queued_event->processed_target) {
      event_dispatcher_.ProcessEvent(*queued_event->event);
      return;
    }
    if (queued_event->processed_target->IsValid()) {
      DispatchInputEventToWindowImpl(
          queued_event->processed_target->window(),
          queued_event->processed_target->client_id(), *queued_event->event,
          queued_event->processed_target->accelerator());
      return;
    }
  }
}

void WindowManagerState::DispatchInputEventToWindowImpl(
    ServerWindow* target,
    ClientSpecificId client_id,
    const ui::Event& event,
    base::WeakPtr<Accelerator> accelerator) {
  if (target && target->parent() == nullptr)
    target = GetWindowManagerRoot(target);

  if (event.IsMousePointerEvent()) {
    DCHECK(event_dispatcher_.mouse_cursor_source_window());

    int32_t cursor_id = 0;
    if (event_dispatcher_.GetCurrentMouseCursor(&cursor_id)) {
      WindowManagerDisplayRoot* display_root =
          display_manager()->GetWindowManagerDisplayRoot(target);
      display_root->display()->UpdateNativeCursor(cursor_id);
    }
  }

  WindowTree* tree = window_server()->GetTreeWithId(client_id);

  // TOOD(sad): Adjust this delay, possibly make this dynamic.
  const base::TimeDelta max_delay = base::debug::BeingDebugged()
                                        ? base::TimeDelta::FromDays(1)
                                        : GetDefaultAckTimerDelay();
  event_ack_timer_.Start(
      FROM_HERE, max_delay,
      base::Bind(&WindowManagerState::OnEventAckTimeout,
                 weak_factory_.GetWeakPtr(), tree->id()));

  tree_awaiting_input_ack_ = tree;
  if (accelerator) {
    event_awaiting_input_ack_ = ui::Event::Clone(event);
    post_target_accelerator_ = accelerator;
  }

  // Ignore |tree| because it will receive the event via normal dispatch.
  window_server()->SendToEventObservers(event, user_id(), tree);

  tree->DispatchInputEvent(target, event);
}

void WindowManagerState::AddDebugAccelerators() {
  // Always register the accelerators, even if they only work in debug, so that
  // keyboard behavior is the same in release and debug builds.
  mojom::EventMatcherPtr matcher = CreateKeyMatcher(
      ui::mojom::KeyboardCode::S, ui::mojom::kEventFlagControlDown |
                                      ui::mojom::kEventFlagAltDown |
                                      ui::mojom::kEventFlagShiftDown);
  event_dispatcher_.AddAccelerator(kPrintWindowsDebugAcceleratorId,
                                   std::move(matcher));
}

bool WindowManagerState::HandleDebugAccelerator(uint32_t accelerator_id) {
#if !defined(NDEBUG)
  if (accelerator_id == kPrintWindowsDebugAcceleratorId) {
    // Error so it will be collected in system logs.
    for (Display* display : display_manager()->displays()) {
      WindowManagerDisplayRoot* display_root =
          display->GetWindowManagerDisplayRootForUser(user_id());
      if (display_root) {
        LOG(ERROR) << "ServerWindow hierarchy:\n"
                   << display_root->root()->GetDebugWindowHierarchy();
      }
    }
    return true;
  }
#endif
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EventDispatcherDelegate:

void WindowManagerState::OnAccelerator(uint32_t accelerator_id,
                                       const ui::Event& event) {
  DCHECK(IsActive());
  if (HandleDebugAccelerator(accelerator_id))
    return;
  window_tree_->OnAccelerator(accelerator_id, event);
}

void WindowManagerState::SetFocusedWindowFromEventDispatcher(
    ServerWindow* new_focused_window) {
  DCHECK(IsActive());
  window_server()->SetFocusedWindow(new_focused_window);
}

ServerWindow* WindowManagerState::GetFocusedWindowForEventDispatcher() {
  return window_server()->GetFocusedWindow();
}

void WindowManagerState::SetNativeCapture(ServerWindow* window) {
  DCHECK(IsActive());
  WindowManagerDisplayRoot* display_root =
      display_manager()->GetWindowManagerDisplayRoot(window);
  DCHECK(display_root);
  platform_display_with_capture_ = display_root->display()->platform_display();
  platform_display_with_capture_->SetCapture();
}

void WindowManagerState::ReleaseNativeCapture() {
  // Tests trigger calling this without a corresponding SetNativeCapture().
  // TODO(sky): maybe abstract this away so that DCHECK can be added?
  if (!platform_display_with_capture_)
    return;

  platform_display_with_capture_->ReleaseCapture();
  platform_display_with_capture_ = nullptr;
}

void WindowManagerState::OnServerWindowCaptureLost(ServerWindow* window) {
  DCHECK(window);
  window_server()->ProcessLostCapture(window);
}

void WindowManagerState::OnMouseCursorLocationChanged(const gfx::Point& point) {
  window_server()
      ->display_manager()
      ->GetUserDisplayManager(user_id())
      ->OnMouseCursorLocationChanged(point);
}

void WindowManagerState::DispatchInputEventToWindow(ServerWindow* target,
                                                    ClientSpecificId client_id,
                                                    const ui::Event& event,
                                                    Accelerator* accelerator) {
  DCHECK(IsActive());
  // TODO(sky): this needs to see if another wms has capture and if so forward
  // to it.
  if (event_ack_timer_.IsRunning()) {
    std::unique_ptr<ProcessedEventTarget> processed_event_target(
        new ProcessedEventTarget(target, client_id, accelerator));
    QueueEvent(event, std::move(processed_event_target));
    return;
  }

  base::WeakPtr<Accelerator> weak_accelerator;
  if (accelerator)
    weak_accelerator = accelerator->GetWeakPtr();
  DispatchInputEventToWindowImpl(target, client_id, event, weak_accelerator);
}

ClientSpecificId WindowManagerState::GetEventTargetClientId(
    const ServerWindow* window,
    bool in_nonclient_area) {
  // If the event is in the non-client area the event goes to the owner of
  // the window.
  WindowTree* tree = nullptr;
  if (in_nonclient_area) {
    tree = window_server()->GetTreeWithId(window->id().client_id);
  } else {
    // If the window is an embed root, forward to the embedded window.
    tree = window_server()->GetTreeWithRoot(window);
    if (!tree)
      tree = window_server()->GetTreeWithId(window->id().client_id);
  }

  const ServerWindow* embed_root =
      tree->HasRoot(window) ? window : GetEmbedRoot(window);
  while (tree && tree->embedder_intercepts_events()) {
    DCHECK(tree->HasRoot(embed_root));
    tree = window_server()->GetTreeWithId(embed_root->id().client_id);
    embed_root = GetEmbedRoot(embed_root);
  }

  if (!tree) {
    DCHECK(in_nonclient_area);
    tree = window_tree_;
  }
  return tree->id();
}

ServerWindow* WindowManagerState::GetRootWindowContaining(
    const gfx::Point& location) {
  if (display_manager()->displays().empty())
    return nullptr;

  // TODO(sky): this isn't right. To correctly implement need bounds of
  // Display, which we aren't tracking yet. For now, use the first display.
  Display* display = *(display_manager()->displays().begin());
  WindowManagerDisplayRoot* display_root =
      display->GetWindowManagerDisplayRootForUser(user_id());
  return display_root ? display_root->root() : nullptr;
}

void WindowManagerState::OnEventTargetNotFound(const ui::Event& event) {
  window_server()->SendToEventObservers(event, user_id(),
                                        nullptr /* ignore_tree */);
}

}  // namespace ws
}  // namespace mus
