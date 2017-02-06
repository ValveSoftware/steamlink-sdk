// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "cc/scheduler/begin_frame_source.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/text_input_manager.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/public/common/context_menu_params.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/cursor_client_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/display/display_observer.h"
#include "ui/events/gestures/motion_event_aura.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/selection_bound.h"
#include "ui/wm/public/activation_delegate.h"

namespace aura {
namespace client {
class ScopedTooltipDisabler;
}
}

namespace cc {
class CopyOutputRequest;
class CopyOutputResult;
class DelegatedFrameData;
}

namespace gfx {
class Canvas;
class Display;
class Point;
class Rect;
}

namespace gpu {
struct Mailbox;
}

namespace ui {
class CompositorLock;
class InputMethod;
class LocatedEvent;
#if defined(OS_WIN)
class OnScreenKeyboardObserver;
#endif
class Texture;
class TouchSelectionController;
}

namespace content {
#if defined(OS_WIN)
class LegacyRenderWidgetHostHWND;
#endif

class OverscrollController;
class RenderFrameHostImpl;
class RenderViewHostDelegateView;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class TouchSelectionControllerClientAura;
struct TextInputState;

// RenderWidgetHostView class hierarchy described in render_widget_host_view.h.
class CONTENT_EXPORT RenderWidgetHostViewAura
    : public RenderWidgetHostViewBase,
      public DelegatedFrameHostClient,
      public TextInputManager::Observer,
      public ui::TextInputClient,
      public display::DisplayObserver,
      public aura::WindowTreeHostObserver,
      public aura::WindowDelegate,
      public aura::client::ActivationDelegate,
      public aura::client::FocusChangeObserver,
      public aura::client::CursorClientObserver,
      public cc::BeginFrameObserver {
 public:
  // When |is_guest_view_hack| is true, this view isn't really the view for
  // the |widget|, a RenderWidgetHostViewGuest is.
  //
  // TODO(lazyboy): Remove |is_guest_view_hack| once BrowserPlugin has migrated
  // to use RWHVChildFrame (http://crbug.com/330264).
  RenderWidgetHostViewAura(RenderWidgetHost* host, bool is_guest_view_hack);

  // RenderWidgetHostView implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void InitAsChild(gfx::NativeView parent_view) override;
  RenderWidgetHost* GetRenderWidgetHost() const override;
  void SetSize(const gfx::Size& size) override;
  void SetBounds(const gfx::Rect& rect) override;
  gfx::Vector2dF GetLastScrollOffset() const override;
  gfx::NativeView GetNativeView() const override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::TextInputClient* GetTextInputClient() override;
  bool HasFocus() const override;
  bool IsSurfaceAvailableForCopy() const override;
  void Show() override;
  void Hide() override;
  bool IsShowing() override;
  gfx::Rect GetViewBounds() const override;
  void SetBackgroundColor(SkColor color) override;
  gfx::Size GetVisibleViewportSize() const override;
  void SetInsets(const gfx::Insets& insets) override;
  void FocusedNodeTouched(const gfx::Point& location_dips_screen,
                          bool editable) override;

  // Overridden from RenderWidgetHostViewBase:
  void InitAsPopup(RenderWidgetHostView* parent_host_view,
                   const gfx::Rect& pos) override;
  void InitAsFullscreen(RenderWidgetHostView* reference_host_view) override;
  void Focus() override;
  void UpdateCursor(const WebCursor& cursor) override;
  void SetIsLoading(bool is_loading) override;
  void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  void RenderProcessGone(base::TerminationStatus status,
                         int error_code) override;
  void Destroy() override;
  void SetTooltipText(const base::string16& tooltip_text) override;
  void SelectionChanged(const base::string16& text,
                        size_t offset,
                        const gfx::Range& range) override;
  gfx::Size GetRequestedRendererSize() const override;
  void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) override;
  void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const ReadbackRequestCallback& callback,
      const SkColorType preferred_color_type) override;
  void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(const gfx::Rect&, bool)>& callback) override;
  bool CanCopyToVideoFrame() const override;
  void BeginFrameSubscription(
      std::unique_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) override;
  void EndFrameSubscription() override;
  bool HasAcceleratedSurface(const gfx::Size& desired_size) override;
  void GetScreenInfo(blink::WebScreenInfo* results) override;
  gfx::Rect GetBoundsInRootWindow() override;
  void WheelEventAck(const blink::WebMouseWheelEvent& event,
                     InputEventAckState ack_result) override;
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result) override;
  void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                              InputEventAckState ack_result) override;
  std::unique_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget()
      override;
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event) override;
  BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate, bool for_root_frame) override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;
  bool LockMouse() override;
  void UnlockMouse() override;
  void OnSwapCompositorFrame(uint32_t output_surface_id,
                             cc::CompositorFrame frame) override;
  void ClearCompositorFrame() override;
  void DidStopFlinging() override;
  void OnDidNavigateMainFrameToNewPage() override;
  void LockCompositingSurface() override;
  void UnlockCompositingSurface() override;
  uint32_t GetSurfaceIdNamespace() override;
  uint32_t SurfaceIdNamespaceAtPoint(cc::SurfaceHittestDelegate* delegate,
                                     const gfx::Point& point,
                                     gfx::Point* transformed_point) override;
  void ProcessMouseEvent(const blink::WebMouseEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                              const ui::LatencyInfo& latency) override;
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency) override;
  void ProcessGestureEvent(const blink::WebGestureEvent& event,
                           const ui::LatencyInfo& latency) override;
  void TransformPointToLocalCoordSpace(const gfx::Point& point,
                                       cc::SurfaceId original_surface,
                                       gfx::Point* transformed_point) override;
  void FocusedNodeChanged(bool is_editable_node) override;

  // Overridden from ui::TextInputClient:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(gfx::Path* mask) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from aura::client::ActivationDelegate:
  bool ShouldActivate() const override;

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;

  // Overridden from aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // Overridden from aura::WindowTreeHostObserver:
  void OnHostMoved(const aura::WindowTreeHost* host,
                   const gfx::Point& new_origin) override;

#if defined(OS_WIN)
  // Gets the HWND of the host window.
  HWND GetHostWindowHWND() const;

  // Updates the cursor clip region. Used for mouse locking.
  void UpdateMouseLockRegion();

  // Notification that the LegacyRenderWidgetHostHWND was destroyed.
  void OnLegacyWindowDestroyed();
#endif

  // Method to indicate if this instance is shutting down or closing.
  // TODO(shrikant): Discuss around to see if it makes sense to add this method
  // as part of RenderWidgetHostView.
  bool IsClosing() const { return in_shutdown_; }

  // Sets whether the overscroll controller should be enabled for this page.
  void SetOverscrollControllerEnabled(bool enabled);

  void SnapToPhysicalPixelBoundary();

  ui::TouchSelectionController* selection_controller() const {
    return selection_controller_.get();
  }

  TouchSelectionControllerClientAura* selection_controller_client() const {
    return selection_controller_client_.get();
  }

  OverscrollController* overscroll_controller() const {
    return overscroll_controller_.get();
  }

  // Called when the context menu is about to be displayed.
  // Returns true if the context menu should be displayed. We only return false
  // on Windows if the context menu is being displayed in response to a long
  // press gesture. On Windows we should be consistent like other apps and
  // display the menu when the touch is released.
  bool OnShowContextMenu(const ContextMenuParams& params);

  // Used in tests to set a mock client for touch selection controller. It will
  // create a new touch selection controller for the new client.
  void SetSelectionControllerClientForTest(
      std::unique_ptr<TouchSelectionControllerClientAura> client);

  // Exposed for tests.
  cc::SurfaceId SurfaceIdForTesting() const override;

 protected:
  ~RenderWidgetHostViewAura() override;

  // Exposed for tests.
  aura::Window* window() { return window_; }

  DelegatedFrameHost* GetDelegatedFrameHost() const {
    return delegated_frame_host_.get();
  }

  const ui::MotionEventAura& pointer_state() const { return pointer_state_; }

 private:
  friend class InputMethodResultAuraTest;
  friend class RenderWidgetHostViewAuraCopyRequestTest;
  friend class TestInputMethodObserver;
  FRIEND_TEST_ALL_PREFIXES(InputMethodResultAuraTest,
                           FinishImeCompositionSession);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           PopupRetainsCaptureAfterMouseRelease);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SetCompositionText);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventState);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           TouchEventPositionsArentRounded);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, TouchEventSyncAsync);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, Resize);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SwapNotifiesWindow);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, RecreateLayers);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           SkippedDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, OutputSurfaceIdChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFrames);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           DiscardDelegatedFramesWithLocking);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, SoftwareDPIChange);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           UpdateCursorIfOverSelf);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           VisibleViewportTest);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           OverscrollResetsOnBlur);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           FinishCompositionByMouse);
  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest,
                           WebContentsViewReparent);

  class WindowObserver;
  friend class WindowObserver;

  class WindowAncestorObserver;
  friend class WindowAncestorObserver;

  void CreateAuraWindow();

  void UpdateCursorIfOverSelf();

  // Tracks whether SnapToPhysicalPixelBoundary() has been called.
  bool has_snapped_to_boundary() { return has_snapped_to_boundary_; }
  void ResetHasSnappedToBoundary() { has_snapped_to_boundary_ = false; }

  // Set the bounds of the window and handle size changes.  Assumes the caller
  // has already adjusted the origin of |rect| to conform to whatever coordinate
  // space is required by the aura::Window.
  void InternalSetBounds(const gfx::Rect& rect);

#if defined(OS_WIN)
  // Creates and/or updates the legacy dummy window which corresponds to
  // the bounds of the webcontents. It is needed for accessibility and
  // for scrolling to work in legacy drivers for trackpoints/trackpads, etc.
  void UpdateLegacyWin();

  bool UsesNativeWindowFrame() const;
#endif

  ui::InputMethod* GetInputMethod() const;

  // Sends shutdown request.
  void Shutdown();

  // Returns whether the widget needs an input grab to work properly.
  bool NeedsInputGrab();

  // Returns whether the widget needs to grab mouse capture to work properly.
  bool NeedsMouseCapture();

  // Confirm existing composition text in the webpage and ask the input method
  // to cancel its ongoing composition session.
  void FinishImeCompositionSession();

  // This method computes movementX/Y and keeps track of mouse location for
  // mouse lock on all mouse move events.
  void ModifyEventMovementAndCoords(blink::WebMouseEvent* event);

  // Sends an IPC to the renderer process to communicate whether or not
  // the mouse cursor is visible anywhere on the screen.
  void NotifyRendererOfCursorVisibilityState(bool is_visible);

  // If |clip| is non-empty and and doesn't contain |rect| or |clip| is empty
  // SchedulePaint() is invoked for |rect|.
  void SchedulePaintIfNotInClip(const gfx::Rect& rect, const gfx::Rect& clip);

  // Helper method to determine if, in mouse locked mode, the cursor should be
  // moved to center.
  bool ShouldMoveToCenter();

  // Called after |window_| is parented to a WindowEventDispatcher.
  void AddedToRootWindow();

  // Called prior to removing |window_| from a WindowEventDispatcher.
  void RemovingFromRootWindow();

  // DelegatedFrameHostClient implementation.
  ui::Layer* DelegatedFrameHostGetLayer() const override;
  bool DelegatedFrameHostIsVisible() const override;
  SkColor DelegatedFrameHostGetGutterColor(SkColor color) const override;
  gfx::Size DelegatedFrameHostDesiredSizeInDIP() const override;
  bool DelegatedFrameCanCreateResizeLock() const override;
  std::unique_ptr<ResizeLock> DelegatedFrameHostCreateResizeLock(
      bool defer_compositor_lock) override;
  void DelegatedFrameHostResizeLockWasReleased() override;
  void DelegatedFrameHostSendCompositorSwapAck(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) override;
  void DelegatedFrameHostSendReclaimCompositorResources(
      int output_surface_id,
      const cc::CompositorFrameAck& ack) override;
  void DelegatedFrameHostOnLostCompositorResources() override;
  void DelegatedFrameHostUpdateVSyncParameters(
      const base::TimeTicks& timebase,
      const base::TimeDelta& interval) override;
  void SetBeginFrameSource(cc::BeginFrameSource* source) override;
  bool IsAutoResizeEnabled() const override;

  // TextInputManager::Observer implementation.
  void OnUpdateTextInputStateCalled(TextInputManager* text_input_manager,
                                    RenderWidgetHostViewBase* updated_view,
                                    bool did_update_state) override;
  void OnImeCancelComposition(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override;

  // cc::BeginFrameObserver implementation.
  void OnBeginFrame(const cc::BeginFrameArgs& args) override;
  const cc::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
  void OnBeginFrameSourcePausedChanged(bool paused) override;

  // Detaches |this| from the input method object.
  void DetachFromInputMethod();

  // Before calling RenderWidgetHost::ForwardKeyboardEvent(), this method
  // calls our keybindings handler against the event and send matched
  // edit commands to renderer instead.
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& event);

  // Dismisses a Web Popup on a mouse or touch press outside the popup and its
  // parent.
  void ApplyEventFilterForPopupExit(ui::LocatedEvent* event);

  // Converts |rect| from window coordinate to screen coordinate.
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const;

  // Converts |rect| from screen coordinate to window coordinate.
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const;

  // Helper function to set keyboard focus to the main window.
  void SetKeyboardFocus();

  // Called when RenderWidget wants to start BeginFrame scheduling or stop.
  void OnSetNeedsBeginFrames(bool needs_begin_frames);

  RenderFrameHostImpl* GetFocusedFrame();

  // Returns true if the |event| passed in can be forwarded to the renderer.
  bool CanRendererHandleEvent(const ui::MouseEvent* event,
                              bool mouse_locked,
                              bool selection_popup) const;

  // Returns true when we can do SurfaceHitTesting for the event type.
  bool ShouldRouteEvent(const ui::Event* event) const;

  // Called when the parent window bounds change.
  void HandleParentBoundsChanged();

  // Called when the parent window hierarchy for our window changes.
  void ParentHierarchyChanged();

  // Helper function to be called whenever new selection information is
  // received. It will update selection controller.
  void SelectionUpdated(bool is_editable,
                        bool is_empty_text_form_control,
                        const gfx::SelectionBound& start,
                        const gfx::SelectionBound& end);

  // Helper function to create a selection controller.
  void CreateSelectionController();

  // Performs gesture handling needed for touch text selection. Sets event as
  // handled if it should not be further processed.
  void HandleGestureForTouchSelection(ui::GestureEvent* event);

  // Forwards a mouse event to this view's parent window delegate.
  void ForwardMouseEventToParent(ui::MouseEvent* event);

  // Returns the RenderViewHostDelegateView instance for this view. Returns
  // NULL on failure.
  RenderViewHostDelegateView* GetRenderViewHostDelegateView();

  // The model object.
  RenderWidgetHostImpl* const host_;

  aura::Window* window_;

  std::unique_ptr<DelegatedFrameHost> delegated_frame_host_;

  std::unique_ptr<WindowObserver> window_observer_;

  // Tracks the ancestors of the RWHVA window for window location changes.
  std::unique_ptr<WindowAncestorObserver> ancestor_window_observer_;

  // Are we in the process of closing?  Tracked so fullscreen views can avoid
  // sending a second shutdown request to the host when they lose the focus
  // after requesting shutdown for another reason (e.g. Escape key).
  bool in_shutdown_;

  // True if in the process of handling a window bounds changed notification.
  bool in_bounds_changed_;

  // Is this a fullscreen view?
  bool is_fullscreen_;

  // Our parent host view, if this is a popup.  NULL otherwise.
  RenderWidgetHostViewAura* popup_parent_host_view_;

  // Our child popup host. NULL if we do not have a child popup.
  RenderWidgetHostViewAura* popup_child_host_view_;

  class EventFilterForPopupExit;
  friend class EventFilterForPopupExit;
  std::unique_ptr<ui::EventHandler> event_filter_for_popup_exit_;

  // True when content is being loaded. Used to show an hourglass cursor.
  bool is_loading_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // Stores the current state of the active pointers targeting this
  // object.
  ui::MotionEventAura pointer_state_;

  // Bounds for the selection.
  gfx::SelectionBound selection_anchor_;
  gfx::SelectionBound selection_focus_;

  // The current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Indicates if there is onging composition text.
  bool has_composition_text_;

  // Whether return characters should be passed on to the RenderWidgetHostImpl.
  bool accept_return_character_;

  // Current tooltip text.
  base::string16 tooltip_;

  // The begin frame source being observed.  Null if none.
  cc::BeginFrameSource* begin_frame_source_;
  cc::BeginFrameArgs last_begin_frame_args_;
  bool needs_begin_frames_;

  // Used to record the last position of the mouse.
  // While the mouse is locked, they store the last known position just as mouse
  // lock was entered.
  // Relative to the upper-left corner of the view.
  gfx::Point unlocked_mouse_position_;
  // Relative to the upper-left corner of the screen.
  gfx::Point unlocked_global_mouse_position_;
  // Last cursor position relative to screen. Used to compute movementX/Y.
  gfx::Point global_mouse_position_;
  // In mouse locked mode, we synthetically move the mouse cursor to the center
  // of the window when it reaches the window borders to avoid it going outside.
  // This flag is used to differentiate between these synthetic mouse move
  // events vs. normal mouse move events.
  bool synthetic_move_sent_;

  // Used to track the state of the window we're created from. Only used when
  // created fullscreen.
  std::unique_ptr<aura::WindowTracker> host_tracker_;

  // Used to track the last cursor visibility update that was sent to the
  // renderer via NotifyRendererOfCursorVisibilityState().
  enum CursorVisibilityState {
    UNKNOWN,
    VISIBLE,
    NOT_VISIBLE,
  };
  CursorVisibilityState cursor_visibility_state_in_renderer_;

#if defined(OS_WIN)
  // The LegacyRenderWidgetHostHWND class provides a dummy HWND which is used
  // for accessibility, as the container for windowless plugins like
  // Flash/Silverlight, etc and for legacy drivers for trackpoints/trackpads,
  // etc.
  // The LegacyRenderWidgetHostHWND instance is created during the first call
  // to RenderWidgetHostViewAura::InternalSetBounds. The instance is destroyed
  // when the LegacyRenderWidgetHostHWND hwnd is destroyed.
  content::LegacyRenderWidgetHostHWND* legacy_render_widget_host_HWND_;

  // Set to true if the legacy_render_widget_host_HWND_ instance was destroyed
  // by Windows. This could happen if the browser window was destroyed by
  // DestroyWindow for e.g. This flag helps ensure that we don't try to create
  // the LegacyRenderWidgetHostHWND instance again as that would be a futile
  // exercise.
  bool legacy_window_destroyed_;

  // Contains a copy of the last context menu request parameters. Only set when
  // we receive a request to show the context menu on a long press.
  std::unique_ptr<ContextMenuParams> last_context_menu_params_;

  // Set to true if we requested the on screen keyboard to be displayed.
  bool virtual_keyboard_requested_;

  std::unique_ptr<ui::OnScreenKeyboardObserver> keyboard_observer_;
#endif

  bool has_snapped_to_boundary_;

  std::unique_ptr<TouchSelectionControllerClientAura>
      selection_controller_client_;
  std::unique_ptr<ui::TouchSelectionController> selection_controller_;

  std::unique_ptr<OverscrollController> overscroll_controller_;

  // The last scroll offset of the view.
  gfx::Vector2dF last_scroll_offset_;

  gfx::Insets insets_;

  std::vector<ui::LatencyInfo> software_latency_info_;

  std::unique_ptr<aura::client::ScopedTooltipDisabler> tooltip_disabler_;

  // True when this view acts as a platform view hack for a
  // RenderWidgetHostViewGuest.
  bool is_guest_view_hack_;

  // This flag when set ensures that we send over a notification to blink that
  // the current view has focus. Defaults to false.
  bool set_focus_on_mouse_down_or_key_event_;

  float device_scale_factor_;

  // Allows tests to send gesture events for testing without first sending a
  // corresponding touch sequence, as would be required by
  // RenderWidgetHostInputEventRouter.
  bool disable_input_event_router_for_testing_;

  base::WeakPtrFactory<RenderWidgetHostViewAura> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
