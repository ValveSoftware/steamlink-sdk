// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/bridged_native_widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, public:

NativeWidgetMac::NativeWidgetMac(internal::NativeWidgetDelegate* delegate)
    : delegate_(delegate), bridge_(new BridgedNativeWidget) {
}

NativeWidgetMac::~NativeWidgetMac() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMac, internal::NativeWidgetPrivate implementation:

void NativeWidgetMac::InitNativeWidget(const Widget::InitParams& params) {
  // TODO(tapted): Convert position into Cocoa's flipped coordinate space.
  NSRect content_rect =
      NSMakeRect(0, 0, params.bounds.width(), params.bounds.height());
  // TODO(tapted): Determine a good initial style mask from |params|.
  NSInteger style_mask = NSTitledWindowMask | NSClosableWindowMask |
                         NSMiniaturizableWindowMask | NSResizableWindowMask;
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:content_rect
                                  styleMask:style_mask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  bridge_->Init(window);
}

NonClientFrameView* NativeWidgetMac::CreateNonClientFrameView() {
  return NULL;
}

bool NativeWidgetMac::ShouldUseNativeFrame() const {
  return false;
}

bool NativeWidgetMac::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

Widget* NativeWidgetMac::GetWidget() {
  return delegate_->AsWidget();
}

const Widget* NativeWidgetMac::GetWidget() const {
  return delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMac::GetNativeView() const {
  return bridge_->ns_view();
}

gfx::NativeWindow NativeWidgetMac::GetNativeWindow() const {
  return bridge_->ns_window();
}

Widget* NativeWidgetMac::GetTopLevelWidget() {
  NOTIMPLEMENTED();
  return GetWidget();
}

const ui::Compositor* NativeWidgetMac::GetCompositor() const {
  NOTIMPLEMENTED();
  return NULL;
}

ui::Compositor* NativeWidgetMac::GetCompositor() {
  NOTIMPLEMENTED();
  return NULL;
}

ui::Layer* NativeWidgetMac::GetLayer() {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetMac::ReorderNativeViews() {
  bridge_->SetRootView(GetWidget()->GetRootView());
}

void NativeWidgetMac::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetNativeWindowProperty(const char* name, void* value) {
  NOTIMPLEMENTED();
}

void* NativeWidgetMac::GetNativeWindowProperty(const char* name) const {
  NOTIMPLEMENTED();
  return NULL;
}

TooltipManager* NativeWidgetMac::GetTooltipManager() const {
  NOTIMPLEMENTED();
  return NULL;
}

void NativeWidgetMac::SetCapture() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ReleaseCapture() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::HasCapture() const {
  NOTIMPLEMENTED();
  return false;
}

InputMethod* NativeWidgetMac::CreateInputMethod() {
  return bridge_->CreateInputMethod();
}

internal::InputMethodDelegate* NativeWidgetMac::GetInputMethodDelegate() {
  return bridge_.get();
}

ui::InputMethod* NativeWidgetMac::GetHostInputMethod() {
  return bridge_->GetHostInputMethod();
}

void NativeWidgetMac::CenterWindow(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::GetWindowPlacement(gfx::Rect* bounds,
                                         ui::WindowShowState* maximized) const {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::SetWindowTitle(const base::string16& title) {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::InitModalType(ui::ModalType modal_type) {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMac::GetWindowBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect NativeWidgetMac::GetClientAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect NativeWidgetMac::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void NativeWidgetMac::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetSize(const gfx::Size& size) {
  [bridge_->ns_window() setContentSize:NSMakeSize(size.width(), size.height())];
}

void NativeWidgetMac::StackAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::StackAtTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::StackBelow(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetShape(gfx::NativeRegion shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Close() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::CloseNow() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Show() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Hide() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::ShowWithWindowState(ui::WindowShowState state) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsVisible() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::Activate() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Deactivate() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsActive() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::SetAlwaysOnTop(bool always_on_top) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsAlwaysOnTop() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetVisibleOnAllWorkspaces(bool always_visible) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Maximize() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::Minimize() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsMaximized() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMac::IsMinimized() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::Restore() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetFullscreen(bool fullscreen) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeWidgetMac::SetOpacity(unsigned char opacity) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetUseDragFrame(bool use_drag_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   const gfx::Point& location,
                                   int operation,
                                   ui::DragDropTypes::DragEventSource source) {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SchedulePaintInRect(const gfx::Rect& rect) {
  // TODO(tapted): This should use setNeedsDisplayInRect:, once the coordinate
  // system of |rect| has been converted.
  [bridge_->ns_view() setNeedsDisplay:YES];
}

void NativeWidgetMac::SetCursor(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsMouseEventsEnabled() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMac::ClearNativeFocus() {
  NOTIMPLEMENTED();
}

gfx::Rect NativeWidgetMac::GetWorkAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMac::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  NOTIMPLEMENTED();
  return Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMac::EndMoveLoop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMac::SetVisibilityChangedAnimationsEnabled(bool value) {
  NOTIMPLEMENTED();
}

ui::NativeTheme* NativeWidgetMac::GetNativeTheme() const {
  return ui::NativeTheme::instance();
}

void NativeWidgetMac::OnRootViewLayout() const {
  NOTIMPLEMENTED();
}

bool NativeWidgetMac::IsTranslucentWindowOpacitySupported() const {
  return false;
}

void NativeWidgetMac::RepostNativeEvent(gfx::NativeEvent native_event) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

bool Widget::ConvertRect(const Widget* source,
                         const Widget* target,
                         gfx::Rect* rect) {
  return false;
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, public:

// static
NativeWidgetPrivate* NativeWidgetPrivate::CreateNativeWidget(
    internal::NativeWidgetDelegate* delegate) {
  return new NativeWidgetMac(delegate);
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeView(
    gfx::NativeView native_view) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
    gfx::NativeWindow native_window) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
NativeWidgetPrivate* NativeWidgetPrivate::GetTopLevelNativeWidget(
    gfx::NativeView native_view) {
  NOTIMPLEMENTED();
  return NULL;
}

// static
void NativeWidgetPrivate::GetAllChildWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* children) {
  NOTIMPLEMENTED();
}

// static
void NativeWidgetPrivate::GetAllOwnedWidgets(gfx::NativeView native_view,
                                             Widget::Widgets* owned) {
  NOTIMPLEMENTED();
}

// static
void NativeWidgetPrivate::ReparentNativeView(gfx::NativeView native_view,
                                             gfx::NativeView new_parent) {
  NOTIMPLEMENTED();
}

// static
bool NativeWidgetPrivate::IsMouseButtonDown() {
  NOTIMPLEMENTED();
  return false;
}

// static
bool NativeWidgetPrivate::IsTouchDown() {
  NOTIMPLEMENTED();
  return false;
}

// static
gfx::FontList NativeWidgetPrivate::GetWindowTitleFontList() {
  NOTIMPLEMENTED();
  return gfx::FontList();
}

}  // namespace internal
}  // namespace views
