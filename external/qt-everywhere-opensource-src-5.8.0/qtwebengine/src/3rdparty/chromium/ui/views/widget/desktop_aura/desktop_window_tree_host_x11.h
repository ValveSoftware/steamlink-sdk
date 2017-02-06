// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_

#include <stddef.h>
#include <stdint.h>
#include <X11/extensions/shape.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor_loader_x11.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

namespace gfx {
class ImageSkia;
class ImageSkiaRep;
}

namespace ui {
class EventHandler;
}

namespace views {
class DesktopDragDropClientAuraX11;
class DesktopWindowTreeHostObserverX11;
class X11DesktopWindowMoveClient;
class X11WindowEventFilter;

class VIEWS_EXPORT DesktopWindowTreeHostX11
    : public DesktopWindowTreeHost,
      public aura::WindowTreeHost,
      public ui::PlatformEventDispatcher {
 public:
  DesktopWindowTreeHostX11(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~DesktopWindowTreeHostX11() override;

  // A way of converting an X11 |xid| host window into a |content_window_|.
  static aura::Window* GetContentWindowForXID(XID xid);

  // A way of converting an X11 |xid| host window into this object.
  static DesktopWindowTreeHostX11* GetHostForXID(XID xid);

  // Get all open top-level windows. This includes windows that may not be
  // visible. This list is sorted in their stacking order, i.e. the first window
  // is the topmost window.
  static std::vector<aura::Window*> GetAllOpenWindows();

  // Returns the current bounds in terms of the X11 Root Window.
  gfx::Rect GetX11RootWindowBounds() const;

  // Returns the current bounds in terms of the X11 Root Window including the
  // borders provided by the window manager (if any).
  gfx::Rect GetX11RootWindowOuterBounds() const;

  // Returns the window shape if the window is not rectangular. Returns NULL
  // otherwise.
  ::Region GetWindowShape() const;

  // Called by X11DesktopHandler to notify us that the native windowing system
  // has changed our activation.
  void HandleNativeWidgetActivationChanged(bool active);

  void AddObserver(views::DesktopWindowTreeHostObserverX11* observer);
  void RemoveObserver(views::DesktopWindowTreeHostObserverX11* observer);

  // Swaps the current handler for events in the non client view with |handler|.
  void SwapNonClientEventHandler(std::unique_ptr<ui::EventHandler> handler);

  // Runs the |func| callback for each content-window, and deallocates the
  // internal list of open windows.
  static void CleanUpWindowList(void (*func)(aura::Window* window));

 protected:
  // Overridden from DesktopWindowTreeHost:
  void Init(aura::Window* content_window,
            const Widget::InitParams& params) override;
  void OnNativeWidgetCreated(const Widget::InitParams& params) override;
  std::unique_ptr<corewm::Tooltip> CreateTooltip() override;
  std::unique_ptr<aura::client::DragDropClient> CreateDragDropClient(
      DesktopNativeCursorManager* cursor_manager) override;
  void Close() override;
  void CloseNow() override;
  aura::WindowTreeHost* AsWindowTreeHost() override;
  void ShowWindowWithState(ui::WindowShowState show_state) override;
  void ShowMaximizedWithBounds(const gfx::Rect& restored_bounds) override;
  bool IsVisible() const override;
  void SetSize(const gfx::Size& requested_size) override;
  void StackAbove(aura::Window* window) override;
  void StackAtTop() override;
  void CenterWindow(const gfx::Size& size) override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  gfx::Rect GetWindowBoundsInScreen() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  gfx::Rect GetRestoredBounds() const override;
  std::string GetWorkspace() const override;
  gfx::Rect GetWorkAreaBoundsInScreen() const override;
  void SetShape(SkRegion* native_region) override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  bool IsMaximized() const override;
  bool IsMinimized() const override;
  bool HasCapture() const override;
  void SetAlwaysOnTop(bool always_on_top) override;
  bool IsAlwaysOnTop() const override;
  void SetVisibleOnAllWorkspaces(bool always_visible) override;
  bool SetWindowTitle(const base::string16& title) override;
  void ClearNativeFocus() override;
  Widget::MoveLoopResult RunMoveLoop(
      const gfx::Vector2d& drag_offset,
      Widget::MoveLoopSource source,
      Widget::MoveLoopEscapeBehavior escape_behavior) override;
  void EndMoveLoop() override;
  void SetVisibilityChangedAnimationsEnabled(bool value) override;
  bool ShouldUseNativeFrame() const override;
  bool ShouldWindowContentsBeTransparent() const override;
  void FrameTypeChanged() override;
  void SetFullscreen(bool fullscreen) override;
  bool IsFullscreen() const override;
  void SetOpacity(float opacity) override;
  void SetWindowIcons(const gfx::ImageSkia& window_icon,
                      const gfx::ImageSkia& app_icon) override;
  void InitModalType(ui::ModalType modal_type) override;
  void FlashFrame(bool flash_frame) override;
  void OnRootViewLayout() override;
  void OnNativeWidgetFocus() override;
  void OnNativeWidgetBlur() override;
  bool IsAnimatingClosed() const override;
  bool IsTranslucentWindowOpacitySupported() const override;
  void SizeConstraintsChanged() override;

  // Overridden from aura::WindowTreeHost:
  gfx::Transform GetRootTransform() const override;
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBounds() const override;
  void SetBounds(const gfx::Rect& requested_bounds_in_pixels) override;
  gfx::Point GetLocationOnNativeScreen() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor) override;
  void MoveCursorToNative(const gfx::Point& location) override;
  void OnCursorVisibilityChangedNative(bool show) override;

 private:
  friend class DesktopWindowTreeHostX11HighDPITest;
  // Initializes our X11 surface to draw on. This method performs all
  // initialization related to talking to the X11 server.
  void InitX11Window(const Widget::InitParams& params);

  // Creates an aura::WindowEventDispatcher to contain the |content_window|,
  // along with all aura client objects that direct behavior.
  aura::WindowEventDispatcher* InitDispatcher(const Widget::InitParams& params);

  // Adjusts |requested_size| to avoid the WM "feature" where setting the
  // window size to the monitor size causes the WM to set the EWMH for
  // fullscreen.
  gfx::Size AdjustSize(const gfx::Size& requested_size);

  // Called when |xwindow_|'s _NET_WM_STATE property is updated.
  void OnWMStateUpdated();

  // Called when |xwindow_|'s _NET_FRAME_EXTENTS property is updated.
  void OnFrameExtentsUpdated();

  // Updates |xwindow_|'s minimum and maximum size.
  void UpdateMinAndMaxSize();

  // Updates |xwindow_|'s _NET_WM_USER_TIME if |xwindow_| is active.
  void UpdateWMUserTime(const ui::PlatformEvent& event);

  // Sends a message to the x11 window manager, enabling or disabling the
  // states |state1| and |state2|.
  void SetWMSpecState(bool enabled, ::Atom state1, ::Atom state2);

  // Checks if the window manager has set a specific state.
  bool HasWMSpecProperty(const char* property) const;

  // Sets whether the window's borders are provided by the window manager.
  void SetUseNativeFrame(bool use_native_frame);

  // Dispatches a mouse event, taking mouse capture into account. If a
  // different host has capture, we translate the event to its coordinate space
  // and dispatch it to that host instead.
  void DispatchMouseEvent(ui::MouseEvent* event);

  // Dispatches a touch event, taking capture into account. If a different host
  // has capture, then touch-press events are translated to its coordinate space
  // and dispatched to that host instead.
  void DispatchTouchEvent(ui::TouchEvent* event);

  // Dispatches a key event.
  void DispatchKeyEvent(ui::KeyEvent* event);

  // Updates the location of |located_event| to be in |host|'s coordinate system
  // so that it can be dispatched to |host|.
  void ConvertEventToDifferentHost(ui::LocatedEvent* located_event,
                                   DesktopWindowTreeHostX11* host);

  // Resets the window region for the current widget bounds if necessary.
  void ResetWindowRegion();

  // Serializes an image to the format used by _NET_WM_ICON.
  void SerializeImageRepresentation(const gfx::ImageSkiaRep& rep,
                                    std::vector<unsigned long>* data);

  // See comment for variable open_windows_.
  static std::list<XID>& open_windows();

  // Map the window (shows it) taking into account the given |show_state|.
  void MapWindow(ui::WindowShowState show_state);

  void SetWindowTransparency();

  // Relayout the widget's client and non-client views.
  void Relayout();

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  void DelayedResize(const gfx::Size& size_in_pixels);

  gfx::Rect GetWorkAreaBoundsInPixels() const;
  gfx::Rect ToDIPRect(const gfx::Rect& rect_in_pixels) const;
  gfx::Rect ToPixelRect(const gfx::Rect& rect_in_dip) const;

  // X11 things
  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  ui::X11AtomCache atom_cache_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // Should we wait for an UnmapNotify before trying to remap the window?
  bool wait_for_unmap_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_in_pixels_;

  // Whenever the bounds are set, we keep the previous set of bounds around so
  // we can have a better chance of getting the real
  // |restored_bounds_in_pixels_|. Window managers tend to send a Configure
  // message with the maximized bounds, and then set the window maximized
  // property. (We don't rely on this for when we request that the window be
  // maximized, only when we detect that some other process has requested that
  // we become the maximized window.)
  gfx::Rect previous_bounds_in_pixels_;

  // The bounds of our window before we were maximized.
  gfx::Rect restored_bounds_in_pixels_;

  // |xwindow_|'s minimum size.
  gfx::Size min_size_in_pixels_;

  // |xwindow_|'s maximum size.
  gfx::Size max_size_in_pixels_;

  // The window manager state bits.
  std::set< ::Atom> window_properties_;

  // Whether |xwindow_| was requested to be fullscreen via SetFullscreen().
  bool is_fullscreen_;

  // True if the window should stay on top of most other windows.
  bool is_always_on_top_;

  // True if the window has title-bar / borders provided by the window manager.
  bool use_native_frame_;

  // True if a Maximize() call should be done after mapping the window.
  bool should_maximize_after_map_;

  // Whether we used an ARGB visual for our window.
  bool use_argb_visual_;

  DesktopDragDropClientAuraX11* drag_drop_client_;

  std::unique_ptr<ui::EventHandler> x11_non_client_event_filter_;
  std::unique_ptr<X11DesktopWindowMoveClient> x11_window_move_client_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  DesktopNativeWidgetAura* desktop_native_widget_aura_;

  aura::Window* content_window_;

  // We can optionally have a parent which can order us to close, or own
  // children who we're responsible for closing when we CloseNow().
  DesktopWindowTreeHostX11* window_parent_;
  std::set<DesktopWindowTreeHostX11*> window_children_;

  base::ObserverList<DesktopWindowTreeHostObserverX11> observer_list_;

  // The window shape if the window is non-rectangular.
  gfx::XScopedPtr<_XRegion, gfx::XObjectDeleter<_XRegion, int, XDestroyRegion>>
      window_shape_;

  // Whether |window_shape_| was set via SetShape().
  bool custom_window_shape_;

  // The size of the window manager provided borders (if any).
  gfx::Insets native_window_frame_borders_in_pixels_;

  // The current DesktopWindowTreeHostX11 which has capture. Set synchronously
  // when capture is requested via SetCapture().
  static DesktopWindowTreeHostX11* g_current_capture;

  // A list of all (top-level) windows that have been created but not yet
  // destroyed.
  static std::list<XID>* open_windows_;

  base::string16 window_title_;

  // Whether we currently are flashing our frame. This feature is implemented
  // by setting the urgency hint with the window manager, which can draw
  // attention to the window or completely ignore the hint. We stop flashing
  // the frame when |xwindow_| gains focus or handles a mouse button event.
  bool urgency_hint_set_;

  bool activatable_;

  base::CancelableCallback<void()> delayed_resize_task_;

  base::WeakPtrFactory<DesktopWindowTreeHostX11> close_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopWindowTreeHostX11);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_WINDOW_TREE_HOST_X11_H_
