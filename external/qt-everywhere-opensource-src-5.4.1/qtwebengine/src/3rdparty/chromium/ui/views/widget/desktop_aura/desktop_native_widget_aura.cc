// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/drag_utils.h"
#include "ui/views/ime/input_method_bridge.h"
#include "ui/views/ime/null_input_method.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/desktop_aura/desktop_capture_client.h"
#include "ui/views/widget/desktop_aura/desktop_cursor_loader_updater.h"
#include "ui/views/widget/desktop_aura/desktop_dispatcher_client.h"
#include "ui/views/widget/desktop_aura/desktop_event_client.h"
#include "ui/views/widget/desktop_aura/desktop_focus_rules.h"
#include "ui/views/widget/desktop_aura/desktop_native_cursor_manager.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_aura_utils.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/window_reorderer.h"
#include "ui/views/window/native_frame_view.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/input_method_event_filter.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/drag_drop_client.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#include "ui/gfx/win/dpi.h"
#endif

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(VIEWS_EXPORT,
                                      views::DesktopNativeWidgetAura*);

namespace views {

DEFINE_WINDOW_PROPERTY_KEY(DesktopNativeWidgetAura*,
                           kDesktopNativeWidgetAuraKey, NULL);

namespace {

// This class provides functionality to create a top level widget to host a
// child window.
class DesktopNativeWidgetTopLevelHandler : public aura::WindowObserver {
 public:
  // This function creates a widget with the bounds passed in which eventually
  // becomes the parent of the child window passed in.
  static aura::Window* CreateParentWindow(aura::Window* child_window,
                                          const gfx::Rect& bounds,
                                          bool full_screen,
                                          bool root_is_always_on_top) {
    // This instance will get deleted when the widget is destroyed.
    DesktopNativeWidgetTopLevelHandler* top_level_handler =
        new DesktopNativeWidgetTopLevelHandler;

    child_window->SetBounds(gfx::Rect(bounds.size()));

    Widget::InitParams init_params;
    init_params.type = full_screen ? Widget::InitParams::TYPE_WINDOW :
        Widget::InitParams::TYPE_POPUP;
    init_params.bounds = bounds;
    init_params.ownership = Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
    init_params.layer_type = aura::WINDOW_LAYER_NOT_DRAWN;
    init_params.activatable = full_screen ?
        Widget::InitParams::ACTIVATABLE_YES :
        Widget::InitParams::ACTIVATABLE_NO;
    init_params.keep_on_top = root_is_always_on_top;

    // This widget instance will get deleted when the window is
    // destroyed.
    top_level_handler->top_level_widget_ = new Widget();
    top_level_handler->top_level_widget_->Init(init_params);

    top_level_handler->top_level_widget_->SetFullscreen(full_screen);
    top_level_handler->top_level_widget_->Show();

    aura::Window* native_window =
        top_level_handler->top_level_widget_->GetNativeView();
    child_window->AddObserver(top_level_handler);
    native_window->AddObserver(top_level_handler);
    top_level_handler->child_window_ = child_window;
    return native_window;
  }

  // aura::WindowObserver overrides
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    window->RemoveObserver(this);

    // If the widget is being destroyed by the OS then we should not try and
    // destroy it again.
    if (top_level_widget_ &&
        window == top_level_widget_->GetNativeView()) {
      top_level_widget_ = NULL;
      return;
    }

    if (top_level_widget_) {
      DCHECK(top_level_widget_->GetNativeView());
      top_level_widget_->GetNativeView()->RemoveObserver(this);
      // When we receive a notification that the child of the window created
      // above is being destroyed we go ahead and initiate the destruction of
      // the corresponding widget.
      top_level_widget_->Close();
      top_level_widget_ = NULL;
    }
    delete this;
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    if (top_level_widget_ && window == child_window_)
      top_level_widget_->SetSize(new_bounds.size());
  }

 private:
  DesktopNativeWidgetTopLevelHandler()
      : top_level_widget_(NULL),
        child_window_(NULL) {}

  virtual ~DesktopNativeWidgetTopLevelHandler() {}

  Widget* top_level_widget_;
  aura::Window* child_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetTopLevelHandler);
};

class DesktopNativeWidgetAuraWindowTreeClient :
    public aura::client::WindowTreeClient {
 public:
  explicit DesktopNativeWidgetAuraWindowTreeClient(
      aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowTreeClient(root_window_, this);
  }
  virtual ~DesktopNativeWidgetAuraWindowTreeClient() {
    aura::client::SetWindowTreeClient(root_window_, NULL);
  }

  // Overridden from client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    bool is_fullscreen = window->GetProperty(aura::client::kShowStateKey) ==
        ui::SHOW_STATE_FULLSCREEN;
    bool is_menu = window->type() == ui::wm::WINDOW_TYPE_MENU;

    if (is_fullscreen || is_menu) {
      bool root_is_always_on_top = false;
      internal::NativeWidgetPrivate* native_widget =
          DesktopNativeWidgetAura::ForWindow(root_window_);
      if (native_widget)
        root_is_always_on_top = native_widget->IsAlwaysOnTop();

      return DesktopNativeWidgetTopLevelHandler::CreateParentWindow(
          window, bounds, is_fullscreen, root_is_always_on_top);
    }
    return root_window_;
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNativeWidgetAuraWindowTreeClient);
};

}  // namespace

class FocusManagerEventHandler : public ui::EventHandler {
 public:
  FocusManagerEventHandler(DesktopNativeWidgetAura* desktop_native_widget_aura)
      : desktop_native_widget_aura_(desktop_native_widget_aura) {}

  // Implementation of ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    Widget* widget = desktop_native_widget_aura_->GetWidget();
    if (widget && widget->GetFocusManager()->GetFocusedView() &&
        !widget->GetFocusManager()->OnKeyEvent(*event)) {
      event->SetHandled();
    }
  }

 private:
  DesktopNativeWidgetAura* desktop_native_widget_aura_;

  DISALLOW_COPY_AND_ASSIGN(FocusManagerEventHandler);
};

class RootWindowDestructionObserver : public aura::WindowObserver {
 public:
  explicit RootWindowDestructionObserver(DesktopNativeWidgetAura* parent)
    : parent_(parent) {}
  virtual ~RootWindowDestructionObserver() {}

 private:
  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    parent_->RootWindowDestroyed();
    window->RemoveObserver(this);
    delete this;
  }

  DesktopNativeWidgetAura* parent_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowDestructionObserver);
};

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, public:

int DesktopNativeWidgetAura::cursor_reference_count_ = 0;
DesktopNativeCursorManager* DesktopNativeWidgetAura::native_cursor_manager_ =
    NULL;
wm::CursorManager* DesktopNativeWidgetAura::cursor_manager_ = NULL;

DesktopNativeWidgetAura::DesktopNativeWidgetAura(
    internal::NativeWidgetDelegate* delegate)
    : desktop_window_tree_host_(NULL),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      close_widget_factory_(this),
      content_window_container_(NULL),
      content_window_(new aura::Window(this)),
      native_widget_delegate_(delegate),
      last_drop_operation_(ui::DragDropTypes::DRAG_NONE),
      restore_focus_on_activate_(false),
      cursor_(gfx::kNullCursor),
      widget_type_(Widget::InitParams::TYPE_WINDOW) {
  aura::client::SetFocusChangeObserver(content_window_, this);
  aura::client::SetActivationChangeObserver(content_window_, this);
}

DesktopNativeWidgetAura::~DesktopNativeWidgetAura() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete native_widget_delegate_;
  else
    CloseNow();
}

// static
DesktopNativeWidgetAura* DesktopNativeWidgetAura::ForWindow(
    aura::Window* window) {
  return window->GetProperty(kDesktopNativeWidgetAuraKey);
}

void DesktopNativeWidgetAura::OnHostClosed() {
  // Don't invoke Widget::OnNativeWidgetDestroying(), its done by
  // DesktopWindowTreeHost.

  // The WindowModalityController is at the front of the event pretarget
  // handler list. We destroy it first to preserve order symantics.
  if (window_modality_controller_)
    window_modality_controller_.reset();

  // Make sure we don't have capture. Otherwise CaptureController and
  // WindowEventDispatcher are left referencing a deleted Window.
  {
    aura::Window* capture_window = capture_client_->GetCaptureWindow();
    if (capture_window && host_->window()->Contains(capture_window))
      capture_window->ReleaseCapture();
  }

  // DesktopWindowTreeHost owns the ActivationController which ShadowController
  // references. Make sure we destroy ShadowController early on.
  shadow_controller_.reset();
  tooltip_manager_.reset();
  if (tooltip_controller_.get()) {
    host_->window()->RemovePreTargetHandler(tooltip_controller_.get());
    aura::client::SetTooltipClient(host_->window(), NULL);
    tooltip_controller_.reset();
  }

  root_window_event_filter_->RemoveHandler(input_method_event_filter_.get());

  window_tree_client_.reset();  // Uses host_->dispatcher() at destruction.

  capture_client_.reset();  // Uses host_->dispatcher() at destruction.

  // FocusController uses |content_window_|. Destroy it now so that we don't
  // have to worry about the possibility of FocusController attempting to use
  // |content_window_| after it's been destroyed but before all child windows
  // have been destroyed.
  host_->window()->RemovePreTargetHandler(focus_client_.get());
  aura::client::SetFocusClient(host_->window(), NULL);
  aura::client::SetActivationClient(host_->window(), NULL);
  focus_client_.reset();

  host_->RemoveObserver(this);
  host_.reset();  // Uses input_method_event_filter_ at destruction.
  // WindowEventDispatcher owns |desktop_window_tree_host_|.
  desktop_window_tree_host_ = NULL;
  content_window_ = NULL;

  native_widget_delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void DesktopNativeWidgetAura::OnDesktopWindowTreeHostDestroyed(
    aura::WindowTreeHost* host) {
  // |dispatcher_| is still valid, but DesktopWindowTreeHost is nearly
  // destroyed. Do cleanup here of members DesktopWindowTreeHost may also use.
  aura::client::SetDispatcherClient(host->window(), NULL);
  dispatcher_client_.reset();

  // We explicitly do NOT clear the cursor client property. Since the cursor
  // manager is a singleton, it can outlive any window hierarchy, and it's
  // important that objects attached to this destroying window hierarchy have
  // an opportunity to deregister their observers from the cursor manager.
  // They may want to do this when they are notified that they're being
  // removed from the window hierarchy, which happens soon after this
  // function when DesktopWindowTreeHost* calls DestroyDispatcher().
  native_cursor_manager_->RemoveHost(host);

  aura::client::SetScreenPositionClient(host->window(), NULL);
  position_client_.reset();

  aura::client::SetDragDropClient(host->window(), NULL);
  drag_drop_client_.reset();

  aura::client::SetEventClient(host->window(), NULL);
  event_client_.reset();
}

void DesktopNativeWidgetAura::HandleActivationChanged(bool active) {
  native_widget_delegate_->OnNativeWidgetActivationChanged(active);
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(host_->window());
  if (!activation_client)
    return;
  if (active) {
    if (GetWidget()->HasFocusManager()) {
      // This function can be called before the focus manager has had a
      // chance to set the focused view. In which case we should get the
      // last focused view.
      View* view_for_activation =
          GetWidget()->GetFocusManager()->GetFocusedView() ?
              GetWidget()->GetFocusManager()->GetFocusedView() :
                  GetWidget()->GetFocusManager()->GetStoredFocusView();
      if (!view_for_activation)
        view_for_activation = GetWidget()->GetRootView();
      activation_client->ActivateWindow(
          view_for_activation->GetWidget()->GetNativeView());
    }
  } else {
    // If we're not active we need to deactivate the corresponding
    // aura::Window. This way if a child widget is active it gets correctly
    // deactivated (child widgets don't get native desktop activation changes,
    // only aura activation changes).
    aura::Window* active_window = activation_client->GetActiveWindow();
    if (active_window)
      activation_client->DeactivateWindow(active_window);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, internal::NativeWidgetPrivate implementation:

void DesktopNativeWidgetAura::InitNativeWidget(
    const Widget::InitParams& params) {
  ownership_ = params.ownership;
  widget_type_ = params.type;

  NativeWidgetAura::RegisterNativeWidgetForWindow(this, content_window_);
  // Animations on TYPE_WINDOW are handled by the OS. Additionally if we animate
  // these windows the size of the window gets augmented, effecting restore
  // bounds and maximized windows in bad ways.
  if (params.type == Widget::InitParams::TYPE_WINDOW &&
      !params.remove_standard_frame) {
    content_window_->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }
  content_window_->SetType(GetAuraWindowTypeForWidgetType(params.type));
  content_window_->Init(params.layer_type);
  wm::SetShadowType(content_window_, wm::SHADOW_TYPE_NONE);

  content_window_container_ = new aura::Window(NULL);
  content_window_container_->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  content_window_container_->Show();
  content_window_container_->AddChild(content_window_);

  desktop_window_tree_host_ = params.desktop_window_tree_host ?
      params.desktop_window_tree_host :
      DesktopWindowTreeHost::Create(native_widget_delegate_, this);
  host_.reset(desktop_window_tree_host_->AsWindowTreeHost());
  desktop_window_tree_host_->Init(content_window_, params);

  // Mark this window as Desktop root window.
  host_->window()->SetProperty(views::kDesktopRootWindow, true);

  host_->InitHost();
  host_->window()->AddChild(content_window_container_);
  host_->window()->SetProperty(kDesktopNativeWidgetAuraKey, this);

  host_->window()->AddObserver(new RootWindowDestructionObserver(this));

  // The WindowsModalityController event filter should be at the head of the
  // pre target handlers list. This ensures that it handles input events first
  // when modal windows are at the top of the Zorder.
  if (widget_type_ == Widget::InitParams::TYPE_WINDOW)
    window_modality_controller_.reset(
        new wm::WindowModalityController(host_->window()));

  // |root_window_event_filter_| must be created before
  // OnWindowTreeHostCreated() is invoked.

  // CEF sets focus to the window the user clicks down on.
  // TODO(beng): see if we can't do this some other way. CEF seems a heavy-
  //             handed way of accomplishing focus.
  // No event filter for aura::Env. Create CompoundEventFilter per
  // WindowEventDispatcher.
  root_window_event_filter_.reset(new wm::CompoundEventFilter);
  host_->window()->AddPreTargetHandler(root_window_event_filter_.get());

  // The host's dispatcher must be added to |native_cursor_manager_| before
  // OnNativeWidgetCreated() is called.
  cursor_reference_count_++;
  if (!native_cursor_manager_) {
    native_cursor_manager_ = new DesktopNativeCursorManager(
        DesktopCursorLoaderUpdater::Create());
  }
  if (!cursor_manager_) {
    cursor_manager_ = new wm::CursorManager(
        scoped_ptr<wm::NativeCursorManager>(native_cursor_manager_));
  }
  native_cursor_manager_->AddHost(host());
  aura::client::SetCursorClient(host_->window(), cursor_manager_);

  desktop_window_tree_host_->OnNativeWidgetCreated(params);

  UpdateWindowTransparency();

  capture_client_.reset(new DesktopCaptureClient(host_->window()));

  wm::FocusController* focus_controller =
      new wm::FocusController(new DesktopFocusRules(content_window_));
  focus_client_.reset(focus_controller);
  aura::client::SetFocusClient(host_->window(), focus_controller);
  aura::client::SetActivationClient(host_->window(), focus_controller);
  host_->window()->AddPreTargetHandler(focus_controller);

  dispatcher_client_.reset(new DesktopDispatcherClient);
  aura::client::SetDispatcherClient(host_->window(),
                                    dispatcher_client_.get());

  position_client_.reset(new DesktopScreenPositionClient(host_->window()));

  InstallInputMethodEventFilter();

  drag_drop_client_ = desktop_window_tree_host_->CreateDragDropClient(
      native_cursor_manager_);
  aura::client::SetDragDropClient(host_->window(),
                                  drag_drop_client_.get());

  static_cast<aura::client::FocusClient*>(focus_client_.get())->
      FocusWindow(content_window_);

  OnHostResized(host());

  host_->AddObserver(this);

  window_tree_client_.reset(
      new DesktopNativeWidgetAuraWindowTreeClient(host_->window()));
  drop_helper_.reset(new DropHelper(GetWidget()->GetRootView()));
  aura::client::SetDragDropDelegate(content_window_, this);

  if (params.type != Widget::InitParams::TYPE_TOOLTIP) {
    tooltip_manager_.reset(new TooltipManagerAura(GetWidget()));
    tooltip_controller_.reset(
        new corewm::TooltipController(
            desktop_window_tree_host_->CreateTooltip()));
    aura::client::SetTooltipClient(host_->window(),
                                   tooltip_controller_.get());
    host_->window()->AddPreTargetHandler(tooltip_controller_.get());
  }

  if (params.opacity == Widget::InitParams::TRANSLUCENT_WINDOW) {
    visibility_controller_.reset(new wm::VisibilityController);
    aura::client::SetVisibilityClient(host_->window(),
                                      visibility_controller_.get());
    wm::SetChildWindowVisibilityChangesAnimated(host_->window());
    wm::SetChildWindowVisibilityChangesAnimated(
        content_window_container_);
  }

  if (params.type == Widget::InitParams::TYPE_WINDOW) {
    focus_manager_event_handler_.reset(new FocusManagerEventHandler(this));
    host_->window()->AddPreTargetHandler(focus_manager_event_handler_.get());
  }

  event_client_.reset(new DesktopEventClient);
  aura::client::SetEventClient(host_->window(), event_client_.get());

  aura::client::GetFocusClient(content_window_)->FocusWindow(content_window_);

  aura::client::SetActivationDelegate(content_window_, this);

  shadow_controller_.reset(new wm::ShadowController(
      aura::client::GetActivationClient(host_->window())));

  content_window_->SetProperty(aura::client::kCanMaximizeKey,
                               GetWidget()->widget_delegate()->CanMaximize());
  content_window_->SetProperty(aura::client::kCanResizeKey,
                               GetWidget()->widget_delegate()->CanResize());

  window_reorderer_.reset(new WindowReorderer(content_window_,
      GetWidget()->GetRootView()));
}

NonClientFrameView* DesktopNativeWidgetAura::CreateNonClientFrameView() {
  return ShouldUseNativeFrame() ? new NativeFrameView(GetWidget()) : NULL;
}

bool DesktopNativeWidgetAura::ShouldUseNativeFrame() const {
  return desktop_window_tree_host_->ShouldUseNativeFrame();
}

bool DesktopNativeWidgetAura::ShouldWindowContentsBeTransparent() const {
  return desktop_window_tree_host_->ShouldWindowContentsBeTransparent();
}

void DesktopNativeWidgetAura::FrameTypeChanged() {
  desktop_window_tree_host_->FrameTypeChanged();
  UpdateWindowTransparency();
}

Widget* DesktopNativeWidgetAura::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* DesktopNativeWidgetAura::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView DesktopNativeWidgetAura::GetNativeView() const {
  return content_window_;
}

gfx::NativeWindow DesktopNativeWidgetAura::GetNativeWindow() const {
  return content_window_;
}

Widget* DesktopNativeWidgetAura::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* DesktopNativeWidgetAura::GetCompositor() const {
  return content_window_ ? content_window_->layer()->GetCompositor() : NULL;
}

ui::Compositor* DesktopNativeWidgetAura::GetCompositor() {
  return const_cast<ui::Compositor*>(
      const_cast<const DesktopNativeWidgetAura*>(this)->GetCompositor());
}

ui::Layer* DesktopNativeWidgetAura::GetLayer() {
  return content_window_ ? content_window_->layer() : NULL;
}

void DesktopNativeWidgetAura::ReorderNativeViews() {
  window_reorderer_->ReorderChildWindows();
}

void DesktopNativeWidgetAura::ViewRemoved(View* view) {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->ResetTargetViewIfEquals(view);
}

void DesktopNativeWidgetAura::SetNativeWindowProperty(const char* name,
                                                      void* value) {
  if (content_window_)
    content_window_->SetNativeWindowProperty(name, value);
}

void* DesktopNativeWidgetAura::GetNativeWindowProperty(const char* name) const {
  return content_window_ ?
      content_window_->GetNativeWindowProperty(name) : NULL;
}

TooltipManager* DesktopNativeWidgetAura::GetTooltipManager() const {
  return tooltip_manager_.get();
}

void DesktopNativeWidgetAura::SetCapture() {
  if (!content_window_)
    return;

  content_window_->SetCapture();
}

void DesktopNativeWidgetAura::ReleaseCapture() {
  if (!content_window_)
    return;

  content_window_->ReleaseCapture();
}

bool DesktopNativeWidgetAura::HasCapture() const {
  return content_window_ && content_window_->HasCapture() &&
      desktop_window_tree_host_->HasCapture();
}

InputMethod* DesktopNativeWidgetAura::CreateInputMethod() {
  if (switches::IsTextInputFocusManagerEnabled())
    return new NullInputMethod();

  ui::InputMethod* host = input_method_event_filter_->input_method();
  return new InputMethodBridge(this, host, false);
}

internal::InputMethodDelegate*
    DesktopNativeWidgetAura::GetInputMethodDelegate() {
  return this;
}

ui::InputMethod* DesktopNativeWidgetAura::GetHostInputMethod() {
  return input_method_event_filter_->input_method();
}

void DesktopNativeWidgetAura::CenterWindow(const gfx::Size& size) {
  if (content_window_)
    desktop_window_tree_host_->CenterWindow(size);
}

void DesktopNativeWidgetAura::GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const {
  if (content_window_)
    desktop_window_tree_host_->GetWindowPlacement(bounds, maximized);
}

bool DesktopNativeWidgetAura::SetWindowTitle(const base::string16& title) {
  if (!content_window_)
    return false;
  return desktop_window_tree_host_->SetWindowTitle(title);
}

void DesktopNativeWidgetAura::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                             const gfx::ImageSkia& app_icon) {
  if (content_window_)
    desktop_window_tree_host_->SetWindowIcons(window_icon, app_icon);
}

void DesktopNativeWidgetAura::InitModalType(ui::ModalType modal_type) {
  // 99% of the time, we should not be asked to create a
  // DesktopNativeWidgetAura that is modal. We only support window modal
  // dialogs on the same lines as non AURA.
  desktop_window_tree_host_->InitModalType(modal_type);
}

gfx::Rect DesktopNativeWidgetAura::GetWindowBoundsInScreen() const {
  return content_window_ ?
      desktop_window_tree_host_->GetWindowBoundsInScreen() : gfx::Rect();
}

gfx::Rect DesktopNativeWidgetAura::GetClientAreaBoundsInScreen() const {
  return content_window_ ?
      desktop_window_tree_host_->GetClientAreaBoundsInScreen() : gfx::Rect();
}

gfx::Rect DesktopNativeWidgetAura::GetRestoredBounds() const {
  return content_window_ ?
      desktop_window_tree_host_->GetRestoredBounds() : gfx::Rect();
}

void DesktopNativeWidgetAura::SetBounds(const gfx::Rect& bounds) {
  if (!content_window_)
    return;
  // TODO(ananta)
  // This code by default scales the bounds rectangle by 1.
  // We could probably get rid of this and similar logic from
  // the DesktopNativeWidgetAura::OnWindowTreeHostResized function.
  float scale = 1;
  aura::Window* root = host_->window();
  if (root) {
    scale = gfx::Screen::GetScreenFor(root)->
        GetDisplayNearestWindow(root).device_scale_factor();
  }
  gfx::Rect bounds_in_pixels =
    gfx::ScaleToEnclosingRect(bounds, scale, scale);
  desktop_window_tree_host_->AsWindowTreeHost()->SetBounds(bounds_in_pixels);
}

void DesktopNativeWidgetAura::SetSize(const gfx::Size& size) {
  if (content_window_)
    desktop_window_tree_host_->SetSize(size);
}

void DesktopNativeWidgetAura::StackAbove(gfx::NativeView native_view) {
}

void DesktopNativeWidgetAura::StackAtTop() {
  if (content_window_)
    desktop_window_tree_host_->StackAtTop();
}

void DesktopNativeWidgetAura::StackBelow(gfx::NativeView native_view) {
}

void DesktopNativeWidgetAura::SetShape(gfx::NativeRegion shape) {
  if (content_window_)
    desktop_window_tree_host_->SetShape(shape);
}

void DesktopNativeWidgetAura::Close() {
  if (!content_window_)
    return;

  content_window_->SuppressPaint();
  content_window_->Hide();

  desktop_window_tree_host_->Close();
}

void DesktopNativeWidgetAura::CloseNow() {
  if (content_window_)
    desktop_window_tree_host_->CloseNow();
}

void DesktopNativeWidgetAura::Show() {
  if (!content_window_)
    return;
  desktop_window_tree_host_->AsWindowTreeHost()->Show();
  content_window_->Show();
}

void DesktopNativeWidgetAura::Hide() {
  if (!content_window_)
    return;
  desktop_window_tree_host_->AsWindowTreeHost()->Hide();
  content_window_->Hide();
}

void DesktopNativeWidgetAura::ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) {
  if (!content_window_)
    return;
  desktop_window_tree_host_->ShowMaximizedWithBounds(restored_bounds);
  content_window_->Show();
}

void DesktopNativeWidgetAura::ShowWithWindowState(ui::WindowShowState state) {
  if (!content_window_)
    return;
  desktop_window_tree_host_->ShowWindowWithState(state);
  content_window_->Show();
}

bool DesktopNativeWidgetAura::IsVisible() const {
  return content_window_ && desktop_window_tree_host_->IsVisible();
}

void DesktopNativeWidgetAura::Activate() {
  if (content_window_)
    desktop_window_tree_host_->Activate();
}

void DesktopNativeWidgetAura::Deactivate() {
  if (content_window_)
    desktop_window_tree_host_->Deactivate();
}

bool DesktopNativeWidgetAura::IsActive() const {
  return content_window_ && desktop_window_tree_host_->IsActive();
}

void DesktopNativeWidgetAura::SetAlwaysOnTop(bool always_on_top) {
  if (content_window_)
    desktop_window_tree_host_->SetAlwaysOnTop(always_on_top);
}

bool DesktopNativeWidgetAura::IsAlwaysOnTop() const {
  return content_window_ && desktop_window_tree_host_->IsAlwaysOnTop();
}

void DesktopNativeWidgetAura::SetVisibleOnAllWorkspaces(bool always_visible) {
  if (content_window_)
    desktop_window_tree_host_->SetVisibleOnAllWorkspaces(always_visible);
}

void DesktopNativeWidgetAura::Maximize() {
  if (content_window_)
    desktop_window_tree_host_->Maximize();
}

void DesktopNativeWidgetAura::Minimize() {
  if (content_window_)
    desktop_window_tree_host_->Minimize();
}

bool DesktopNativeWidgetAura::IsMaximized() const {
  return content_window_ && desktop_window_tree_host_->IsMaximized();
}

bool DesktopNativeWidgetAura::IsMinimized() const {
  return content_window_ && desktop_window_tree_host_->IsMinimized();
}

void DesktopNativeWidgetAura::Restore() {
  if (content_window_)
    desktop_window_tree_host_->Restore();
}

void DesktopNativeWidgetAura::SetFullscreen(bool fullscreen) {
  if (content_window_)
    desktop_window_tree_host_->SetFullscreen(fullscreen);
}

bool DesktopNativeWidgetAura::IsFullscreen() const {
  return content_window_ && desktop_window_tree_host_->IsFullscreen();
}

void DesktopNativeWidgetAura::SetOpacity(unsigned char opacity) {
  if (content_window_)
    desktop_window_tree_host_->SetOpacity(opacity);
}

void DesktopNativeWidgetAura::SetUseDragFrame(bool use_drag_frame) {
}

void DesktopNativeWidgetAura::FlashFrame(bool flash_frame) {
  if (content_window_)
    desktop_window_tree_host_->FlashFrame(flash_frame);
}

void DesktopNativeWidgetAura::RunShellDrag(
    View* view,
    const ui::OSExchangeData& data,
    const gfx::Point& location,
    int operation,
    ui::DragDropTypes::DragEventSource source) {
  views::RunShellDrag(content_window_, data, location, operation, source);
}

void DesktopNativeWidgetAura::SchedulePaintInRect(const gfx::Rect& rect) {
  if (content_window_)
    content_window_->SchedulePaintInRect(rect);
}

void DesktopNativeWidgetAura::SetCursor(gfx::NativeCursor cursor) {
  cursor_ = cursor;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(host_->window());
  if (cursor_client)
    cursor_client->SetCursor(cursor);
}

bool DesktopNativeWidgetAura::IsMouseEventsEnabled() const {
  if (!content_window_)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(host_->window());
  return cursor_client ? cursor_client->IsMouseEventsEnabled() : true;
}

void DesktopNativeWidgetAura::ClearNativeFocus() {
  desktop_window_tree_host_->ClearNativeFocus();

  if (ShouldActivate()) {
    aura::client::GetFocusClient(content_window_)->
        ResetFocusWithinActiveWindow(content_window_);
  }
}

gfx::Rect DesktopNativeWidgetAura::GetWorkAreaBoundsInScreen() const {
  return desktop_window_tree_host_ ?
      desktop_window_tree_host_->GetWorkAreaBoundsInScreen() : gfx::Rect();
}

Widget::MoveLoopResult DesktopNativeWidgetAura::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  if (!content_window_)
    return Widget::MOVE_LOOP_CANCELED;
  return desktop_window_tree_host_->RunMoveLoop(drag_offset, source,
                                                escape_behavior);
}

void DesktopNativeWidgetAura::EndMoveLoop() {
  if (content_window_)
    desktop_window_tree_host_->EndMoveLoop();
}

void DesktopNativeWidgetAura::SetVisibilityChangedAnimationsEnabled(
    bool value) {
  if (content_window_)
    desktop_window_tree_host_->SetVisibilityChangedAnimationsEnabled(value);
}

ui::NativeTheme* DesktopNativeWidgetAura::GetNativeTheme() const {
  return DesktopWindowTreeHost::GetNativeTheme(content_window_);
}

void DesktopNativeWidgetAura::OnRootViewLayout() const {
  if (content_window_)
    desktop_window_tree_host_->OnRootViewLayout();
}

bool DesktopNativeWidgetAura::IsTranslucentWindowOpacitySupported() const {
  return content_window_ &&
      desktop_window_tree_host_->IsTranslucentWindowOpacitySupported();
}

void DesktopNativeWidgetAura::RepostNativeEvent(gfx::NativeEvent native_event) {
  OnEvent(native_event);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowDelegate implementation:

gfx::Size DesktopNativeWidgetAura::GetMinimumSize() const {
  return native_widget_delegate_->GetMinimumSize();
}

gfx::Size DesktopNativeWidgetAura::GetMaximumSize() const {
  return native_widget_delegate_->GetMaximumSize();
}

gfx::NativeCursor DesktopNativeWidgetAura::GetCursor(const gfx::Point& point) {
  return cursor_;
}

int DesktopNativeWidgetAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return native_widget_delegate_->GetNonClientComponent(point);
}

bool DesktopNativeWidgetAura::ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) {
  views::WidgetDelegate* widget_delegate = GetWidget()->widget_delegate();
  return !widget_delegate ||
      widget_delegate->ShouldDescendIntoChildForEventHandling(child, location);
}

bool DesktopNativeWidgetAura::CanFocus() {
  return true;
}

void DesktopNativeWidgetAura::OnCaptureLost() {
  native_widget_delegate_->OnMouseCaptureLost();
}

void DesktopNativeWidgetAura::OnPaint(gfx::Canvas* canvas) {
  native_widget_delegate_->OnNativeWidgetPaint(canvas);
}

void DesktopNativeWidgetAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopNativeWidgetAura::OnWindowDestroying(aura::Window* window) {
  // Cleanup happens in OnHostClosed().
}

void DesktopNativeWidgetAura::OnWindowDestroyed(aura::Window* window) {
  // Cleanup happens in OnHostClosed(). We own |content_window_| (indirectly by
  // way of |dispatcher_|) so there should be no need to do any processing
  // here.
}

void DesktopNativeWidgetAura::OnWindowTargetVisibilityChanged(bool visible) {
}

bool DesktopNativeWidgetAura::HasHitTestMask() const {
  return native_widget_delegate_->HasHitTestMask();
}

void DesktopNativeWidgetAura::GetHitTestMask(gfx::Path* mask) const {
  native_widget_delegate_->GetHitTestMask(mask);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, ui::EventHandler implementation:

void DesktopNativeWidgetAura::OnKeyEvent(ui::KeyEvent* event) {
  if (event->is_char()) {
    // If a ui::InputMethod object is attached to the root window, character
    // events are handled inside the object and are not passed to this function.
    // If such object is not attached, character events might be sent (e.g. on
    // Windows). In this case, we just skip these.
    return;
  }
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!content_window_->IsVisible())
    return;

  native_widget_delegate_->OnKeyEvent(event);
  if (event->handled())
    return;

  if (GetWidget()->HasFocusManager() &&
      !GetWidget()->GetFocusManager()->OnKeyEvent(*event))
    event->SetHandled();
}

void DesktopNativeWidgetAura::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(content_window_->IsVisible());
  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  TooltipManagerAura::UpdateTooltipManagerForCapture(GetWidget());
  native_widget_delegate_->OnMouseEvent(event);
  // WARNING: we may have been deleted.
}

void DesktopNativeWidgetAura::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL) {
    native_widget_delegate_->OnScrollEvent(event);
    if (event->handled())
      return;

    // Convert unprocessed scroll events into wheel events.
    ui::MouseWheelEvent mwe(*static_cast<ui::ScrollEvent*>(event));
    native_widget_delegate_->OnMouseEvent(&mwe);
    if (mwe.handled())
      event->SetHandled();
  } else {
    native_widget_delegate_->OnScrollEvent(event);
  }
}

void DesktopNativeWidgetAura::OnGestureEvent(ui::GestureEvent* event) {
  native_widget_delegate_->OnGestureEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::ActivationDelegate implementation:

bool DesktopNativeWidgetAura::ShouldActivate() const {
  return native_widget_delegate_->CanActivate();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::ActivationChangeObserver
//    implementation:

void DesktopNativeWidgetAura::OnWindowActivated(aura::Window* gained_active,
                                                aura::Window* lost_active) {
  DCHECK(content_window_ == gained_active || content_window_ == lost_active);
  if (gained_active == content_window_ && restore_focus_on_activate_) {
    restore_focus_on_activate_ = false;
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  } else if (lost_active == content_window_ && GetWidget()->HasFocusManager()) {
    DCHECK(!restore_focus_on_activate_);
    restore_focus_on_activate_ = true;
    // Pass in false so that ClearNativeFocus() isn't invoked.
    GetWidget()->GetFocusManager()->StoreFocusedView(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::client::FocusChangeObserver implementation:

void DesktopNativeWidgetAura::OnWindowFocused(aura::Window* gained_focus,
                                              aura::Window* lost_focus) {
  if (content_window_ == gained_focus) {
    desktop_window_tree_host_->OnNativeWidgetFocus();
    native_widget_delegate_->OnNativeFocus(lost_focus);

    // If focus is moving from a descendant Window to |content_window_| then
    // native activation hasn't changed. Still, the InputMethod must be informed
    // of the Window focus change.
    InputMethod* input_method = GetWidget()->GetInputMethod();
    if (input_method)
      input_method->OnFocus();
  } else if (content_window_ == lost_focus) {
    desktop_window_tree_host_->OnNativeWidgetBlur();
    native_widget_delegate_->OnNativeBlur(gained_focus);
  }
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, views::internal::InputMethodDelegate:

void DesktopNativeWidgetAura::DispatchKeyEventPostIME(const ui::KeyEvent& key) {
  FocusManager* focus_manager =
      native_widget_delegate_->AsWidget()->GetFocusManager();
  native_widget_delegate_->OnKeyEvent(const_cast<ui::KeyEvent*>(&key));
  if (key.handled() || !focus_manager)
    return;
  focus_manager->OnKeyEvent(key);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowDragDropDelegate implementation:

void DesktopNativeWidgetAura::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  last_drop_operation_ = drop_helper_->OnDragOver(event.data(),
      event.location(), event.source_operations());
}

int DesktopNativeWidgetAura::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  last_drop_operation_ = drop_helper_->OnDragOver(event.data(),
      event.location(), event.source_operations());
  return last_drop_operation_;
}

void DesktopNativeWidgetAura::OnDragExited() {
  DCHECK(drop_helper_.get() != NULL);
  drop_helper_->OnDragExit();
}

int DesktopNativeWidgetAura::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_.get() != NULL);
  Activate();
  return drop_helper_->OnDrop(event.data(), event.location(),
      last_drop_operation_);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, aura::WindowTreeHostObserver implementation:

void DesktopNativeWidgetAura::OnHostCloseRequested(
    const aura::WindowTreeHost* host) {
  GetWidget()->Close();
}

void DesktopNativeWidgetAura::OnHostResized(const aura::WindowTreeHost* host) {
  // Don't update the bounds of the child layers when animating closed. If we
  // did it would force a paint, which we don't want. We don't want the paint
  // as we can't assume any of the children are valid.
  if (desktop_window_tree_host_->IsAnimatingClosed())
    return;

  gfx::Rect new_bounds = gfx::Rect(host->window()->bounds().size());
  content_window_->SetBounds(new_bounds);
  // Can be NULL at start.
  if (content_window_container_)
    content_window_container_->SetBounds(new_bounds);
  native_widget_delegate_->OnNativeWidgetSizeChanged(new_bounds.size());
}

void DesktopNativeWidgetAura::OnHostMoved(const aura::WindowTreeHost* host,
                                          const gfx::Point& new_origin) {
  TRACE_EVENT1("views", "DesktopNativeWidgetAura::OnHostMoved",
               "new_origin", new_origin.ToString());

  native_widget_delegate_->OnNativeWidgetMove();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopNativeWidgetAura, private:

void DesktopNativeWidgetAura::InstallInputMethodEventFilter() {
  DCHECK(!input_method_event_filter_.get());

  input_method_event_filter_.reset(new wm::InputMethodEventFilter(
      host_->GetAcceleratedWidget()));
  input_method_event_filter_->SetInputMethodPropertyInRootWindow(
      host_->window());
  root_window_event_filter_->AddHandler(input_method_event_filter_.get());
}

void DesktopNativeWidgetAura::UpdateWindowTransparency() {
  content_window_->SetTransparent(
      desktop_window_tree_host_->ShouldWindowContentsBeTransparent());
  // Regardless of transparency or not, this root content window will always
  // fill its bounds completely, so set this flag to true to avoid an
  // unecessary clear before update.
  content_window_->SetFillsBoundsCompletely(true);
}

void DesktopNativeWidgetAura::RootWindowDestroyed() {
  cursor_reference_count_--;
  if (cursor_reference_count_ == 0) {
    // We are the last DesktopNativeWidgetAura instance, and we are responsible
    // for cleaning up |cursor_manager_|.
    delete cursor_manager_;
    native_cursor_manager_ = NULL;
    cursor_manager_ = NULL;
  }
}

}  // namespace views
