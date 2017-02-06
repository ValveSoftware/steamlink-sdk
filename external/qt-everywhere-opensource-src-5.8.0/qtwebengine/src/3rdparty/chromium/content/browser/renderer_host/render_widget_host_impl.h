// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "cc/resources/shared_bitmap.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/input/input_ack_handler.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/synthetic_gesture_packet.h"
#include "content/common/view_message_enums.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/page_zoom.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/latency_info.h"
#include "ui/gfx/native_widget_types.h"

struct FrameHostMsg_HittestData_Params;
struct ViewHostMsg_SelectionBounds_Params;
struct ViewHostMsg_UpdateRect_Params;

namespace base {
class RefCountedBytes;
}

namespace blink {
class WebInputEvent;
#if defined(OS_ANDROID)
class WebLayer;
#endif
class WebMouseEvent;
struct WebCompositionUnderline;
struct WebScreenInfo;
}

namespace cc {
class CompositorFrameAck;
}

#if defined(OS_MACOSX)
namespace device {
class PowerSaveBlocker;
}  // namespace device
#endif

namespace gfx {
class Range;
}

namespace content {

class BrowserAccessibilityManager;
class InputRouter;
class MockRenderWidgetHost;
class RenderWidgetHostOwnerDelegate;
class SyntheticGestureController;
class TimeoutMonitor;
class TouchEmulator;
class WebCursor;
struct EditCommand;
struct ResizeParams;
struct TextInputState;

// This implements the RenderWidgetHost interface that is exposed to
// embedders of content, and adds things only visible to content.
class CONTENT_EXPORT RenderWidgetHostImpl : public RenderWidgetHost,
                                            public InputRouterClient,
                                            public InputAckHandler,
                                            public TouchEmulatorClient,
                                            public IPC::Listener {
 public:
  // |routing_id| must not be MSG_ROUTING_NONE.
  // If this object outlives |delegate|, DetachDelegate() must be called when
  // |delegate| goes away.
  RenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                       RenderProcessHost* process,
                       int32_t routing_id,
                       bool hidden);
  ~RenderWidgetHostImpl() override;

  // Similar to RenderWidgetHost::FromID, but returning the Impl object.
  static RenderWidgetHostImpl* FromID(int32_t process_id, int32_t routing_id);

  // Returns all RenderWidgetHosts including swapped out ones for
  // internal use. The public interface
  // RenderWidgetHost::GetRenderWidgetHosts only returns active ones.
  static std::unique_ptr<RenderWidgetHostIterator> GetAllRenderWidgetHosts();

  // Use RenderWidgetHostImpl::From(rwh) to downcast a RenderWidgetHost to a
  // RenderWidgetHostImpl.
  static RenderWidgetHostImpl* From(RenderWidgetHost* rwh);

  void set_hung_renderer_delay(const base::TimeDelta& delay) {
    hung_renderer_delay_ = delay;
  }

  base::TimeDelta hung_renderer_delay() { return hung_renderer_delay_; }

  void set_new_content_rendering_delay_for_testing(
      const base::TimeDelta& delay) {
    new_content_rendering_delay_ = delay;
  }

  base::TimeDelta new_content_rendering_delay() {
    return new_content_rendering_delay_;
  }

  void set_owner_delegate(RenderWidgetHostOwnerDelegate* owner_delegate) {
    owner_delegate_ = owner_delegate;
  }

  RenderWidgetHostOwnerDelegate* owner_delegate() { return owner_delegate_; }

  // RenderWidgetHost implementation.
  void UpdateTextDirection(blink::WebTextDirection direction) override;
  void NotifyTextDirection() override;
  void Focus() override;
  void Blur() override;
  void SetActive(bool active) override;
  void CopyFromBackingStore(const gfx::Rect& src_rect,
                            const gfx::Size& accelerated_dst_size,
                            const ReadbackRequestCallback& callback,
                            const SkColorType preferred_color_type) override;
  bool CanCopyFromBackingStore() override;
  void LockBackingStore() override;
  void UnlockBackingStore() override;
  void ForwardMouseEvent(const blink::WebMouseEvent& mouse_event) override;
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& wheel_event) override;
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event) override;
  void ForwardGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;
  RenderProcessHost* GetProcess() const override;
  int GetRoutingID() const override;
  RenderWidgetHostViewBase* GetView() const override;
  bool IsLoading() const override;
  void ResizeRectChanged(const gfx::Rect& new_rect) override;
  void RestartHangMonitorTimeout() override;
  void DisableHangMonitorForTesting() override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;
  void WasResized() override;
  void AddKeyPressEventCallback(const KeyPressEventCallback& callback) override;
  void RemoveKeyPressEventCallback(
      const KeyPressEventCallback& callback) override;
  void AddMouseEventCallback(const MouseEventCallback& callback) override;
  void RemoveMouseEventCallback(const MouseEventCallback& callback) override;
  void AddInputEventObserver(
      RenderWidgetHost::InputEventObserver* observer) override;
  void RemoveInputEventObserver(
      RenderWidgetHost::InputEventObserver* observer) override;
  void GetWebScreenInfo(blink::WebScreenInfo* result) override;
  void HandleCompositorProto(const std::vector<uint8_t>& proto) override;

  // Notification that the screen info has changed.
  void NotifyScreenInfoChanged();

  // Forces redraw in the renderer and when the update reaches the browser
  // grabs snapshot from the compositor. Returns PNG-encoded snapshot.
  using GetSnapshotFromBrowserCallback =
      base::Callback<void(const unsigned char*, size_t)>;
  void GetSnapshotFromBrowser(const GetSnapshotFromBrowserCallback& callback);

  const NativeWebKeyboardEvent* GetLastKeyboardEvent() const;

  // Sets the View of this RenderWidgetHost.
  void SetView(RenderWidgetHostViewBase* view);

  RenderWidgetHostDelegate* delegate() const { return delegate_; }

  bool empty() const { return current_size_.IsEmpty(); }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  void Init();

  // Initializes a RenderWidgetHost that is attached to a RenderFrameHost.
  void InitForFrame();

  // Signal whether this RenderWidgetHost is owned by a RenderFrameHost, in
  // which case it does not do self-deletion.
  void set_owned_by_render_frame_host(bool owned_by_rfh) {
    owned_by_render_frame_host_ = owned_by_rfh;
  }
  bool owned_by_render_frame_host() const {
    return owned_by_render_frame_host_;
  }

  // Tells the renderer to die and optionally delete |this|.
  void ShutdownAndDestroyWidget(bool also_delete);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Sends a message to the corresponding object in the renderer.
  bool Send(IPC::Message* msg) override;

  // Indicates if the page has finished loading.
  void SetIsLoading(bool is_loading);

  // Called to notify the RenderWidget that it has been hidden or restored from
  // having been hidden.
  void WasHidden();
  void WasShown(const ui::LatencyInfo& latency_info);

  // Returns true if the RenderWidget is hidden.
  bool is_hidden() const { return is_hidden_; }

  // Called to notify the RenderWidget that its associated native window
  // got/lost focused.
  void GotFocus();
  void LostCapture();

  // Indicates whether the RenderWidgetHost thinks it is focused.
  // This is different from RenderWidgetHostView::HasFocus() in the sense that
  // it reflects what the renderer process knows: it saves the state that is
  // sent/received.
  // RenderWidgetHostView::HasFocus() is checking whether the view is focused so
  // it is possible in some edge cases that a view was requested to be focused
  // but it failed, thus HasFocus() returns false.
  bool is_focused() const { return is_focused_; }

  // Called to notify the RenderWidget that it has lost the mouse lock.
  void LostMouseLock();

  // Notifies the RenderWidget that it lost the mouse lock.
  void SendMouseLockLost();

  // Noifies the RenderWidget of the current mouse cursor visibility state.
  void SendCursorVisibilityState(bool is_visible);

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

#if defined(OS_MACOSX)
  // Pause for a moment to wait for pending repaint or resize messages sent to
  // the renderer to arrive. If pending resize messages are for an old window
  // size, then also pump through a new resize message if there is time.
  void PauseForPendingResizeOrRepaints();

  // Whether pausing may be useful.
  bool CanPauseForPendingResizeOrRepaints();

  // Wait for a surface matching the size of the widget's view, possibly
  // blocking until the renderer sends a new frame.
  void WaitForSurface();
#endif

  bool resize_ack_pending_for_testing() { return resize_ack_pending_; }

  // GPU accelerated version of GetBackingStore function. This will
  // trigger a re-composite to the view. It may fail if a resize is pending, or
  // if a composite has already been requested and not acked yet.
  bool ScheduleComposite();

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartHangMonitorTimeout(
      base::TimeDelta delay,
      RenderWidgetHostDelegate::RendererUnresponsiveType hang_monitor_reason);

  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopHangMonitorTimeout();

  // Starts the rendering timeout, which will clear displayed graphics if
  // a new compositor frame is not received before it expires.
  void StartNewContentRenderingTimeout();

  // Notification that a new compositor frame has been generated following
  // a page load. This stops |new_content_rendering_timeout_|, or prevents
  // the timer from running if the load commit message hasn't been received
  // yet.
  void OnFirstPaintAfterLoad();

  // Forwards the given message to the renderer. These are called by the view
  // when it has received a message.
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& ui_latency) override;
  void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& ui_latency);
  void ForwardMouseEventWithLatencyInfo(
      const blink::WebMouseEvent& mouse_event,
      const ui::LatencyInfo& ui_latency);
  virtual void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& ui_latency); // Virtual for testing.

  // Enables/disables touch emulation using mouse event. See TouchEmulator.
  void SetTouchEventEmulationEnabled(
      bool enabled, ui::GestureProviderConfigType config_type);

  // TouchEmulatorClient implementation.
  void ForwardEmulatedGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;
  void ForwardEmulatedTouchEvent(
      const blink::WebTouchEvent& touch_event) override;
  void SetCursor(const WebCursor& cursor) override;
  void ShowContextMenuAtPoint(const gfx::Point& point) override;

  // Queues a synthetic gesture for testing purposes.  Invokes the on_complete
  // callback when the gesture is finished running.
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      const base::Callback<void(SyntheticGesture::Result)>& on_complete);

  void CancelUpdateTextDirection();

  // Update the composition node of the renderer (or WebKit).
  // WebKit has a special node (a composition node) for input method to change
  // its text without affecting any other DOM nodes. When the input method
  // (attached to the browser) updates its text, the browser sends IPC messages
  // to update the composition node of the renderer.
  // (Read the comments of each function for its detail.)

  // Sets the text of the composition node.
  // This function can also update the cursor position and mark the specified
  // range in the composition node.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_COMPSTR flag
  //   (on Windows);
  // * when it receives a "preedit_changed" signal of GtkIMContext (on Linux);
  // * when markedText of NSTextInput is called (on Mac).
  void ImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end);

  // Finishes an ongoing composition with the specified text.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR flag
  //   (on Windows);
  // * when it receives a "commit" signal of GtkIMContext (on Linux);
  // * when insertText of NSTextInput is called (on Mac).
  void ImeConfirmComposition(const base::string16& text,
                             const gfx::Range& replacement_range,
                             bool keep_selection);

  // Cancels an ongoing composition.
  void ImeCancelComposition();

  bool ignore_input_events() const {
    return ignore_input_events_;
  }

  // Whether forwarded WebInputEvents should be dropped.
  bool ShouldDropInputEvents() const;

  bool has_touch_handler() const { return has_touch_handler_; }

  // Set the RenderView background transparency.
  void SetBackgroundOpaque(bool opaque);

  // Notifies the renderer that the next key event is bound to one or more
  // pre-defined edit commands
  void SetEditCommandsForNextKeyEvent(
      const std::vector<EditCommand>& commands);

  // Executes the edit command.
  void ExecuteEditCommand(const std::string& command,
                          const std::string& value);

  // Tells the renderer to scroll the currently focused node into rect only if
  // the currently focused node is a Text node (textfield, text area or content
  // editable divs).
  void ScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);

  // Requests the renderer to move the caret selection towards the point.
  void MoveCaret(const gfx::Point& point);

  // Called when the reponse to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  bool GotResponseToLockMouseRequest(bool allowed);

  // Tells the RenderWidget about the latest vsync parameters.
  void UpdateVSyncParameters(base::TimeTicks timebase,
                             base::TimeDelta interval);

  // Called by the view in response to OnSwapCompositorFrame.
  static void SendSwapCompositorFrameAck(
      int32_t route_id,
      uint32_t output_surface_id,
      int renderer_host_id,
      const cc::CompositorFrameAck& ack);

  // Called by the view to return resources to the compositor.
  static void SendReclaimCompositorResources(int32_t route_id,
                                             uint32_t output_surface_id,
                                             int renderer_host_id,
                                             const cc::CompositorFrameAck& ack);

  void set_allow_privileged_mouse_lock(bool allow) {
    allow_privileged_mouse_lock_ = allow;
  }

  // Resets state variables related to tracking pending size and painting.
  //
  // We need to reset these flags when we want to repaint the contents of
  // browser plugin in this RWH. Resetting these flags will ensure we ignore
  // any previous pending acks that are not relevant upon repaint.
  void ResetSizeAndRepaintPendingFlags();

  void DetachDelegate();

  // Update the renderer's cache of the screen rect of the view and window.
  void SendScreenRects();

  // Suppreses future char events until a keydown. See
  // suppress_next_char_events_.
  void SuppressNextCharEvents();

  // Called by the view in response to a flush request.
  void FlushInput();

  // Request a flush signal from the view.
  void SetNeedsFlush();

  // Indicates whether the renderer drives the RenderWidgetHosts's size or the
  // other way around.
  bool auto_resize_enabled() { return auto_resize_enabled_; }

  // The minimum size of this renderer when auto-resize is enabled.
  const gfx::Size& min_size_for_auto_resize() const {
    return min_size_for_auto_resize_;
  }

  // The maximum size of this renderer when auto-resize is enabled.
  const gfx::Size& max_size_for_auto_resize() const {
    return max_size_for_auto_resize_;
  }

  void FrameSwapped(const ui::LatencyInfo& latency_info);
  void DidReceiveRendererFrame();

  // Returns the ID that uniquely describes this component to the latency
  // subsystem.
  int64_t GetLatencyComponentId() const;

  static void CompositorFrameDrawn(
      const std::vector<ui::LatencyInfo>& latency_info);

  // Don't check whether we expected a resize ack during layout tests.
  static void DisableResizeAckCheckForTesting();

  InputRouter* input_router() { return input_router_.get(); }

  // Get the BrowserAccessibilityManager for the root of the frame tree,
  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();

  // Get the BrowserAccessibilityManager for the root of the frame tree,
  // or create it if it doesn't already exist.
  BrowserAccessibilityManager* GetOrCreateRootBrowserAccessibilityManager();

  void RejectMouseLockOrUnlockIfNecessary();

  void set_renderer_initialized(bool renderer_initialized) {
    renderer_initialized_ = renderer_initialized;
  }

  // Indicates if the render widget host should track the render widget's size
  // as opposed to visa versa.
  void SetAutoResize(bool enable,
                     const gfx::Size& min_size,
                     const gfx::Size& max_size);

  // Fills in the |resize_params| struct.
  // Returns |false| if the update is redundant, |true| otherwise.
  bool GetResizeParams(ResizeParams* resize_params);

  // Sets the |resize_params| that were sent to the renderer bundled with the
  // request to create a new RenderWidget.
  void SetInitialRenderSizeParams(const ResizeParams& resize_params);

  // Called when we receive a notification indicating that the renderer process
  // is gone. This will reset our state so that our state will be consistent if
  // a new renderer is created.
  void RendererExited(base::TerminationStatus status, int exit_code);

  // Expose increment/decrement of the in-flight event count, so
  // RenderViewHostImpl can account for in-flight beforeunload/unload events.
  int increment_in_flight_event_count() { return ++in_flight_event_count_; }
  int decrement_in_flight_event_count() {
    DCHECK_GT(in_flight_event_count_, 0);
    return --in_flight_event_count_;
  }

  bool renderer_initialized() const { return renderer_initialized_; }

 protected:
  // ---------------------------------------------------------------------------
  // The following method is overridden by RenderViewHost to send upwards to
  // its delegate.

  // Callback for notification that we failed to receive any rendered graphics
  // from a newly loaded page. Used for testing.
  virtual void NotifyNewContentRenderingTimeoutForTesting() {}

  // ---------------------------------------------------------------------------

  bool IsMouseLocked() const;

  // The View associated with the RenderWidgetHost. The lifetime of this object
  // is associated with the lifetime of the Render process. If the Renderer
  // crashes, its View is destroyed and this pointer becomes NULL, even though
  // render_view_host_ lives on to load another URL (creating a new View while
  // doing so).
  base::WeakPtr<RenderWidgetHostViewBase> view_;

 private:
  friend class MockRenderWidgetHost;

  // Tell this object to destroy itself. If |also_delete| is specified, the
  // destructor is called as well.
  void Destroy(bool also_delete);

  // Called by |hang_monitor_timeout_| on delayed response from the renderer.
  void RendererIsUnresponsive();

  // Called by |new_content_rendering_timeout_| if a renderer has loaded new
  // content but failed to produce a compositor frame in a defined time.
  void ClearDisplayedGraphics();

  // Called if we know the renderer is responsive. When we currently think the
  // renderer is unresponsive, this will clear that state and call
  // NotifyRendererResponsive.
  void RendererIsResponsive();

  // IPC message handlers
  void OnRenderProcessGone(int status, int error_code);
  void OnClose();
  void OnUpdateScreenRectsAck();
  void OnRequestMove(const gfx::Rect& pos);
  void OnSetTooltipText(const base::string16& tooltip_text,
                        blink::WebTextDirection text_direction_hint);
  bool OnSwapCompositorFrame(const IPC::Message& message);
  void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);
  void OnQueueSyntheticGesture(const SyntheticGesturePacket& gesture_packet);
  void OnSetCursor(const WebCursor& cursor);
  void OnTextInputStateChanged(const TextInputState& params);

  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds);
  void OnImeCancelComposition();
  void OnLockMouse(bool user_gesture,
                   bool last_unlocked_by_target,
                   bool privileged);
  void OnUnlockMouse();
  void OnShowDisambiguationPopup(const gfx::Rect& rect_pixels,
                                 const gfx::Size& size,
                                 const cc::SharedBitmapId& id);
  void OnSelectionChanged(const base::string16& text,
                          uint32_t offset,
                          const gfx::Range& range);
  void OnSelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params);
  void OnForwardCompositorProto(const std::vector<uint8_t>& proto);
  void OnHittestData(const FrameHostMsg_HittestData_Params& params);

  // Called (either immediately or asynchronously) after we're done with our
  // BackingStore and can send an ACK to the renderer so it can paint onto it
  // again.
  void DidUpdateBackingStore(const ViewHostMsg_UpdateRect_Params& params,
                             const base::TimeTicks& paint_start);

  // Give key press listeners a chance to handle this key press. This allow
  // widgets that don't have focus to still handle key presses.
  bool KeyPressListenersHandleEvent(const NativeWebKeyboardEvent& event);

  // InputRouterClient
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount() override;
  void DecrementInFlightEventCount() override;
  void OnHasTouchEventHandlers(bool has_handlers) override;
  void DidFlush() override;
  void DidOverscroll(const DidOverscrollParams& params) override;
  void DidStopFlinging() override;

  // Dispatch input events with latency information
  void DispatchInputEventWithLatencyInfo(const blink::WebInputEvent& event,
                                         ui::LatencyInfo* latency);

  // InputAckHandler
  void OnKeyboardEventAck(const NativeWebKeyboardEventWithLatencyInfo& event,
                          InputEventAckState ack_result) override;
  void OnMouseEventAck(const MouseEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override;
  void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override;
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckState ack_result) override;
  void OnUnexpectedEventAck(UnexpectedEventAckType type) override;

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result);

  // Called when there is a new auto resize (using a post to avoid a stack
  // which may get in recursive loops).
  void DelayedAutoResized();

  void WindowSnapshotReachedScreen(int snapshot_id);

  void OnSnapshotDataReceived(int snapshot_id,
                              const unsigned char* png,
                              size_t size);

  void OnSnapshotDataReceivedAsync(
      int snapshot_id,
      scoped_refptr<base::RefCountedBytes> png_data);

  // true if a renderer has once been valid. We use this flag to display a sad
  // tab only when we lose our renderer and not if a paint occurs during
  // initialization.
  bool renderer_initialized_;

  // True if |Destroy()| has been called.
  bool destroyed_;

  // Our delegate, which wants to know mainly about keyboard events.
  // It will remain non-NULL until DetachDelegate() is called.
  RenderWidgetHostDelegate* delegate_;

  // The delegate of the owner of this object.
  RenderWidgetHostOwnerDelegate* owner_delegate_;

  // Created during construction and guaranteed never to be NULL, but its
  // channel may be NULL if the renderer crashed, so one must always check that.
  RenderProcessHost* const process_;

  // The ID of the corresponding object in the Renderer Instance.
  const int routing_id_;

  // Indicates whether a page is loading or not.
  bool is_loading_;

  // Indicates whether a page is hidden or not. It has to stay in sync with the
  // most recent call to process_->WidgetRestored() / WidgetHidden().
  bool is_hidden_;

  // Set if we are waiting for a repaint ack for the view.
  bool repaint_ack_pending_;

  // True when waiting for RESIZE_ACK.
  bool resize_ack_pending_;

  // The current size of the RenderWidget.
  gfx::Size current_size_;

  // Resize information that was previously sent to the renderer.
  std::unique_ptr<ResizeParams> old_resize_params_;

  // The next auto resize to send.
  gfx::Size new_auto_size_;

  // True if the render widget host should track the render widget's size as
  // opposed to visa versa.
  bool auto_resize_enabled_;

  // The minimum size for the render widget if auto-resize is enabled.
  gfx::Size min_size_for_auto_resize_;

  // The maximum size for the render widget if auto-resize is enabled.
  gfx::Size max_size_for_auto_resize_;

  bool waiting_for_screen_rects_ack_;
  gfx::Rect last_view_screen_rect_;
  gfx::Rect last_window_screen_rect_;

  // Keyboard event listeners.
  std::vector<KeyPressEventCallback> key_press_event_callbacks_;

  // Mouse event callbacks.
  std::vector<MouseEventCallback> mouse_event_callbacks_;

  // Input event callbacks.
  base::ObserverList<RenderWidgetHost::InputEventObserver>
      input_event_observers_;

  // If true, then we should repaint when restoring even if we have a
  // backingstore.  This flag is set to true if we receive a paint message
  // while is_hidden_ to true.  Even though we tell the render widget to hide
  // itself, a paint message could already be in flight at that point.
  bool needs_repainting_on_restore_;

  // This is true if the renderer is currently unresponsive.
  bool is_unresponsive_;

  // This value denotes the number of input events yet to be acknowledged
  // by the renderer.
  int in_flight_event_count_;

  // Flag to detect recursive calls to GetBackingStore().
  bool in_get_backing_store_;

  // Used for UMA histogram logging to measure the time for a repaint view
  // operation to finish.
  base::TimeTicks repaint_start_time_;

  // Set to true if we shouldn't send input events from the render widget.
  bool ignore_input_events_;

  // Set when we update the text direction of the selected input element.
  bool text_direction_updated_;
  blink::WebTextDirection text_direction_;

  // Set when we cancel updating the text direction.
  // This flag also ignores succeeding update requests until we call
  // NotifyTextDirection().
  bool text_direction_canceled_;

  // Indicates if the next sequence of Char events should be suppressed or not.
  // System may translate a RawKeyDown event into zero or more Char events,
  // usually we send them to the renderer directly in sequence. However, If a
  // RawKeyDown event was not handled by the renderer but was handled by
  // our UnhandledKeyboardEvent() method, e.g. as an accelerator key, then we
  // shall not send the following sequence of Char events, which was generated
  // by this RawKeyDown event, to the renderer. Otherwise the renderer may
  // handle the Char events and cause unexpected behavior.
  // For example, pressing alt-2 may let the browser switch to the second tab,
  // but the Char event generated by alt-2 may also activate a HTML element
  // if its accesskey happens to be "2", then the user may get confused when
  // switching back to the original tab, because the content may already be
  // changed.
  bool suppress_next_char_events_;

  bool pending_mouse_lock_request_;
  bool allow_privileged_mouse_lock_;

  // Keeps track of whether the webpage has any touch event handler. If it does,
  // then touch events are sent to the renderer. Otherwise, the touch events are
  // not sent to the renderer.
  bool has_touch_handler_;

  // TODO(wjmaclean) Remove the code for supporting resending gesture events
  // when WebView transitions to OOPIF and BrowserPlugin is removed.
  // http://crbug.com/533069
  bool is_in_touchpad_gesture_scroll_;
  bool is_in_touchscreen_gesture_scroll_;

  std::unique_ptr<SyntheticGestureController> synthetic_gesture_controller_;

  std::unique_ptr<TouchEmulator> touch_emulator_;

  // Receives and handles all input events.
  std::unique_ptr<InputRouter> input_router_;

  std::unique_ptr<TimeoutMonitor> hang_monitor_timeout_;

  std::unique_ptr<TimeoutMonitor> new_content_rendering_timeout_;

  // This boolean is true if RenderWidgetHostImpl receives a compositor frame
  // from a newly loaded page before StartNewContentRenderingTimeout() is
  // called. This means that a paint for the new load has completed before
  // the browser received a DidCommitProvisionalLoad message. In that case
  // |new_content_rendering_timeout_| is not needed. The renderer will send
  // both the FirstPaintAfterLoad and DidCommitProvisionalLoad messages after
  // any new page navigation, it doesn't matter which is received first, and
  // it should not be possible to interleave other navigations in between
  // receipt of those messages (unless FirstPaintAfterLoad is prevented from
  // being sent, in which case the timer should fire).
  bool received_paint_after_load_;

  RenderWidgetHostLatencyTracker latency_tracker_;

  int next_browser_snapshot_id_;
  using PendingSnapshotMap = std::map<int, GetSnapshotFromBrowserCallback>;
  PendingSnapshotMap pending_browser_snapshots_;

  // Indicates whether a RenderFramehost has ownership, in which case this
  // object does not self destroy.
  bool owned_by_render_frame_host_;

  // Indicates whether this RenderWidgetHost thinks is focused. This is trying
  // to match what the renderer process knows. It is different from
  // RenderWidgetHostView::HasFocus in that in that the focus request may fail,
  // causing HasFocus to return false when is_focused_ is true.
  bool is_focused_;

  // This value indicates how long to wait before we consider a renderer hung.
  base::TimeDelta hung_renderer_delay_;

  // Stores the reason the hang_monitor_timeout_ has been started. Used to
  // report histograms if the renderer is hung.
  RenderWidgetHostDelegate::RendererUnresponsiveType hang_monitor_reason_;

  // This value indicates how long to wait for a new compositor frame from a
  // renderer process before clearing any previously displayed content.
  base::TimeDelta new_content_rendering_delay_;

#if defined(OS_MACOSX)
  std::unique_ptr<device::PowerSaveBlocker> power_save_blocker_;
#endif

  base::WeakPtrFactory<RenderWidgetHostImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_
