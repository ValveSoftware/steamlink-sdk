// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_font_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_manager_factory.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_deletion_observer.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/widget/widget_removals_observer.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {

namespace {

// If |view| has a layer the layer is added to |layers|. Else this recurses
// through the children. This is used to build a list of the layers created by
// views that are direct children of the Widgets layer.
void BuildRootLayers(View* view, std::vector<ui::Layer*>* layers) {
  if (view->layer()) {
    layers->push_back(view->layer());
  } else {
    for (int i = 0; i < view->child_count(); ++i)
      BuildRootLayers(view->child_at(i), layers);
  }
}

// Create a native widget implementation.
// First, use the supplied one if non-NULL.
// Finally, make a default one.
NativeWidget* CreateNativeWidget(NativeWidget* native_widget,
                                 internal::NativeWidgetDelegate* delegate) {
  if (!native_widget) {
    native_widget =
        internal::NativeWidgetPrivate::CreateNativeWidget(delegate);
  }
  return native_widget;
}

}  // namespace

// A default implementation of WidgetDelegate, used by Widget when no
// WidgetDelegate is supplied.
class DefaultWidgetDelegate : public WidgetDelegate {
 public:
  explicit DefaultWidgetDelegate(Widget* widget) : widget_(widget) {
  }
  virtual ~DefaultWidgetDelegate() {}

  // Overridden from WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual Widget* GetWidget() OVERRIDE {
    return widget_;
  }
  virtual const Widget* GetWidget() const OVERRIDE {
    return widget_;
  }
  virtual bool ShouldAdvanceFocusToTopLevelWidget() const OVERRIDE {
    // In most situations where a Widget is used without a delegate the Widget
    // is used as a container, so that we want focus to advance to the top-level
    // widget. A good example of this is the find bar.
    return true;
  }

 private:
  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(DefaultWidgetDelegate);
};

////////////////////////////////////////////////////////////////////////////////
// Widget, InitParams:

Widget::InitParams::InitParams()
    : type(TYPE_WINDOW),
      delegate(NULL),
      child(false),
      opacity(INFER_OPACITY),
      accept_events(true),
      activatable(ACTIVATABLE_DEFAULT),
      keep_on_top(false),
      visible_on_all_workspaces(false),
      ownership(NATIVE_WIDGET_OWNS_WIDGET),
      mirror_origin_in_rtl(false),
      shadow_type(SHADOW_TYPE_DEFAULT),
      remove_standard_frame(false),
      use_system_default_icon(false),
      show_state(ui::SHOW_STATE_DEFAULT),
      double_buffer(false),
      parent(NULL),
      native_widget(NULL),
      desktop_window_tree_host(NULL),
      layer_type(aura::WINDOW_LAYER_TEXTURED),
      context(NULL),
      force_show_in_taskbar(false) {
}

Widget::InitParams::InitParams(Type type)
    : type(type),
      delegate(NULL),
      child(false),
      opacity(INFER_OPACITY),
      accept_events(true),
      activatable(ACTIVATABLE_DEFAULT),
      keep_on_top(type == TYPE_MENU || type == TYPE_DRAG),
      visible_on_all_workspaces(false),
      ownership(NATIVE_WIDGET_OWNS_WIDGET),
      mirror_origin_in_rtl(false),
      shadow_type(SHADOW_TYPE_DEFAULT),
      remove_standard_frame(false),
      use_system_default_icon(false),
      show_state(ui::SHOW_STATE_DEFAULT),
      double_buffer(false),
      parent(NULL),
      native_widget(NULL),
      desktop_window_tree_host(NULL),
      layer_type(aura::WINDOW_LAYER_TEXTURED),
      context(NULL),
      force_show_in_taskbar(false) {
}

Widget::InitParams::~InitParams() {
}

////////////////////////////////////////////////////////////////////////////////
// Widget, public:

Widget::Widget()
    : native_widget_(NULL),
      widget_delegate_(NULL),
      non_client_view_(NULL),
      dragged_view_(NULL),
      ownership_(InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      is_secondary_widget_(true),
      frame_type_(FRAME_TYPE_DEFAULT),
      disable_inactive_rendering_(false),
      widget_closed_(false),
      saved_show_state_(ui::SHOW_STATE_DEFAULT),
      focus_on_creation_(true),
      is_top_level_(false),
      native_widget_initialized_(false),
      native_widget_destroyed_(false),
      is_mouse_button_pressed_(false),
      is_touch_down_(false),
      last_mouse_event_was_move_(false),
      auto_release_capture_(true),
      root_layers_dirty_(false),
      movement_disabled_(false),
      observer_manager_(this) {
}

Widget::~Widget() {
  DestroyRootView();
  if (ownership_ == InitParams::WIDGET_OWNS_NATIVE_WIDGET) {
    delete native_widget_;
  } else {
    DCHECK(native_widget_destroyed_)
        << "Destroying a widget with a live native widget. "
        << "Widget probably should use WIDGET_OWNS_NATIVE_WIDGET ownership.";
  }
}

// static
Widget* Widget::CreateWindow(WidgetDelegate* delegate) {
  return CreateWindowWithBounds(delegate, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithBounds(WidgetDelegate* delegate,
                                       const gfx::Rect& bounds) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.bounds = bounds;
  params.delegate = delegate;
  widget->Init(params);
  return widget;
}

// static
Widget* Widget::CreateWindowWithParent(WidgetDelegate* delegate,
                                       gfx::NativeView parent) {
  return CreateWindowWithParentAndBounds(delegate, parent, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithParentAndBounds(WidgetDelegate* delegate,
                                                gfx::NativeView parent,
                                                const gfx::Rect& bounds) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.delegate = delegate;
  params.parent = parent;
  params.bounds = bounds;
  widget->Init(params);
  return widget;
}

// static
Widget* Widget::CreateWindowWithContext(WidgetDelegate* delegate,
                                        gfx::NativeView context) {
  return CreateWindowWithContextAndBounds(delegate, context, gfx::Rect());
}

// static
Widget* Widget::CreateWindowWithContextAndBounds(WidgetDelegate* delegate,
                                                 gfx::NativeView context,
                                                 const gfx::Rect& bounds) {
  Widget* widget = new Widget;
  Widget::InitParams params;
  params.delegate = delegate;
  params.context = context;
  params.bounds = bounds;
  widget->Init(params);
  return widget;
}

// static
Widget* Widget::GetWidgetForNativeView(gfx::NativeView native_view) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeView(native_view);
  return native_widget ? native_widget->GetWidget() : NULL;
}

// static
Widget* Widget::GetWidgetForNativeWindow(gfx::NativeWindow native_window) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetNativeWidgetForNativeWindow(
          native_window);
  return native_widget ? native_widget->GetWidget() : NULL;
}

// static
Widget* Widget::GetTopLevelWidgetForNativeView(gfx::NativeView native_view) {
  internal::NativeWidgetPrivate* native_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(native_view);
  return native_widget ? native_widget->GetWidget() : NULL;
}


// static
void Widget::GetAllChildWidgets(gfx::NativeView native_view,
                                Widgets* children) {
  internal::NativeWidgetPrivate::GetAllChildWidgets(native_view, children);
}

// static
void Widget::GetAllOwnedWidgets(gfx::NativeView native_view,
                                Widgets* owned) {
  internal::NativeWidgetPrivate::GetAllOwnedWidgets(native_view, owned);
}

// static
void Widget::ReparentNativeView(gfx::NativeView native_view,
                                gfx::NativeView new_parent) {
  internal::NativeWidgetPrivate::ReparentNativeView(native_view, new_parent);
}

// static
int Widget::GetLocalizedContentsWidth(int col_resource_id) {
  return ui::GetLocalizedContentsWidthForFont(col_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
int Widget::GetLocalizedContentsHeight(int row_resource_id) {
  return ui::GetLocalizedContentsHeightForFont(row_resource_id,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont));
}

// static
gfx::Size Widget::GetLocalizedContentsSize(int col_resource_id,
                                           int row_resource_id) {
  return gfx::Size(GetLocalizedContentsWidth(col_resource_id),
                   GetLocalizedContentsHeight(row_resource_id));
}

// static
bool Widget::RequiresNonClientView(InitParams::Type type) {
  return type == InitParams::TYPE_WINDOW ||
         type == InitParams::TYPE_PANEL ||
         type == InitParams::TYPE_BUBBLE;
}

void Widget::Init(const InitParams& in_params) {
  TRACE_EVENT0("views", "Widget::Init");
  InitParams params = in_params;

  params.child |= (params.type == InitParams::TYPE_CONTROL);
  is_top_level_ = !params.child;

  if (params.opacity == views::Widget::InitParams::INFER_OPACITY &&
      params.type != views::Widget::InitParams::TYPE_WINDOW &&
      params.type != views::Widget::InitParams::TYPE_PANEL)
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;

  if (ViewsDelegate::views_delegate)
    ViewsDelegate::views_delegate->OnBeforeWidgetInit(&params, this);

  if (params.opacity == views::Widget::InitParams::INFER_OPACITY)
    params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;

  bool can_activate = false;
  if (params.activatable != InitParams::ACTIVATABLE_DEFAULT) {
    can_activate = (params.activatable == InitParams::ACTIVATABLE_YES);
  } else if (params.type != InitParams::TYPE_CONTROL &&
             params.type != InitParams::TYPE_POPUP &&
             params.type != InitParams::TYPE_MENU &&
             params.type != InitParams::TYPE_TOOLTIP &&
             params.type != InitParams::TYPE_DRAG) {
    can_activate = true;
    params.activatable = InitParams::ACTIVATABLE_YES;
  } else {
    can_activate = false;
    params.activatable = InitParams::ACTIVATABLE_NO;
  }

  widget_delegate_ = params.delegate ?
      params.delegate : new DefaultWidgetDelegate(this);
  widget_delegate_->set_can_activate(can_activate);

  ownership_ = params.ownership;
  native_widget_ = CreateNativeWidget(params.native_widget, this)->
                   AsNativeWidgetPrivate();
  root_view_.reset(CreateRootView());
  default_theme_provider_.reset(new ui::DefaultThemeProvider);
  if (params.type == InitParams::TYPE_MENU) {
    is_mouse_button_pressed_ =
        internal::NativeWidgetPrivate::IsMouseButtonDown();
  }
  native_widget_->InitNativeWidget(params);
  if (RequiresNonClientView(params.type)) {
    non_client_view_ = new NonClientView;
    non_client_view_->SetFrameView(CreateNonClientFrameView());
    // Create the ClientView, add it to the NonClientView and add the
    // NonClientView to the RootView. This will cause everything to be parented.
    non_client_view_->set_client_view(widget_delegate_->CreateClientView(this));
    non_client_view_->SetOverlayView(widget_delegate_->CreateOverlayView());
    SetContentsView(non_client_view_);
    // Initialize the window's title before setting the window's initial bounds;
    // the frame view's preferred height may depend on the presence of a title.
    UpdateWindowTitle();
    non_client_view_->ResetWindowControls();
    SetInitialBounds(params.bounds);
    if (params.show_state == ui::SHOW_STATE_MAXIMIZED)
      Maximize();
    else if (params.show_state == ui::SHOW_STATE_MINIMIZED)
      Minimize();
  } else if (params.delegate) {
    SetContentsView(params.delegate->GetContentsView());
    SetInitialBoundsForFramelessWindow(params.bounds);
  }
  // This must come after SetContentsView() or it might not be able to find
  // the correct NativeTheme (on Linux). See http://crbug.com/384492
  observer_manager_.Add(GetNativeTheme());
  native_widget_initialized_ = true;
}

// Unconverted methods (see header) --------------------------------------------

gfx::NativeView Widget::GetNativeView() const {
  return native_widget_->GetNativeView();
}

gfx::NativeWindow Widget::GetNativeWindow() const {
  return native_widget_->GetNativeWindow();
}

void Widget::AddObserver(WidgetObserver* observer) {
  observers_.AddObserver(observer);
}

void Widget::RemoveObserver(WidgetObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool Widget::HasObserver(WidgetObserver* observer) {
  return observers_.HasObserver(observer);
}

void Widget::AddRemovalsObserver(WidgetRemovalsObserver* observer) {
  removals_observers_.AddObserver(observer);
}

void Widget::RemoveRemovalsObserver(WidgetRemovalsObserver* observer) {
  removals_observers_.RemoveObserver(observer);
}

bool Widget::HasRemovalsObserver(WidgetRemovalsObserver* observer) {
  return removals_observers_.HasObserver(observer);
}

bool Widget::GetAccelerator(int cmd_id, ui::Accelerator* accelerator) const {
  return false;
}

void Widget::ViewHierarchyChanged(
    const View::ViewHierarchyChangedDetails& details) {
  if (!details.is_add) {
    if (details.child == dragged_view_)
      dragged_view_ = NULL;
    FocusManager* focus_manager = GetFocusManager();
    if (focus_manager)
      focus_manager->ViewRemoved(details.child);
    ViewStorage::GetInstance()->ViewRemoved(details.child);
    native_widget_->ViewRemoved(details.child);
  }
}

void Widget::NotifyNativeViewHierarchyWillChange() {
  FocusManager* focus_manager = GetFocusManager();
  // We are being removed from a window hierarchy.  Treat this as
  // the root_view_ being removed.
  if (focus_manager)
    focus_manager->ViewRemoved(root_view_.get());
}

void Widget::NotifyNativeViewHierarchyChanged() {
  root_view_->NotifyNativeViewHierarchyChanged();
}

void Widget::NotifyWillRemoveView(View* view) {
    FOR_EACH_OBSERVER(WidgetRemovalsObserver,
                      removals_observers_,
                      OnWillRemoveView(this, view));
}

// Converted methods (see header) ----------------------------------------------

Widget* Widget::GetTopLevelWidget() {
  return const_cast<Widget*>(
      static_cast<const Widget*>(this)->GetTopLevelWidget());
}

const Widget* Widget::GetTopLevelWidget() const {
  // GetTopLevelNativeWidget doesn't work during destruction because
  // property is gone after gobject gets deleted. Short circuit here
  // for toplevel so that InputMethod can remove itself from
  // focus manager.
  return is_top_level() ? this : native_widget_->GetTopLevelWidget();
}

void Widget::SetContentsView(View* view) {
  // Do not SetContentsView() again if it is already set to the same view.
  if (view == GetContentsView())
    return;
  root_view_->SetContentsView(view);
  if (non_client_view_ != view) {
    // |non_client_view_| can only be non-NULL here if RequiresNonClientView()
    // was true when the widget was initialized. Creating widgets with non
    // client views and then setting the contents view can cause subtle
    // problems on Windows, where the native widget thinks there is still a
    // |non_client_view_|. If you get this error, either use a different type
    // when initializing the widget, or don't call SetContentsView().
    DCHECK(!non_client_view_);
    non_client_view_ = NULL;
  }
}

View* Widget::GetContentsView() {
  return root_view_->GetContentsView();
}

gfx::Rect Widget::GetWindowBoundsInScreen() const {
  return native_widget_->GetWindowBoundsInScreen();
}

gfx::Rect Widget::GetClientAreaBoundsInScreen() const {
  return native_widget_->GetClientAreaBoundsInScreen();
}

gfx::Rect Widget::GetRestoredBounds() const {
  return native_widget_->GetRestoredBounds();
}

void Widget::SetBounds(const gfx::Rect& bounds) {
  native_widget_->SetBounds(bounds);
}

void Widget::SetSize(const gfx::Size& size) {
  native_widget_->SetSize(size);
}

void Widget::CenterWindow(const gfx::Size& size) {
  native_widget_->CenterWindow(size);
}

void Widget::SetBoundsConstrained(const gfx::Rect& bounds) {
  gfx::Rect work_area =
      gfx::Screen::GetScreenFor(GetNativeView())->GetDisplayNearestPoint(
          bounds.origin()).work_area();
  if (work_area.IsEmpty()) {
    SetBounds(bounds);
  } else {
    // Inset the work area slightly.
    work_area.Inset(10, 10, 10, 10);
    work_area.AdjustToFit(bounds);
    SetBounds(work_area);
  }
}

void Widget::SetVisibilityChangedAnimationsEnabled(bool value) {
  native_widget_->SetVisibilityChangedAnimationsEnabled(value);
}

Widget::MoveLoopResult Widget::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    MoveLoopSource source,
    MoveLoopEscapeBehavior escape_behavior) {
  return native_widget_->RunMoveLoop(drag_offset, source, escape_behavior);
}

void Widget::EndMoveLoop() {
  native_widget_->EndMoveLoop();
}

void Widget::StackAboveWidget(Widget* widget) {
  native_widget_->StackAbove(widget->GetNativeView());
}

void Widget::StackAbove(gfx::NativeView native_view) {
  native_widget_->StackAbove(native_view);
}

void Widget::StackAtTop() {
  native_widget_->StackAtTop();
}

void Widget::StackBelow(gfx::NativeView native_view) {
  native_widget_->StackBelow(native_view);
}

void Widget::SetShape(gfx::NativeRegion shape) {
  native_widget_->SetShape(shape);
}

void Widget::Close() {
  if (widget_closed_) {
    // It appears we can hit this code path if you close a modal dialog then
    // close the last browser before the destructor is hit, which triggers
    // invoking Close again.
    return;
  }

  bool can_close = true;
  if (non_client_view_)
    can_close = non_client_view_->CanClose();
  if (can_close) {
    SaveWindowPlacement();

    // During tear-down the top-level focus manager becomes unavailable to
    // GTK tabbed panes and their children, so normal deregistration via
    // |FormManager::ViewRemoved()| calls are fouled.  We clear focus here
    // to avoid these redundant steps and to avoid accessing deleted views
    // that may have been in focus.
    if (is_top_level() && focus_manager_.get())
      focus_manager_->SetFocusedView(NULL);

    FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetClosing(this));
    native_widget_->Close();
    widget_closed_ = true;
  }
}

void Widget::CloseNow() {
  FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetClosing(this));
  native_widget_->CloseNow();
}

bool Widget::IsClosed() const {
  return widget_closed_;
}

void Widget::Show() {
  TRACE_EVENT0("views", "Widget::Show");
  if (non_client_view_) {
    // While initializing, the kiosk mode will go to full screen before the
    // widget gets shown. In that case we stay in full screen mode, regardless
    // of the |saved_show_state_| member.
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED &&
        !initial_restored_bounds_.IsEmpty() &&
        !IsFullscreen()) {
      native_widget_->ShowMaximizedWithBounds(initial_restored_bounds_);
    } else {
      native_widget_->ShowWithWindowState(
          IsFullscreen() ? ui::SHOW_STATE_FULLSCREEN : saved_show_state_);
    }
    // |saved_show_state_| only applies the first time the window is shown.
    // If we don't reset the value the window may be shown maximized every time
    // it is subsequently shown after being hidden.
    saved_show_state_ = ui::SHOW_STATE_NORMAL;
  } else {
    CanActivate()
        ? native_widget_->Show()
        : native_widget_->ShowWithWindowState(ui::SHOW_STATE_INACTIVE);
  }
}

void Widget::Hide() {
  native_widget_->Hide();
}

void Widget::ShowInactive() {
  // If this gets called with saved_show_state_ == ui::SHOW_STATE_MAXIMIZED,
  // call SetBounds()with the restored bounds to set the correct size. This
  // normally should not happen, but if it does we should avoid showing unsized
  // windows.
  if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED &&
      !initial_restored_bounds_.IsEmpty()) {
    SetBounds(initial_restored_bounds_);
    saved_show_state_ = ui::SHOW_STATE_NORMAL;
  }
  native_widget_->ShowWithWindowState(ui::SHOW_STATE_INACTIVE);
}

void Widget::Activate() {
  native_widget_->Activate();
}

void Widget::Deactivate() {
  native_widget_->Deactivate();
}

bool Widget::IsActive() const {
  return native_widget_->IsActive();
}

void Widget::DisableInactiveRendering() {
  SetInactiveRenderingDisabled(true);
}

void Widget::SetAlwaysOnTop(bool on_top) {
  native_widget_->SetAlwaysOnTop(on_top);
}

bool Widget::IsAlwaysOnTop() const {
  return native_widget_->IsAlwaysOnTop();
}

void Widget::SetVisibleOnAllWorkspaces(bool always_visible) {
  native_widget_->SetVisibleOnAllWorkspaces(always_visible);
}

void Widget::Maximize() {
  native_widget_->Maximize();
}

void Widget::Minimize() {
  native_widget_->Minimize();
}

void Widget::Restore() {
  native_widget_->Restore();
}

bool Widget::IsMaximized() const {
  return native_widget_->IsMaximized();
}

bool Widget::IsMinimized() const {
  return native_widget_->IsMinimized();
}

void Widget::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;

  native_widget_->SetFullscreen(fullscreen);

  if (non_client_view_)
    non_client_view_->Layout();
}

bool Widget::IsFullscreen() const {
  return native_widget_->IsFullscreen();
}

void Widget::SetOpacity(unsigned char opacity) {
  native_widget_->SetOpacity(opacity);
}

void Widget::SetUseDragFrame(bool use_drag_frame) {
  native_widget_->SetUseDragFrame(use_drag_frame);
}

void Widget::FlashFrame(bool flash) {
  native_widget_->FlashFrame(flash);
}

View* Widget::GetRootView() {
  return root_view_.get();
}

const View* Widget::GetRootView() const {
  return root_view_.get();
}

bool Widget::IsVisible() const {
  return native_widget_->IsVisible();
}

ui::ThemeProvider* Widget::GetThemeProvider() const {
  const Widget* root_widget = GetTopLevelWidget();
  if (root_widget && root_widget != this) {
    // Attempt to get the theme provider, and fall back to the default theme
    // provider if not found.
    ui::ThemeProvider* provider = root_widget->GetThemeProvider();
    if (provider)
      return provider;

    provider = root_widget->default_theme_provider_.get();
    if (provider)
      return provider;
  }
  return default_theme_provider_.get();
}

const ui::NativeTheme* Widget::GetNativeTheme() const {
  return native_widget_->GetNativeTheme();
}

FocusManager* Widget::GetFocusManager() {
  Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : NULL;
}

const FocusManager* Widget::GetFocusManager() const {
  const Widget* toplevel_widget = GetTopLevelWidget();
  return toplevel_widget ? toplevel_widget->focus_manager_.get() : NULL;
}

InputMethod* Widget::GetInputMethod() {
  return const_cast<InputMethod*>(
      const_cast<const Widget*>(this)->GetInputMethod());
}

const InputMethod* Widget::GetInputMethod() const {
  if (is_top_level()) {
    if (!input_method_.get())
      input_method_ = const_cast<Widget*>(this)->CreateInputMethod().Pass();
    return input_method_.get();
  } else {
    const Widget* toplevel = GetTopLevelWidget();
    // If GetTopLevelWidget() returns itself which is not toplevel,
    // the widget is detached from toplevel widget.
    // TODO(oshima): Fix GetTopLevelWidget() to return NULL
    // if there is no toplevel. We probably need to add GetTopMostWidget()
    // to replace some use cases.
    return (toplevel && toplevel != this) ? toplevel->GetInputMethod() : NULL;
  }
}

ui::InputMethod* Widget::GetHostInputMethod() {
  return native_widget_private()->GetHostInputMethod();
}

void Widget::RunShellDrag(View* view,
                          const ui::OSExchangeData& data,
                          const gfx::Point& location,
                          int operation,
                          ui::DragDropTypes::DragEventSource source) {
  dragged_view_ = view;
  native_widget_->RunShellDrag(view, data, location, operation, source);
  // If the view is removed during the drag operation, dragged_view_ is set to
  // NULL.
  if (view && dragged_view_ == view) {
    dragged_view_ = NULL;
    view->OnDragDone();
  }
}

void Widget::SchedulePaintInRect(const gfx::Rect& rect) {
  native_widget_->SchedulePaintInRect(rect);
}

void Widget::SetCursor(gfx::NativeCursor cursor) {
  native_widget_->SetCursor(cursor);
}

bool Widget::IsMouseEventsEnabled() const {
  return native_widget_->IsMouseEventsEnabled();
}

void Widget::SetNativeWindowProperty(const char* name, void* value) {
  native_widget_->SetNativeWindowProperty(name, value);
}

void* Widget::GetNativeWindowProperty(const char* name) const {
  return native_widget_->GetNativeWindowProperty(name);
}

void Widget::UpdateWindowTitle() {
  if (!non_client_view_)
    return;

  // Update the native frame's text. We do this regardless of whether or not
  // the native frame is being used, since this also updates the taskbar, etc.
  base::string16 window_title = widget_delegate_->GetWindowTitle();
  base::i18n::AdjustStringForLocaleDirection(&window_title);
  if (!native_widget_->SetWindowTitle(window_title))
    return;
  non_client_view_->UpdateWindowTitle();

  // If the non-client view is rendering its own title, it'll need to relayout
  // now and to get a paint update later on.
  non_client_view_->Layout();
}

void Widget::UpdateWindowIcon() {
  if (non_client_view_)
    non_client_view_->UpdateWindowIcon();
  native_widget_->SetWindowIcons(widget_delegate_->GetWindowIcon(),
                                 widget_delegate_->GetWindowAppIcon());
}

FocusTraversable* Widget::GetFocusTraversable() {
  return static_cast<internal::RootView*>(root_view_.get());
}

void Widget::ThemeChanged() {
  root_view_->ThemeChanged();
}

void Widget::LocaleChanged() {
  root_view_->LocaleChanged();
}

void Widget::SetFocusTraversableParent(FocusTraversable* parent) {
  root_view_->SetFocusTraversableParent(parent);
}

void Widget::SetFocusTraversableParentView(View* parent_view) {
  root_view_->SetFocusTraversableParentView(parent_view);
}

void Widget::ClearNativeFocus() {
  native_widget_->ClearNativeFocus();
}

NonClientFrameView* Widget::CreateNonClientFrameView() {
  NonClientFrameView* frame_view =
      widget_delegate_->CreateNonClientFrameView(this);
  if (!frame_view)
    frame_view = native_widget_->CreateNonClientFrameView();
  if (!frame_view && ViewsDelegate::views_delegate) {
    frame_view =
        ViewsDelegate::views_delegate->CreateDefaultNonClientFrameView(this);
  }
  if (frame_view)
    return frame_view;

  CustomFrameView* custom_frame_view = new CustomFrameView;
  custom_frame_view->Init(this);
  return custom_frame_view;
}

bool Widget::ShouldUseNativeFrame() const {
  if (frame_type_ != FRAME_TYPE_DEFAULT)
    return frame_type_ == FRAME_TYPE_FORCE_NATIVE;
  return native_widget_->ShouldUseNativeFrame();
}

bool Widget::ShouldWindowContentsBeTransparent() const {
  return native_widget_->ShouldWindowContentsBeTransparent();
}

void Widget::DebugToggleFrameType() {
  if (frame_type_ == FRAME_TYPE_DEFAULT) {
    frame_type_ = ShouldUseNativeFrame() ? FRAME_TYPE_FORCE_CUSTOM :
        FRAME_TYPE_FORCE_NATIVE;
  } else {
    frame_type_ = frame_type_ == FRAME_TYPE_FORCE_CUSTOM ?
        FRAME_TYPE_FORCE_NATIVE : FRAME_TYPE_FORCE_CUSTOM;
  }
  FrameTypeChanged();
}

void Widget::FrameTypeChanged() {
  native_widget_->FrameTypeChanged();
}

const ui::Compositor* Widget::GetCompositor() const {
  return native_widget_->GetCompositor();
}

ui::Compositor* Widget::GetCompositor() {
  return native_widget_->GetCompositor();
}

ui::Layer* Widget::GetLayer() {
  return native_widget_->GetLayer();
}

void Widget::ReorderNativeViews() {
  native_widget_->ReorderNativeViews();
}

void Widget::UpdateRootLayers() {
  // Calculate the layers requires traversing the tree, and since nearly any
  // mutation of the tree can trigger this call we delay until absolutely
  // necessary.
  root_layers_dirty_ = true;
}

const NativeWidget* Widget::native_widget() const {
  return native_widget_;
}

NativeWidget* Widget::native_widget() {
  return native_widget_;
}

void Widget::SetCapture(View* view) {
  if (internal::NativeWidgetPrivate::IsMouseButtonDown())
    is_mouse_button_pressed_ = true;
  if (internal::NativeWidgetPrivate::IsTouchDown())
    is_touch_down_ = true;
  root_view_->SetMouseHandler(view);
  if (!native_widget_->HasCapture())
    native_widget_->SetCapture();
}

void Widget::ReleaseCapture() {
  if (native_widget_->HasCapture())
    native_widget_->ReleaseCapture();
}

bool Widget::HasCapture() {
  return native_widget_->HasCapture();
}

TooltipManager* Widget::GetTooltipManager() {
  return native_widget_->GetTooltipManager();
}

const TooltipManager* Widget::GetTooltipManager() const {
  return native_widget_->GetTooltipManager();
}

gfx::Rect Widget::GetWorkAreaBoundsInScreen() const {
  return native_widget_->GetWorkAreaBoundsInScreen();
}

void Widget::SynthesizeMouseMoveEvent() {
  last_mouse_event_was_move_ = false;
  ui::MouseEvent mouse_event(ui::ET_MOUSE_MOVED,
                             last_mouse_event_position_,
                             last_mouse_event_position_,
                             ui::EF_IS_SYNTHESIZED, 0);
  root_view_->OnMouseMoved(mouse_event);
}

void Widget::OnRootViewLayout() {
  native_widget_->OnRootViewLayout();
}

bool Widget::IsTranslucentWindowOpacitySupported() const {
  return native_widget_->IsTranslucentWindowOpacitySupported();
}

void Widget::OnOwnerClosing() {
}

////////////////////////////////////////////////////////////////////////////////
// Widget, NativeWidgetDelegate implementation:

bool Widget::IsModal() const {
  return widget_delegate_->GetModalType() != ui::MODAL_TYPE_NONE;
}

bool Widget::IsDialogBox() const {
  return !!widget_delegate_->AsDialogDelegate();
}

bool Widget::CanActivate() const {
  return widget_delegate_->CanActivate();
}

bool Widget::IsInactiveRenderingDisabled() const {
  return disable_inactive_rendering_;
}

void Widget::EnableInactiveRendering() {
  SetInactiveRenderingDisabled(false);
}

void Widget::OnNativeWidgetActivationChanged(bool active) {
  // On windows we may end up here before we've completed initialization (from
  // an WM_NCACTIVATE). If that happens the WidgetDelegate likely doesn't know
  // the Widget and will crash attempting to access it.
  if (!active && native_widget_initialized_)
    SaveWindowPlacement();

  FOR_EACH_OBSERVER(WidgetObserver, observers_,
                    OnWidgetActivationChanged(this, active));

  // During window creation, the widget gets focused without activation, and in
  // that case, the focus manager cannot set the appropriate text input client
  // because the widget is not active.  Thus we have to notify the focus manager
  // not only when the focus changes but also when the widget gets activated.
  // See crbug.com/377479 for details.
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager) {
    if (active)
      focus_manager->FocusTextInputClient(focus_manager->GetFocusedView());
    else
      focus_manager->BlurTextInputClient(focus_manager->GetFocusedView());
  }

  if (IsVisible() && non_client_view())
    non_client_view()->frame_view()->SchedulePaint();
}

void Widget::OnNativeFocus(gfx::NativeView old_focused_view) {
  WidgetFocusManager::GetInstance()->OnWidgetFocusEvent(old_focused_view,
                                                        GetNativeView());
}

void Widget::OnNativeBlur(gfx::NativeView new_focused_view) {
  WidgetFocusManager::GetInstance()->OnWidgetFocusEvent(GetNativeView(),
                                                        new_focused_view);
}

void Widget::OnNativeWidgetVisibilityChanging(bool visible) {
  FOR_EACH_OBSERVER(WidgetObserver, observers_,
                    OnWidgetVisibilityChanging(this, visible));
}

void Widget::OnNativeWidgetVisibilityChanged(bool visible) {
  View* root = GetRootView();
  if (root)
    root->PropagateVisibilityNotifications(root, visible);
  FOR_EACH_OBSERVER(WidgetObserver, observers_,
                    OnWidgetVisibilityChanged(this, visible));
  if (GetCompositor() && root && root->layer())
    root->layer()->SetVisible(visible);
}

void Widget::OnNativeWidgetCreated(bool desktop_widget) {
  if (is_top_level())
    focus_manager_.reset(FocusManagerFactory::Create(this, desktop_widget));

  native_widget_->InitModalType(widget_delegate_->GetModalType());

  FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetCreated(this));
}

void Widget::OnNativeWidgetDestroying() {
  // Tell the focus manager (if any) that root_view is being removed
  // in case that the focused view is under this root view.
  if (GetFocusManager())
    GetFocusManager()->ViewRemoved(root_view_.get());
  FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetDestroying(this));
  if (non_client_view_)
    non_client_view_->WindowClosing();
  widget_delegate_->WindowClosing();
}

void Widget::OnNativeWidgetDestroyed() {
  FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetDestroyed(this));
  widget_delegate_->DeleteDelegate();
  widget_delegate_ = NULL;
  native_widget_destroyed_ = true;
}

gfx::Size Widget::GetMinimumSize() const {
  return non_client_view_ ? non_client_view_->GetMinimumSize() : gfx::Size();
}

gfx::Size Widget::GetMaximumSize() const {
  return non_client_view_ ? non_client_view_->GetMaximumSize() : gfx::Size();
}

void Widget::OnNativeWidgetMove() {
  widget_delegate_->OnWidgetMove();
  View* root = GetRootView();
  if (root && root->GetFocusManager()) {
    View* focused_view = root->GetFocusManager()->GetFocusedView();
    if (focused_view && focused_view->GetInputMethod())
      focused_view->GetInputMethod()->OnCaretBoundsChanged(focused_view);
  }
  FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetBoundsChanged(
    this,
    GetWindowBoundsInScreen()));
}

void Widget::OnNativeWidgetSizeChanged(const gfx::Size& new_size) {
  View* root = GetRootView();
  if (root) {
    root->SetSize(new_size);
    if (root->GetFocusManager()) {
      View* focused_view = GetRootView()->GetFocusManager()->GetFocusedView();
      if (focused_view && focused_view->GetInputMethod())
        focused_view->GetInputMethod()->OnCaretBoundsChanged(focused_view);
    }
  }
  // Size changed notifications can fire prior to full initialization
  // i.e. during session restore.  Avoid saving session state during these
  // startup procedures.
  if (native_widget_initialized_)
    SaveWindowPlacement();

  FOR_EACH_OBSERVER(WidgetObserver, observers_, OnWidgetBoundsChanged(
    this,
    GetWindowBoundsInScreen()));
}

void Widget::OnNativeWidgetBeginUserBoundsChange() {
  widget_delegate_->OnWindowBeginUserBoundsChange();
}

void Widget::OnNativeWidgetEndUserBoundsChange() {
  widget_delegate_->OnWindowEndUserBoundsChange();
}

bool Widget::HasFocusManager() const {
  return !!focus_manager_.get();
}

bool Widget::OnNativeWidgetPaintAccelerated(const gfx::Rect& dirty_region) {
  ui::Compositor* compositor = GetCompositor();
  if (!compositor)
    return false;

  compositor->ScheduleRedrawRect(dirty_region);
  return true;
}

void Widget::OnNativeWidgetPaint(gfx::Canvas* canvas) {
  // On Linux Aura, we can get here during Init() because of the
  // SetInitialBounds call.
  if (native_widget_initialized_)
    GetRootView()->Paint(canvas, CullSet());
}

int Widget::GetNonClientComponent(const gfx::Point& point) {
  int component = non_client_view_ ?
      non_client_view_->NonClientHitTest(point) :
      HTNOWHERE;

  if (movement_disabled_ && (component == HTCAPTION || component == HTSYSMENU))
    return HTNOWHERE;

  return component;
}

void Widget::OnKeyEvent(ui::KeyEvent* event) {
  SendEventToProcessor(event);
}

// TODO(tdanderson): We should not be calling the OnMouse*() functions on
//                   RootView from anywhere in Widget. Use
//                   SendEventToProcessor() instead. See crbug.com/348087.
void Widget::OnMouseEvent(ui::MouseEvent* event) {
  View* root_view = GetRootView();
  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED: {
      last_mouse_event_was_move_ = false;

      // We may get deleted by the time we return from OnMousePressed. So we
      // use an observer to make sure we are still alive.
      WidgetDeletionObserver widget_deletion_observer(this);

      // Make sure we're still visible before we attempt capture as the mouse
      // press processing may have made the window hide (as happens with menus).

      // It is possible for a View to show a context menu on mouse-press. Since
      // the menu does a capture and starts a nested message-loop, the release
      // would go to the menu. The next click (i.e. both mouse-press and release
      // events) also go to the menu. The menu (and the nested message-loop)
      // gets closed after this second release event. The code then resumes from
      // here. So make sure that the mouse-button is still down before doing a
      // capture.
      if (root_view && root_view->OnMousePressed(*event) &&
          widget_deletion_observer.IsWidgetAlive() && IsVisible() &&
          internal::NativeWidgetPrivate::IsMouseButtonDown()) {
        is_mouse_button_pressed_ = true;
        if (!native_widget_->HasCapture())
          native_widget_->SetCapture();
        event->SetHandled();
      }
      return;
    }

    case ui::ET_MOUSE_RELEASED:
      last_mouse_event_was_move_ = false;
      is_mouse_button_pressed_ = false;
      // Release capture first, to avoid confusion if OnMouseReleased blocks.
      if (auto_release_capture_ && native_widget_->HasCapture())
        native_widget_->ReleaseCapture();
      if (root_view)
        root_view->OnMouseReleased(*event);
      if ((event->flags() & ui::EF_IS_NON_CLIENT) == 0)
        event->SetHandled();
      return;

    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      if (native_widget_->HasCapture() && is_mouse_button_pressed_) {
        last_mouse_event_was_move_ = false;
        if (root_view)
          root_view->OnMouseDragged(*event);
      } else if (!last_mouse_event_was_move_ ||
                 last_mouse_event_position_ != event->location()) {
        last_mouse_event_position_ = event->location();
        last_mouse_event_was_move_ = true;
        if (root_view)
          root_view->OnMouseMoved(*event);
      }
      return;

    case ui::ET_MOUSE_EXITED:
      last_mouse_event_was_move_ = false;
      if (root_view)
        root_view->OnMouseExited(*event);
      return;

    case ui::ET_MOUSEWHEEL:
      if (root_view && root_view->OnMouseWheel(
          static_cast<const ui::MouseWheelEvent&>(*event)))
        event->SetHandled();
      return;

    default:
      return;
  }
}

void Widget::OnMouseCaptureLost() {
  if (is_mouse_button_pressed_ || is_touch_down_) {
    View* root_view = GetRootView();
    if (root_view)
      root_view->OnMouseCaptureLost();
  }
  is_touch_down_ = false;
  is_mouse_button_pressed_ = false;
}

void Widget::OnScrollEvent(ui::ScrollEvent* event) {
  ui::ScrollEvent event_copy(*event);
  SendEventToProcessor(&event_copy);

  // Convert unhandled ui::ET_SCROLL events into ui::ET_MOUSEWHEEL events.
  if (!event_copy.handled() && event_copy.type() == ui::ET_SCROLL) {
    ui::MouseWheelEvent wheel(*event);
    OnMouseEvent(&wheel);
  }
}

void Widget::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      is_touch_down_ = true;
      // We explicitly don't capture here. Not capturing enables multiple
      // widgets to get tap events at the same time. Views (such as tab
      // dragging) may explicitly capture.
      break;

    case ui::ET_GESTURE_END:
      if (event->details().touch_points() == 1)
        is_touch_down_ = false;
      break;

    default:
      break;
  }
  SendEventToProcessor(event);
}

bool Widget::ExecuteCommand(int command_id) {
  return widget_delegate_->ExecuteWindowsCommand(command_id);
}

InputMethod* Widget::GetInputMethodDirect() {
  return input_method_.get();
}

const std::vector<ui::Layer*>& Widget::GetRootLayers() {
  if (root_layers_dirty_) {
    root_layers_dirty_ = false;
    root_layers_.clear();
    BuildRootLayers(GetRootView(), &root_layers_);
  }
  return root_layers_;
}

bool Widget::HasHitTestMask() const {
  return widget_delegate_->WidgetHasHitTestMask();
}

void Widget::GetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  widget_delegate_->GetWidgetHitTestMask(mask);
}

Widget* Widget::AsWidget() {
  return this;
}

const Widget* Widget::AsWidget() const {
  return this;
}

bool Widget::SetInitialFocus(ui::WindowShowState show_state) {
  View* v = widget_delegate_->GetInitiallyFocusedView();
  if (!focus_on_creation_ || show_state == ui::SHOW_STATE_INACTIVE ||
      show_state == ui::SHOW_STATE_MINIMIZED) {
    // If not focusing the window now, tell the focus manager which view to
    // focus when the window is restored.
    if (v)
      focus_manager_->SetStoredFocusView(v);
    return true;
  }
  if (v)
    v->RequestFocus();
  return !!v;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::EventSource implementation:
ui::EventProcessor* Widget::GetEventProcessor() {
  return root_view_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, FocusTraversable implementation:

FocusSearch* Widget::GetFocusSearch() {
  return root_view_->GetFocusSearch();
}

FocusTraversable* Widget::GetFocusTraversableParent() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

View* Widget::GetFocusTraversableParentView() {
  // We are a proxy to the root view, so we should be bypassed when traversing
  // up and as a result this should not be called.
  NOTREACHED();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Widget, ui::NativeThemeObserver implementation:

void Widget::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK(observer_manager_.IsObserving(observed_theme));

  ui::NativeTheme* current_native_theme = GetNativeTheme();
  if (!observer_manager_.IsObserving(current_native_theme)) {
    observer_manager_.RemoveAll();
    observer_manager_.Add(current_native_theme);
  }

  root_view_->PropagateNativeThemeChanged(current_native_theme);
}

////////////////////////////////////////////////////////////////////////////////
// Widget, protected:

internal::RootView* Widget::CreateRootView() {
  return new internal::RootView(this);
}

void Widget::DestroyRootView() {
  non_client_view_ = NULL;
  root_view_.reset();
  // Input method has to be destroyed before focus manager.
  input_method_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// Widget, private:

void Widget::SetInactiveRenderingDisabled(bool value) {
  if (value == disable_inactive_rendering_)
    return;

  disable_inactive_rendering_ = value;
  if (non_client_view_)
    non_client_view_->SetInactiveRenderingDisabled(value);
}

void Widget::SaveWindowPlacement() {
  // The window delegate does the actual saving for us. It seems like (judging
  // by go/crash) that in some circumstances we can end up here after
  // WM_DESTROY, at which point the window delegate is likely gone. So just
  // bail.
  if (!widget_delegate_)
    return;

  ui::WindowShowState show_state = ui::SHOW_STATE_NORMAL;
  gfx::Rect bounds;
  native_widget_->GetWindowPlacement(&bounds, &show_state);
  widget_delegate_->SaveWindowPlacement(bounds, show_state);
}

void Widget::SetInitialBounds(const gfx::Rect& bounds) {
  if (!non_client_view_)
    return;

  gfx::Rect saved_bounds;
  if (GetSavedWindowPlacement(&saved_bounds, &saved_show_state_)) {
    if (saved_show_state_ == ui::SHOW_STATE_MAXIMIZED) {
      // If we're going to maximize, wait until Show is invoked to set the
      // bounds. That way we avoid a noticeable resize.
      initial_restored_bounds_ = saved_bounds;
    } else if (!saved_bounds.IsEmpty()) {
      // If the saved bounds are valid, use them.
      SetBounds(saved_bounds);
    }
  } else {
    if (bounds.IsEmpty()) {
      // No initial bounds supplied, so size the window to its content and
      // center over its parent.
      native_widget_->CenterWindow(non_client_view_->GetPreferredSize());
    } else {
      // Use the supplied initial bounds.
      SetBoundsConstrained(bounds);
    }
  }
}

void Widget::SetInitialBoundsForFramelessWindow(const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    View* contents_view = GetContentsView();
    DCHECK(contents_view);
    // No initial bounds supplied, so size the window to its content and
    // center over its parent if preferred size is provided.
    gfx::Size size = contents_view->GetPreferredSize();
    if (!size.IsEmpty())
      native_widget_->CenterWindow(size);
  } else {
    // Use the supplied initial bounds.
    SetBoundsConstrained(bounds);
  }
}

bool Widget::GetSavedWindowPlacement(gfx::Rect* bounds,
                                     ui::WindowShowState* show_state) {
  // First we obtain the window's saved show-style and store it. We need to do
  // this here, rather than in Show() because by the time Show() is called,
  // the window's size will have been reset (below) and the saved maximized
  // state will have been lost. Sadly there's no way to tell on Windows when
  // a window is restored from maximized state, so we can't more accurately
  // track maximized state independently of sizing information.

  // Restore the window's placement from the controller.
  if (widget_delegate_->GetSavedWindowPlacement(this, bounds, show_state)) {
    if (!widget_delegate_->ShouldRestoreWindowSize()) {
      bounds->set_size(non_client_view_->GetPreferredSize());
    } else {
      gfx::Size minimum_size = GetMinimumSize();
      // Make sure the bounds are at least the minimum size.
      if (bounds->width() < minimum_size.width())
        bounds->set_width(minimum_size.width());

      if (bounds->height() < minimum_size.height())
        bounds->set_height(minimum_size.height());
    }
    return true;
  }
  return false;
}

scoped_ptr<InputMethod> Widget::CreateInputMethod() {
  scoped_ptr<InputMethod> input_method(native_widget_->CreateInputMethod());
  if (input_method.get())
    input_method->Init(this);
  return input_method.Pass();
}

void Widget::ReplaceInputMethod(InputMethod* input_method) {
  input_method_.reset(input_method);
  input_method->SetDelegate(native_widget_->GetInputMethodDelegate());
  input_method->Init(this);
}

namespace internal {

////////////////////////////////////////////////////////////////////////////////
// internal::NativeWidgetPrivate, NativeWidget implementation:

internal::NativeWidgetPrivate* NativeWidgetPrivate::AsNativeWidgetPrivate() {
  return this;
}

}  // namespace internal
}  // namespace views
