// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

#include "base/win/metro.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/win/shell.h"
#include "ui/compositor/compositor_constants.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/path.h"
#include "ui/gfx/path_win.h"
#include "ui/gfx/vector2d.h"
#include "ui/gfx/win/dpi.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/native_theme/native_theme_win.h"
#include "ui/views/corewm/tooltip_win.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"
#include "ui/views/widget/desktop_aura/desktop_drag_drop_client_win.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_hwnd_utils.h"
#include "ui/views/win/fullscreen_handler.h"
#include "ui/views/win/hwnd_message_handler.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/input_method_event_filter.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/scoped_tooltip_disabler.h"

namespace views {

namespace {

gfx::Size GetExpandedWindowSize(DWORD window_style, gfx::Size size) {
  if (!(window_style & WS_EX_COMPOSITED) || !ui::win::IsAeroGlassEnabled())
    return size;

  // Some AMD drivers can't display windows that are less than 64x64 pixels,
  // so expand them to be at least that size. http://crbug.com/286609
  gfx::Size expanded(std::max(size.width(), 64), std::max(size.height(), 64));
  return expanded;
}

void InsetBottomRight(gfx::Rect* rect, gfx::Vector2d vector) {
  rect->Inset(0, 0, vector.x(), vector.y());
}

}  // namespace

DEFINE_WINDOW_PROPERTY_KEY(aura::Window*, kContentWindowForRootWindow, NULL);

// Identifies the DesktopWindowTreeHostWin associated with the
// WindowEventDispatcher.
DEFINE_WINDOW_PROPERTY_KEY(DesktopWindowTreeHostWin*, kDesktopWindowTreeHostKey,
                           NULL);

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, public:

bool DesktopWindowTreeHostWin::is_cursor_visible_ = true;

DesktopWindowTreeHostWin::DesktopWindowTreeHostWin(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura)
    : message_handler_(new HWNDMessageHandler(this)),
      native_widget_delegate_(native_widget_delegate),
      desktop_native_widget_aura_(desktop_native_widget_aura),
      content_window_(NULL),
      drag_drop_client_(NULL),
      should_animate_window_close_(false),
      pending_close_(false),
      has_non_client_view_(false),
      tooltip_(NULL) {
}

DesktopWindowTreeHostWin::~DesktopWindowTreeHostWin() {
  // WARNING: |content_window_| has been destroyed by the time we get here.
  desktop_native_widget_aura_->OnDesktopWindowTreeHostDestroyed(this);
  DestroyDispatcher();
}

// static
aura::Window* DesktopWindowTreeHostWin::GetContentWindowForHWND(HWND hwnd) {
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(hwnd);
  return host ? host->window()->GetProperty(kContentWindowForRootWindow) : NULL;
}

// static
ui::NativeTheme* DesktopWindowTreeHost::GetNativeTheme(aura::Window* window) {
  // Use NativeThemeWin for windows shown on the desktop, those not on the
  // desktop come from Ash and get NativeThemeAura.
  aura::WindowTreeHost* host = window ? window->GetHost() : NULL;
  if (host) {
    HWND host_hwnd = host->GetAcceleratedWidget();
    if (host_hwnd &&
        DesktopWindowTreeHostWin::GetContentWindowForHWND(host_hwnd)) {
      return ui::NativeThemeWin::instance();
    }
  }
  return ui::NativeThemeAura::instance();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, DesktopWindowTreeHost implementation:

void DesktopWindowTreeHostWin::Init(aura::Window* content_window,
                                    const Widget::InitParams& params) {
  // TODO(beng): SetInitParams().
  content_window_ = content_window;

  aura::client::SetAnimationHost(content_window_, this);

  ConfigureWindowStyles(message_handler_.get(), params,
                        GetWidget()->widget_delegate(),
                        native_widget_delegate_);

  HWND parent_hwnd = NULL;
  if (params.parent && params.parent->GetHost())
    parent_hwnd = params.parent->GetHost()->GetAcceleratedWidget();

  message_handler_->set_remove_standard_frame(params.remove_standard_frame);

  has_non_client_view_ = Widget::RequiresNonClientView(params.type);

  gfx::Rect pixel_bounds = gfx::win::DIPToScreenRect(params.bounds);
  message_handler_->Init(parent_hwnd, pixel_bounds);
  if (params.type == Widget::InitParams::TYPE_MENU) {
    ::SetProp(GetAcceleratedWidget(),
              kForceSoftwareCompositor,
              reinterpret_cast<HANDLE>(true));
  }
  CreateCompositor(GetAcceleratedWidget());
}

void DesktopWindowTreeHostWin::OnNativeWidgetCreated(
    const Widget::InitParams& params) {
  // The cursor is not necessarily visible when the root window is created.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(window());
  if (cursor_client)
    is_cursor_visible_ = cursor_client->IsCursorVisible();

  window()->SetProperty(kContentWindowForRootWindow, content_window_);
  window()->SetProperty(kDesktopWindowTreeHostKey, this);

  should_animate_window_close_ =
      content_window_->type() != ui::wm::WINDOW_TYPE_NORMAL &&
      !wm::WindowAnimationsDisabled(content_window_);

// TODO this is not invoked *after* Init(), but should be ok.
  SetWindowTransparency();
}

scoped_ptr<corewm::Tooltip> DesktopWindowTreeHostWin::CreateTooltip() {
  DCHECK(!tooltip_);
  tooltip_ = new corewm::TooltipWin(GetAcceleratedWidget());
  return scoped_ptr<corewm::Tooltip>(tooltip_);
}

scoped_ptr<aura::client::DragDropClient>
DesktopWindowTreeHostWin::CreateDragDropClient(
    DesktopNativeCursorManager* cursor_manager) {
  drag_drop_client_ = new DesktopDragDropClientWin(window(), GetHWND());
  return scoped_ptr<aura::client::DragDropClient>(drag_drop_client_).Pass();
}

void DesktopWindowTreeHostWin::Close() {
  // TODO(beng): Move this entire branch to DNWA so it can be shared with X11.
  if (should_animate_window_close_) {
    pending_close_ = true;
    const bool is_animating =
        content_window_->layer()->GetAnimator()->IsAnimatingProperty(
            ui::LayerAnimationElement::VISIBILITY);
    // Animation may not start for a number of reasons.
    if (!is_animating)
      message_handler_->Close();
    // else case, OnWindowHidingAnimationCompleted does the actual Close.
  } else {
    message_handler_->Close();
  }
}

void DesktopWindowTreeHostWin::CloseNow() {
  message_handler_->CloseNow();
}

aura::WindowTreeHost* DesktopWindowTreeHostWin::AsWindowTreeHost() {
  return this;
}

void DesktopWindowTreeHostWin::ShowWindowWithState(
    ui::WindowShowState show_state) {
  message_handler_->ShowWindowWithState(show_state);
}

void DesktopWindowTreeHostWin::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  gfx::Rect pixel_bounds = gfx::win::DIPToScreenRect(restored_bounds);
  message_handler_->ShowMaximizedWithBounds(pixel_bounds);
}

bool DesktopWindowTreeHostWin::IsVisible() const {
  return message_handler_->IsVisible();
}

void DesktopWindowTreeHostWin::SetSize(const gfx::Size& size) {
  gfx::Size size_in_pixels = gfx::win::DIPToScreenSize(size);
  gfx::Size expanded = GetExpandedWindowSize(
      message_handler_->window_ex_style(), size_in_pixels);
  window_enlargement_ =
      gfx::Vector2d(expanded.width() - size_in_pixels.width(),
                    expanded.height() - size_in_pixels.height());
  message_handler_->SetSize(expanded);
}

void DesktopWindowTreeHostWin::StackAtTop() {
  message_handler_->StackAtTop();
}

void DesktopWindowTreeHostWin::CenterWindow(const gfx::Size& size) {
  gfx::Size size_in_pixels = gfx::win::DIPToScreenSize(size);
  gfx::Size expanded_size;
  expanded_size = GetExpandedWindowSize(message_handler_->window_ex_style(),
                                        size_in_pixels);
  window_enlargement_ =
      gfx::Vector2d(expanded_size.width() - size_in_pixels.width(),
                    expanded_size.height() - size_in_pixels.height());
  message_handler_->CenterWindow(expanded_size);
}

void DesktopWindowTreeHostWin::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  message_handler_->GetWindowPlacement(bounds, show_state);
  InsetBottomRight(bounds, window_enlargement_);
  *bounds = gfx::win::ScreenToDIPRect(*bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetWindowBoundsInScreen() const {
  gfx::Rect pixel_bounds = message_handler_->GetWindowBoundsInScreen();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetClientAreaBoundsInScreen() const {
  gfx::Rect pixel_bounds = message_handler_->GetClientAreaBoundsInScreen();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetRestoredBounds() const {
  gfx::Rect pixel_bounds = message_handler_->GetRestoredBounds();
  InsetBottomRight(&pixel_bounds, window_enlargement_);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

gfx::Rect DesktopWindowTreeHostWin::GetWorkAreaBoundsInScreen() const {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(message_handler_->hwnd(),
                                   MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  gfx::Rect pixel_bounds = gfx::Rect(monitor_info.rcWork);
  return gfx::win::ScreenToDIPRect(pixel_bounds);
}

void DesktopWindowTreeHostWin::SetShape(gfx::NativeRegion native_region) {
  if (native_region) {
    message_handler_->SetRegion(gfx::CreateHRGNFromSkRegion(*native_region));
  } else {
    message_handler_->SetRegion(NULL);
  }

  delete native_region;
}

void DesktopWindowTreeHostWin::Activate() {
  message_handler_->Activate();
}

void DesktopWindowTreeHostWin::Deactivate() {
  message_handler_->Deactivate();
}

bool DesktopWindowTreeHostWin::IsActive() const {
  return message_handler_->IsActive();
}

void DesktopWindowTreeHostWin::Maximize() {
  message_handler_->Maximize();
}

void DesktopWindowTreeHostWin::Minimize() {
  message_handler_->Minimize();
}

void DesktopWindowTreeHostWin::Restore() {
  message_handler_->Restore();
}

bool DesktopWindowTreeHostWin::IsMaximized() const {
  return message_handler_->IsMaximized();
}

bool DesktopWindowTreeHostWin::IsMinimized() const {
  return message_handler_->IsMinimized();
}

bool DesktopWindowTreeHostWin::HasCapture() const {
  return message_handler_->HasCapture();
}

void DesktopWindowTreeHostWin::SetAlwaysOnTop(bool always_on_top) {
  message_handler_->SetAlwaysOnTop(always_on_top);
}

bool DesktopWindowTreeHostWin::IsAlwaysOnTop() const {
  return message_handler_->IsAlwaysOnTop();
}

void DesktopWindowTreeHostWin::SetVisibleOnAllWorkspaces(bool always_visible) {
  // Windows does not have the concept of workspaces.
}

bool DesktopWindowTreeHostWin::SetWindowTitle(const base::string16& title) {
  return message_handler_->SetTitle(title);
}

void DesktopWindowTreeHostWin::ClearNativeFocus() {
  message_handler_->ClearNativeFocus();
}

Widget::MoveLoopResult DesktopWindowTreeHostWin::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  const bool hide_on_escape =
      escape_behavior == Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_HIDE;
  return message_handler_->RunMoveLoop(drag_offset, hide_on_escape) ?
      Widget::MOVE_LOOP_SUCCESSFUL : Widget::MOVE_LOOP_CANCELED;
}

void DesktopWindowTreeHostWin::EndMoveLoop() {
  message_handler_->EndMoveLoop();
}

void DesktopWindowTreeHostWin::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  message_handler_->SetVisibilityChangedAnimationsEnabled(value);
  content_window_->SetProperty(aura::client::kAnimationsDisabledKey, !value);
}

bool DesktopWindowTreeHostWin::ShouldUseNativeFrame() const {
  return IsTranslucentWindowOpacitySupported();
}

bool DesktopWindowTreeHostWin::ShouldWindowContentsBeTransparent() const {
  // If the window has a native frame, we assume it is an Aero Glass window, and
  // is therefore transparent. Note: This is not equivalent to calling
  // IsAeroGlassEnabled, because ShouldUseNativeFrame is overridden in a
  // subclass.
  return ShouldUseNativeFrame();
}

void DesktopWindowTreeHostWin::FrameTypeChanged() {
  message_handler_->FrameTypeChanged();
  SetWindowTransparency();
}

void DesktopWindowTreeHostWin::SetFullscreen(bool fullscreen) {
  message_handler_->fullscreen_handler()->SetFullscreen(fullscreen);
  // TODO(sky): workaround for ScopedFullscreenVisibility showing window
  // directly. Instead of this should listen for visibility changes and then
  // update window.
  if (message_handler_->IsVisible() && !content_window_->TargetVisibility())
    content_window_->Show();
  SetWindowTransparency();
}

bool DesktopWindowTreeHostWin::IsFullscreen() const {
  return message_handler_->fullscreen_handler()->fullscreen();
}

void DesktopWindowTreeHostWin::SetOpacity(unsigned char opacity) {
  message_handler_->SetOpacity(static_cast<BYTE>(opacity));
  content_window_->layer()->SetOpacity(opacity / 255.0);
}

void DesktopWindowTreeHostWin::SetWindowIcons(
    const gfx::ImageSkia& window_icon, const gfx::ImageSkia& app_icon) {
  message_handler_->SetWindowIcons(window_icon, app_icon);
}

void DesktopWindowTreeHostWin::InitModalType(ui::ModalType modal_type) {
  message_handler_->InitModalType(modal_type);
}

void DesktopWindowTreeHostWin::FlashFrame(bool flash_frame) {
  message_handler_->FlashFrame(flash_frame);
}

void DesktopWindowTreeHostWin::OnRootViewLayout() const {
}

void DesktopWindowTreeHostWin::OnNativeWidgetFocus() {
  // HWNDMessageHandler will perform the proper updating on its own.
}

void DesktopWindowTreeHostWin::OnNativeWidgetBlur() {
}

bool DesktopWindowTreeHostWin::IsAnimatingClosed() const {
  return pending_close_;
}

bool DesktopWindowTreeHostWin::IsTranslucentWindowOpacitySupported() const {
  return ui::win::IsAeroGlassEnabled();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, WindowTreeHost implementation:

ui::EventSource* DesktopWindowTreeHostWin::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget DesktopWindowTreeHostWin::GetAcceleratedWidget() {
  return message_handler_->hwnd();
}

void DesktopWindowTreeHostWin::Show() {
  message_handler_->Show();
}

void DesktopWindowTreeHostWin::Hide() {
  if (!pending_close_)
    message_handler_->Hide();
}

// GetBounds and SetBounds work in pixel coordinates, whereas other get/set
// methods work in DIP.

gfx::Rect DesktopWindowTreeHostWin::GetBounds() const {
  gfx::Rect bounds(message_handler_->GetClientAreaBounds());
  // If the window bounds were expanded we need to return the original bounds
  // To achieve this we do the reverse of the expansion, i.e. add the
  // window_expansion_top_left_delta_ to the origin and subtract the
  // window_expansion_bottom_right_delta_ from the width and height.
  gfx::Rect without_expansion(
      bounds.x() + window_expansion_top_left_delta_.x(),
      bounds.y() + window_expansion_top_left_delta_.y(),
      bounds.width() - window_expansion_bottom_right_delta_.x() -
          window_enlargement_.x(),
      bounds.height() - window_expansion_bottom_right_delta_.y() -
          window_enlargement_.y());
  return without_expansion;
}

void DesktopWindowTreeHostWin::SetBounds(const gfx::Rect& bounds) {
  // If the window bounds have to be expanded we need to subtract the
  // window_expansion_top_left_delta_ from the origin and add the
  // window_expansion_bottom_right_delta_ to the width and height
  gfx::Size old_hwnd_size(message_handler_->GetClientAreaBounds().size());
  gfx::Size old_content_size = GetBounds().size();

  gfx::Rect expanded(
      bounds.x() - window_expansion_top_left_delta_.x(),
      bounds.y() - window_expansion_top_left_delta_.y(),
      bounds.width() + window_expansion_bottom_right_delta_.x(),
      bounds.height() + window_expansion_bottom_right_delta_.y());

  gfx::Rect new_expanded(
      expanded.origin(),
      GetExpandedWindowSize(message_handler_->window_ex_style(),
                            expanded.size()));
  window_enlargement_ =
      gfx::Vector2d(new_expanded.width() - expanded.width(),
                    new_expanded.height() - expanded.height());
  message_handler_->SetBounds(new_expanded, old_content_size != bounds.size());
}

gfx::Point DesktopWindowTreeHostWin::GetLocationOnNativeScreen() const {
  return GetBounds().origin();
}

void DesktopWindowTreeHostWin::SetCapture() {
  message_handler_->SetCapture();
}

void DesktopWindowTreeHostWin::ReleaseCapture() {
  message_handler_->ReleaseCapture();
}

void DesktopWindowTreeHostWin::PostNativeEvent(
    const base::NativeEvent& native_event) {
}

void DesktopWindowTreeHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopWindowTreeHostWin::SetCursorNative(gfx::NativeCursor cursor) {
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);

  message_handler_->SetCursor(cursor.platform());
}

void DesktopWindowTreeHostWin::OnCursorVisibilityChangedNative(bool show) {
  if (is_cursor_visible_ == show)
    return;
  is_cursor_visible_ = show;
  ::ShowCursor(!!show);
}

void DesktopWindowTreeHostWin::MoveCursorToNative(const gfx::Point& location) {
  POINT cursor_location = location.ToPOINT();
  ::ClientToScreen(GetHWND(), &cursor_location);
  ::SetCursorPos(cursor_location.x, cursor_location.y);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, ui::EventSource implementation:

ui::EventProcessor* DesktopWindowTreeHostWin::GetEventProcessor() {
  return dispatcher();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, aura::AnimationHost implementation:

void DesktopWindowTreeHostWin::SetHostTransitionOffsets(
    const gfx::Vector2d& top_left_delta,
    const gfx::Vector2d& bottom_right_delta) {
  gfx::Rect bounds_without_expansion = GetBounds();
  window_expansion_top_left_delta_ = top_left_delta;
  window_expansion_bottom_right_delta_ = bottom_right_delta;
  SetBounds(bounds_without_expansion);
}

void DesktopWindowTreeHostWin::OnWindowHidingAnimationCompleted() {
  if (pending_close_)
    message_handler_->Close();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, HWNDMessageHandlerDelegate implementation:

bool DesktopWindowTreeHostWin::IsWidgetWindow() const {
  return has_non_client_view_;
}

bool DesktopWindowTreeHostWin::IsUsingCustomFrame() const {
  return !GetWidget()->ShouldUseNativeFrame();
}

void DesktopWindowTreeHostWin::SchedulePaint() {
  GetWidget()->GetRootView()->SchedulePaint();
}

void DesktopWindowTreeHostWin::EnableInactiveRendering() {
  native_widget_delegate_->EnableInactiveRendering();
}

bool DesktopWindowTreeHostWin::IsInactiveRenderingDisabled() {
  return native_widget_delegate_->IsInactiveRenderingDisabled();
}

bool DesktopWindowTreeHostWin::CanResize() const {
  return GetWidget()->widget_delegate()->CanResize();
}

bool DesktopWindowTreeHostWin::CanMaximize() const {
  return GetWidget()->widget_delegate()->CanMaximize();
}

bool DesktopWindowTreeHostWin::CanActivate() const {
  if (IsModalWindowActive())
    return true;
  return native_widget_delegate_->CanActivate();
}

bool DesktopWindowTreeHostWin::WidgetSizeIsClientSize() const {
  const Widget* widget = GetWidget()->GetTopLevelWidget();
  return IsMaximized() || (widget && widget->ShouldUseNativeFrame());
}

bool DesktopWindowTreeHostWin::IsModal() const {
  return native_widget_delegate_->IsModal();
}

int DesktopWindowTreeHostWin::GetInitialShowState() const {
  return CanActivate() ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE;
}

bool DesktopWindowTreeHostWin::WillProcessWorkAreaChange() const {
  return GetWidget()->widget_delegate()->WillProcessWorkAreaChange();
}

int DesktopWindowTreeHostWin::GetNonClientComponent(
    const gfx::Point& point) const {
  gfx::Point dip_position = gfx::win::ScreenToDIPPoint(point);
  return native_widget_delegate_->GetNonClientComponent(dip_position);
}

void DesktopWindowTreeHostWin::GetWindowMask(const gfx::Size& size,
                                             gfx::Path* path) {
  if (GetWidget()->non_client_view()) {
    GetWidget()->non_client_view()->GetWindowMask(size, path);
  } else if (!window_enlargement_.IsZero()) {
    gfx::Rect bounds(WidgetSizeIsClientSize()
                         ? message_handler_->GetClientAreaBoundsInScreen()
                         : message_handler_->GetWindowBoundsInScreen());
    InsetBottomRight(&bounds, window_enlargement_);
    path->addRect(SkRect::MakeXYWH(0, 0, bounds.width(), bounds.height()));
  }
}

bool DesktopWindowTreeHostWin::GetClientAreaInsets(gfx::Insets* insets) const {
  return false;
}

void DesktopWindowTreeHostWin::GetMinMaxSize(gfx::Size* min_size,
                                             gfx::Size* max_size) const {
  *min_size = native_widget_delegate_->GetMinimumSize();
  *max_size = native_widget_delegate_->GetMaximumSize();
}

gfx::Size DesktopWindowTreeHostWin::GetRootViewSize() const {
  return GetWidget()->GetRootView()->size();
}

void DesktopWindowTreeHostWin::ResetWindowControls() {
  GetWidget()->non_client_view()->ResetWindowControls();
}

void DesktopWindowTreeHostWin::PaintLayeredWindow(gfx::Canvas* canvas) {
  GetWidget()->GetRootView()->Paint(canvas, views::CullSet());
}

gfx::NativeViewAccessible DesktopWindowTreeHostWin::GetNativeViewAccessible() {
  return GetWidget()->GetRootView()->GetNativeViewAccessible();
}

InputMethod* DesktopWindowTreeHostWin::GetInputMethod() {
  return GetWidget()->GetInputMethodDirect();
}

bool DesktopWindowTreeHostWin::ShouldHandleSystemCommands() const {
  return GetWidget()->widget_delegate()->ShouldHandleSystemCommands();
}

void DesktopWindowTreeHostWin::HandleAppDeactivated() {
  native_widget_delegate_->EnableInactiveRendering();
}

void DesktopWindowTreeHostWin::HandleActivationChanged(bool active) {
  // This can be invoked from HWNDMessageHandler::Init(), at which point we're
  // not in a good state and need to ignore it.
  // TODO(beng): Do we need this still now the host owns the dispatcher?
  if (!dispatcher())
    return;

  if (active)
    OnHostActivated();
  desktop_native_widget_aura_->HandleActivationChanged(active);
}

bool DesktopWindowTreeHostWin::HandleAppCommand(short command) {
  // We treat APPCOMMAND ids as an extension of our command namespace, and just
  // let the delegate figure out what to do...
  return GetWidget()->widget_delegate() &&
      GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void DesktopWindowTreeHostWin::HandleCancelMode() {
  dispatcher()->DispatchCancelModeEvent();
}

void DesktopWindowTreeHostWin::HandleCaptureLost() {
  OnHostLostWindowCapture();
  native_widget_delegate_->OnMouseCaptureLost();
}

void DesktopWindowTreeHostWin::HandleClose() {
  GetWidget()->Close();
}

bool DesktopWindowTreeHostWin::HandleCommand(int command) {
  return GetWidget()->widget_delegate()->ExecuteWindowsCommand(command);
}

void DesktopWindowTreeHostWin::HandleAccelerator(
    const ui::Accelerator& accelerator) {
  GetWidget()->GetFocusManager()->ProcessAccelerator(accelerator);
}

void DesktopWindowTreeHostWin::HandleCreate() {
  native_widget_delegate_->OnNativeWidgetCreated(true);
}

void DesktopWindowTreeHostWin::HandleDestroying() {
  drag_drop_client_->OnNativeWidgetDestroying(GetHWND());
  native_widget_delegate_->OnNativeWidgetDestroying();

  // Destroy the compositor before destroying the HWND since shutdown
  // may try to swap to the window.
  DestroyCompositor();
}

void DesktopWindowTreeHostWin::HandleDestroyed() {
  desktop_native_widget_aura_->OnHostClosed();
}

bool DesktopWindowTreeHostWin::HandleInitialFocus(
    ui::WindowShowState show_state) {
  return GetWidget()->SetInitialFocus(show_state);
}

void DesktopWindowTreeHostWin::HandleDisplayChange() {
  GetWidget()->widget_delegate()->OnDisplayChanged();
}

void DesktopWindowTreeHostWin::HandleBeginWMSizeMove() {
  native_widget_delegate_->OnNativeWidgetBeginUserBoundsChange();
}

void DesktopWindowTreeHostWin::HandleEndWMSizeMove() {
  native_widget_delegate_->OnNativeWidgetEndUserBoundsChange();
}

void DesktopWindowTreeHostWin::HandleMove() {
  native_widget_delegate_->OnNativeWidgetMove();
  OnHostMoved(GetBounds().origin());
}

void DesktopWindowTreeHostWin::HandleWorkAreaChanged() {
  GetWidget()->widget_delegate()->OnWorkAreaChanged();
}

void DesktopWindowTreeHostWin::HandleVisibilityChanging(bool visible) {
  native_widget_delegate_->OnNativeWidgetVisibilityChanging(visible);
}

void DesktopWindowTreeHostWin::HandleVisibilityChanged(bool visible) {
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void DesktopWindowTreeHostWin::HandleClientSizeChanged(
    const gfx::Size& new_size) {
  if (dispatcher())
    OnHostResized(new_size);
}

void DesktopWindowTreeHostWin::HandleFrameChanged() {
  SetWindowTransparency();
  // Replace the frame and layout the contents.
  GetWidget()->non_client_view()->UpdateFrame();
}

void DesktopWindowTreeHostWin::HandleNativeFocus(HWND last_focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnFocus();
}

void DesktopWindowTreeHostWin::HandleNativeBlur(HWND focused_window) {
  // TODO(beng): inform the native_widget_delegate_.
  InputMethod* input_method = GetInputMethod();
  if (input_method)
    input_method->OnBlur();
}

bool DesktopWindowTreeHostWin::HandleMouseEvent(const ui::MouseEvent& event) {
  SendEventToProcessor(const_cast<ui::MouseEvent*>(&event));
  return event.handled();
}

bool DesktopWindowTreeHostWin::HandleKeyEvent(const ui::KeyEvent& event) {
  return false;
}

bool DesktopWindowTreeHostWin::HandleUntranslatedKeyEvent(
    const ui::KeyEvent& event) {
  ui::KeyEvent duplicate_event(event);
  SendEventToProcessor(&duplicate_event);
  return duplicate_event.handled();
}

void DesktopWindowTreeHostWin::HandleTouchEvent(
    const ui::TouchEvent& event) {
  // HWNDMessageHandler asynchronously processes touch events. Because of this
  // it's possible for the aura::WindowEventDispatcher to have been destroyed
  // by the time we attempt to process them.
  if (!GetWidget()->GetNativeView())
    return;

  // Currently we assume the window that has capture gets touch events too.
  aura::WindowTreeHost* host =
      aura::WindowTreeHost::GetForAcceleratedWidget(GetCapture());
  if (host) {
    DesktopWindowTreeHostWin* target =
        host->window()->GetProperty(kDesktopWindowTreeHostKey);
    if (target && target->HasCapture() && target != this) {
      POINT target_location(event.location().ToPOINT());
      ClientToScreen(GetHWND(), &target_location);
      ScreenToClient(target->GetHWND(), &target_location);
      ui::TouchEvent target_event(event, static_cast<View*>(NULL),
                                  static_cast<View*>(NULL));
      target_event.set_location(gfx::Point(target_location));
      target_event.set_root_location(target_event.location());
      target->SendEventToProcessor(&target_event);
      return;
    }
  }
  SendEventToProcessor(const_cast<ui::TouchEvent*>(&event));
}

bool DesktopWindowTreeHostWin::HandleIMEMessage(UINT message,
                                                WPARAM w_param,
                                                LPARAM l_param,
                                                LRESULT* result) {
  MSG msg = {};
  msg.hwnd = GetHWND();
  msg.message = message;
  msg.wParam = w_param;
  msg.lParam = l_param;
  return desktop_native_widget_aura_->input_method_event_filter()->
      input_method()->OnUntranslatedIMEMessage(msg, result);
}

void DesktopWindowTreeHostWin::HandleInputLanguageChange(
    DWORD character_set,
    HKL input_language_id) {
  desktop_native_widget_aura_->input_method_event_filter()->
      input_method()->OnInputLocaleChanged();
}

bool DesktopWindowTreeHostWin::HandlePaintAccelerated(
    const gfx::Rect& invalid_rect) {
  return native_widget_delegate_->OnNativeWidgetPaintAccelerated(invalid_rect);
}

void DesktopWindowTreeHostWin::HandlePaint(gfx::Canvas* canvas) {
  // It appears possible to get WM_PAINT after WM_DESTROY.
  if (compositor())
    compositor()->ScheduleRedrawRect(gfx::Rect());
}

bool DesktopWindowTreeHostWin::HandleTooltipNotify(int w_param,
                                                   NMHDR* l_param,
                                                   LRESULT* l_result) {
  return tooltip_ && tooltip_->HandleNotify(w_param, l_param, l_result);
}

void DesktopWindowTreeHostWin::HandleTooltipMouseMove(UINT message,
                                                      WPARAM w_param,
                                                      LPARAM l_param) {
  // TooltipWin implementation doesn't need this.
  // TODO(sky): remove from HWNDMessageHandler once non-aura path nuked.
}

void DesktopWindowTreeHostWin::HandleMenuLoop(bool in_menu_loop) {
  if (in_menu_loop) {
    tooltip_disabler_.reset(
        new aura::client::ScopedTooltipDisabler(window()));
  } else {
    tooltip_disabler_.reset();
  }
}

bool DesktopWindowTreeHostWin::PreHandleMSG(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param,
                                            LRESULT* result) {
  return false;
}

void DesktopWindowTreeHostWin::PostHandleMSG(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
}

bool DesktopWindowTreeHostWin::HandleScrollEvent(
    const ui::ScrollEvent& event) {
  SendEventToProcessor(const_cast<ui::ScrollEvent*>(&event));
  return event.handled();
}

void DesktopWindowTreeHostWin::HandleWindowSizeChanging() {
  if (compositor())
    compositor()->FinishAllRendering();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostWin, private:

Widget* DesktopWindowTreeHostWin::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopWindowTreeHostWin::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

HWND DesktopWindowTreeHostWin::GetHWND() const {
  return message_handler_->hwnd();
}

void DesktopWindowTreeHostWin::SetWindowTransparency() {
  bool transparent = ShouldUseNativeFrame() && !IsFullscreen();
  compositor()->SetHostHasTransparentBackground(transparent);
  window()->SetTransparent(transparent);
  content_window_->SetTransparent(transparent);
}

bool DesktopWindowTreeHostWin::IsModalWindowActive() const {
  // This function can get called during window creation which occurs before
  // dispatcher() has been created.
  if (!dispatcher())
    return false;

  aura::Window::Windows::const_iterator index;
  for (index = window()->children().begin();
       index != window()->children().end();
       ++index) {
    if ((*index)->GetProperty(aura::client::kModalKey) !=
        ui:: MODAL_TYPE_NONE && (*index)->TargetVisibility())
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHost, public:

// static
DesktopWindowTreeHost* DesktopWindowTreeHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura) {
  return new DesktopWindowTreeHostWin(native_widget_delegate,
                                      desktop_native_widget_aura);
}

}  // namespace views
