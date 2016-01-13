// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

#if defined(OS_MACOSX)
#include <OpenGL/OpenGL.h>
#endif

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/callback_forward.h"
#include "base/process/kill.h"
#include "base/timer/timer.h"
#include "cc/output/compositor_frame.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/display.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/surface/transport_dib.h"

class SkBitmap;

struct AccessibilityHostMsg_EventParams;
struct GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params;
struct GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params;
struct ViewHostMsg_SelectionBounds_Params;
struct ViewHostMsg_TextInputState_Params;

namespace media {
class VideoFrame;
}

namespace blink {
struct WebScreenInfo;
}

namespace content {
class BrowserAccessibilityManager;
class SyntheticGesture;
class SyntheticGestureTarget;
class WebCursor;
struct DidOverscrollParams;
struct NativeWebKeyboardEvent;
struct WebPluginGeometry;

// Basic implementation shared by concrete RenderWidgetHostView subclasses.
class CONTENT_EXPORT RenderWidgetHostViewBase : public RenderWidgetHostView,
                                                public IPC::Listener {
 public:
  virtual ~RenderWidgetHostViewBase();

  // RenderWidgetHostView implementation.
  virtual void SetBackgroundOpaque(bool opaque) OVERRIDE;
  virtual bool GetBackgroundOpaque() OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual bool IsShowingContextMenu() const OVERRIDE;
  virtual void SetShowingContextMenu(bool showing_menu) OVERRIDE;
  virtual base::string16 GetSelectedText() const OVERRIDE;
  virtual bool IsMouseLocked() OVERRIDE;
  virtual gfx::Size GetVisibleViewportSize() const OVERRIDE;
  virtual void SetInsets(const gfx::Insets& insets) OVERRIDE;
  virtual void BeginFrameSubscription(
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) OVERRIDE;
  virtual void EndFrameSubscription() OVERRIDE;

  // IPC::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Called by the host when the input flush has completed.
  void OnDidFlushInput();

  void SetPopupType(blink::WebPopupType popup_type);

  blink::WebPopupType GetPopupType();

  // Get the BrowserAccessibilityManager if it exists, may return NULL.
  BrowserAccessibilityManager* GetBrowserAccessibilityManager() const;

  void SetBrowserAccessibilityManager(BrowserAccessibilityManager* manager);

  // Return a value that is incremented each time the renderer swaps a new frame
  // to the view.
  uint32 RendererFrameNumber();

  // Called each time the RenderWidgetHost receives a new frame for display from
  // the renderer.
  void DidReceiveRendererFrame();

  // Notification that a resize or move session ended on the native widget.
  void UpdateScreenInfo(gfx::NativeView view);

  // Tells if the display property (work area/scale factor) has
  // changed since the last time.
  bool HasDisplayPropertyChanged(gfx::NativeView view);

  //----------------------------------------------------------------------------
  // The following methods can be overridden by derived classes.

  // Notifies the View that the renderer text selection has changed.
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range);

  // The requested size of the renderer. May differ from GetViewBounds().size()
  // when the view requires additional throttling.
  virtual gfx::Size GetRequestedRendererSize() const;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  virtual gfx::Size GetPhysicalBackingSize() const;

  // The height of the physical backing surface that is overdrawn opaquely in
  // the browser, for example by an on-screen-keyboard (in DPI-adjusted pixels).
  virtual float GetOverdrawBottomHeight() const;

  // Called prior to forwarding input event messages to the renderer, giving
  // the view a chance to perform in-process event filtering or processing.
  // Return values of |NOT_CONSUMED| or |UNKNOWN| will result in |input_event|
  // being forwarded.
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event);

  // Called by the host when it requires an input flush; the flush call should
  // by synchronized with BeginFrame.
  virtual void OnSetNeedsFlushInput();

  virtual void WheelEventAck(const blink::WebMouseWheelEvent& event,
                             InputEventAckState ack_result);

  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result);

  // Create a platform specific SyntheticGestureTarget implementation that will
  // be used to inject synthetic input events.
  virtual scoped_ptr<SyntheticGestureTarget> CreateSyntheticGestureTarget();

  // Return true if frame subscription is supported on this platform.
  virtual bool CanSubscribeFrame() const;

  // Create a BrowserAccessibilityManager for this view if it's possible to
  // create one and if one doesn't exist already. Some ports may not create
  // one depending on the current state.
  virtual void CreateBrowserAccessibilityManagerIfNeeded();

  virtual void OnAccessibilitySetFocus(int acc_obj_id);
  virtual void AccessibilityShowMenu(int acc_obj_id);
  virtual gfx::Point AccessibilityOriginInScreen(const gfx::Rect& bounds);

  virtual SkBitmap::Config PreferredReadbackFormat();

  // Informs that the focused DOM node has changed.
  virtual void FocusedNodeChanged(bool is_editable_node) {}

  virtual void OnSwapCompositorFrame(uint32 output_surface_id,
                                     scoped_ptr<cc::CompositorFrame> frame) {}

  // Because the associated remote WebKit instance can asynchronously
  // prevent-default on a dispatched touch event, the touch events are queued in
  // the GestureRecognizer until invocation of ProcessAckedTouchEvent releases
  // it to be consumed (when |ack_result| is NOT_CONSUMED OR NO_CONSUMER_EXISTS)
  // or ignored (when |ack_result| is CONSUMED).
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result) {}

  virtual void DidOverscroll(const DidOverscrollParams& params) {}

  virtual void DidStopFlinging() {}

  //----------------------------------------------------------------------------
  // The following static methods are implemented by each platform.

  static void GetDefaultScreenInfo(blink::WebScreenInfo* results);

  //----------------------------------------------------------------------------
  // The following pure virtual methods are implemented by derived classes.

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  // |reference_host_view| is the view associated with the creating page that
  // helps to position the full screen widget on the correct monitor.
  virtual void InitAsFullscreen(RenderWidgetHostView* reference_host_view) = 0;

  // Notifies the View that it has become visible.
  virtual void WasShown() = 0;

  // Notifies the View that it has been hidden.
  virtual void WasHidden() = 0;

  // Moves all plugin windows as described in the given list.
  // |scroll_offset| is the scroll offset of the render view.
  virtual void MovePluginWindows(
      const std::vector<WebPluginGeometry>& moves) = 0;

  // Take focus from the associated View component.
  virtual void Blur() = 0;

  // Sets the cursor to the one associated with the specified cursor_type
  virtual void UpdateCursor(const WebCursor& cursor) = 0;

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Updates the type of the input method attached to the view.
  virtual void TextInputStateChanged(
      const ViewHostMsg_TextInputState_Params& params) = 0;

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition() = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy() = 0;

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const base::string16& tooltip_text) = 0;

  // Notifies the View that the renderer selection bounds has changed.
  // |start_rect| and |end_rect| are the bounds end of the selection in the
  // coordinate system of the render view. |start_direction| and |end_direction|
  // indicates the direction at which the selection was made on touch devices.
  virtual void SelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params) = 0;

  // Notifies the view that the scroll offset has changed.
  virtual void ScrollOffsetChanged() = 0;

  // Copies the contents of the compositing surface into the given
  // (uninitialized) PlatformCanvas if any.
  // The rectangle region specified with |src_subrect| is copied from the
  // contents, scaled to |dst_size|, and written to |output|.
  // |callback| is invoked with true on success, false otherwise. |output| can
  // be initialized even on failure.
  // A smaller region than |src_subrect| may be copied if the underlying surface
  // is smaller than |src_subrect|.
  // NOTE: |callback| is called asynchronously.
  virtual void CopyFromCompositingSurface(
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      const base::Callback<void(bool, const SkBitmap&)>& callback,
      const SkBitmap::Config config) = 0;

  // Copies a given subset of the compositing surface's content into a YV12
  // VideoFrame, and invokes a callback with a success/fail parameter. |target|
  // must contain an allocated, YV12 video frame of the intended size. If the
  // copy rectangle does not match |target|'s size, the copied content will be
  // scaled and letterboxed with black borders. The copy will happen
  // asynchronously. This operation will fail if there is no available
  // compositing surface.
  virtual void CopyFromCompositingSurfaceToVideoFrame(
      const gfx::Rect& src_subrect,
      const scoped_refptr<media::VideoFrame>& target,
      const base::Callback<void(bool)>& callback) = 0;

  // Returns true if CopyFromCompositingSurfaceToVideoFrame() is likely to
  // succeed.
  //
  // TODO(nick): When VideoFrame copies are broadly implemented, this method
  // should be renamed to HasCompositingSurface(), or unified with
  // IsSurfaceAvailableForCopy() and HasAcceleratedSurface().
  virtual bool CanCopyToVideoFrame() const = 0;

  // Called when an accelerated compositing surface is initialized.
  virtual void AcceleratedSurfaceInitialized(int host_id, int route_id) = 0;
  // |params.window| and |params.surface_id| indicate which accelerated
  // surface's buffers swapped. |params.renderer_id| and |params.route_id|
  // are used to formulate a reply to the GPU process to prevent it from getting
  // too far ahead. They may all be zero, in which case no flow control is
  // enforced; this case is currently used for accelerated plugins.
  virtual void AcceleratedSurfaceBuffersSwapped(
      const GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params& params_in_pixel,
      int gpu_host_id) = 0;
  // Similar to above, except |params.(x|y|width|height)| define the region
  // of the surface that changed.
  virtual void AcceleratedSurfacePostSubBuffer(
      const GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params& params_in_pixel,
      int gpu_host_id) = 0;

  // Release the accelerated surface temporarily. It will be recreated on the
  // next swap buffers or post sub buffer.
  virtual void AcceleratedSurfaceSuspend() = 0;

  virtual void AcceleratedSurfaceRelease() = 0;

  // Return true if the view has an accelerated surface that contains the last
  // presented frame for the view. If |desired_size| is non-empty, true is
  // returned only if the accelerated surface size matches.
  virtual bool HasAcceleratedSurface(const gfx::Size& desired_size) = 0;

  virtual void GetScreenInfo(blink::WebScreenInfo* results) = 0;

  // Gets the bounds of the window, in screen coordinates.
  virtual gfx::Rect GetBoundsInRootWindow() = 0;

  virtual gfx::GLSurfaceHandle GetCompositingSurface() = 0;

  virtual void OnTextSurroundingSelectionResponse(const base::string16& content,
                                                  size_t start_offset,
                                                  size_t end_offset) {};

#if defined(OS_ANDROID)
  virtual void ShowDisambiguationPopup(const gfx::Rect& target_rect,
                                       const SkBitmap& zoomed_bitmap) = 0;

  // Instructs the view to not drop the surface even when the view is hidden.
  virtual void LockCompositingSurface() = 0;
  virtual void UnlockCompositingSurface() = 0;
#endif

#if defined(OS_MACOSX)
  // Does any event handling necessary for plugin IME; should be called after
  // the plugin has already had a chance to process the event. If plugin IME is
  // not enabled, this is a no-op, so it is always safe to call.
  // Returns true if the event was handled by IME.
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) = 0;
#endif

#if defined(OS_MACOSX) || defined(USE_AURA)
  // Updates the range of the marked text in an IME composition.
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) = 0;
#endif

#if defined(OS_WIN)
  virtual void SetParentNativeViewAccessible(
      gfx::NativeViewAccessible accessible_parent) = 0;

  // Returns an HWND that's given as the parent window for windowless Flash to
  // workaround crbug.com/301548.
  virtual gfx::NativeViewId GetParentForWindowlessPlugin() const = 0;

  // The callback that DetachPluginsHelper calls for each child window. Call
  // this directly if you want to do custom filtering on plugin windows first.
  static void DetachPluginWindowsCallback(HWND window);
#endif

 protected:
  // Interface class only, do not construct.
  RenderWidgetHostViewBase();

#if defined(OS_WIN)
  // Shared implementation of MovePluginWindows for use by win and aura/wina.
  static void MovePluginWindowsHelper(
      HWND parent,
      const std::vector<WebPluginGeometry>& moves);

  static void PaintPluginWindowsHelper(
      HWND parent,
      const gfx::Rect& damaged_screen_rect);

  // Needs to be called before the HWND backing the view goes away to avoid
  // crashes in Windowed plugins.
  static void DetachPluginsHelper(HWND parent);
#endif

  // Whether this view is a popup and what kind of popup it is (select,
  // autofill...).
  blink::WebPopupType popup_type_;

  // When false, the background of the web content is not fully opaque.
  bool background_opaque_;

  // While the mouse is locked, the cursor is hidden from the user. Mouse events
  // are still generated. However, the position they report is the last known
  // mouse position just as mouse lock was entered; the movement they report
  // indicates what the change in position of the mouse would be had it not been
  // locked.
  bool mouse_locked_;

  // Whether we are showing a context menu.
  bool showing_context_menu_;

  // A buffer containing the text inside and around the current selection range.
  base::string16 selection_text_;

  // The offset of the text stored in |selection_text_| relative to the start of
  // the web page.
  size_t selection_text_offset_;

  // The current selection range relative to the start of the web page.
  gfx::Range selection_range_;

protected:
  // The scale factor of the display the renderer is currently on.
  float current_device_scale_factor_;

  // The orientation of the display the renderer is currently on.
  gfx::Display::Rotation current_display_rotation_;

  // Whether pinch-to-zoom should be enabled and pinch events forwarded to the
  // renderer.
  bool pinch_zoom_enabled_;

 private:
  void FlushInput();

  // Manager of the tree representation of the WebKit render tree.
  scoped_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  gfx::Rect current_display_area_;

  uint32 renderer_frame_number_;

  base::OneShotTimer<RenderWidgetHostViewBase> flush_input_timer_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
