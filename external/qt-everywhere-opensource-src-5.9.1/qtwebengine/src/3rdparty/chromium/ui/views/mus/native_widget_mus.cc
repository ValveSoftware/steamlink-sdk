// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This has to be before any other includes, else default is picked up.
// See base/logging for details on this.
#define NOTIMPLEMENTED_POLICY 5

#include "ui/views/mus/native_widget_mus.h"

#include <map>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/interfaces/cursor.mojom.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_aura.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/drag_utils.h"
#include "ui/views/mus/drag_drop_client_mus.h"
#include "ui/views/mus/drop_target_mus.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/mus/window_manager_constants_converters.h"
#include "ui/views/mus/window_manager_frame_values.h"
#include "ui/views/mus/window_tree_host_mus.h"
#include "ui/views/widget/drop_helper.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/tooltip_manager_aura.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/custom_frame_view.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/default_screen_position_client.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/native_cursor_manager.h"

DECLARE_WINDOW_PROPERTY_TYPE(ui::Window*);

using ui::mojom::EventResult;

namespace views {
namespace {

DEFINE_WINDOW_PROPERTY_KEY(ui::Window*, kMusWindow, nullptr);

MUS_DEFINE_WINDOW_PROPERTY_KEY(NativeWidgetMus*, kNativeWidgetMusKey, nullptr);

// This ensures that only the top-level aura Window can be activated.
class FocusRulesImpl : public wm::BaseFocusRules {
 public:
  explicit FocusRulesImpl(aura::Window* root) : root_(root) {}
  ~FocusRulesImpl() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return root_ == window;
  }

 private:
  aura::Window* root_;

  DISALLOW_COPY_AND_ASSIGN(FocusRulesImpl);
};

// This makes sure that an aura::Window focused (or activated) through the
// aura::client::FocusClient (or ActivationClient) focuses (or activates) the
// corresponding ui::Window too.
class FocusControllerMus : public wm::FocusController {
 public:
  explicit FocusControllerMus(wm::FocusRules* rules) : FocusController(rules) {}
  ~FocusControllerMus() override {}

 private:
  void FocusWindow(aura::Window* window) override {
    FocusController::FocusWindow(window);
    if (window) {
      ui::Window* mus_window = window->GetRootWindow()->GetProperty(kMusWindow);
      if (mus_window)
        mus_window->SetFocus();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerMus);
};

class ContentWindowLayoutManager : public aura::LayoutManager {
 public:
  ContentWindowLayoutManager(aura::Window* outer, aura::Window* inner)
      : outer_(outer), inner_(inner) {}
  ~ContentWindowLayoutManager() override {}

 private:
  // aura::LayoutManager:
  void OnWindowResized() override { inner_->SetBounds(outer_->bounds()); }
  void OnWindowAddedToLayout(aura::Window* child) override {
    OnWindowResized();
  }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* outer_;
  aura::Window* inner_;

  DISALLOW_COPY_AND_ASSIGN(ContentWindowLayoutManager);
};

class NativeWidgetMusWindowParentingClient
    : public aura::client::WindowParentingClient {
 public:
  explicit NativeWidgetMusWindowParentingClient(aura::Window* root_window)
      : root_window_(root_window) {
    aura::client::SetWindowParentingClient(root_window_, this);
  }
  ~NativeWidgetMusWindowParentingClient() override {
    aura::client::SetWindowParentingClient(root_window_, nullptr);
  }

  // Overridden from client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* context,
                                 aura::Window* window,
                                 const gfx::Rect& bounds) override {
    return root_window_;
  }

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMusWindowParentingClient);
};

// A screen position client that applies the offset of the ui::Window.
class ScreenPositionClientMus : public wm::DefaultScreenPositionClient {
 public:
  explicit ScreenPositionClientMus(ui::Window* mus_window)
      : mus_window_(mus_window) {}
  ~ScreenPositionClientMus() override {}

  // wm::DefaultScreenPositionClient:
  void ConvertPointToScreen(const aura::Window* window,
                            gfx::Point* point) override {
    wm::DefaultScreenPositionClient::ConvertPointToScreen(window, point);
    gfx::Rect mus_bounds = mus_window_->GetBoundsInRoot();
    point->Offset(-mus_bounds.x(), -mus_bounds.y());
  }
  void ConvertPointFromScreen(const aura::Window* window,
                              gfx::Point* point) override {
    gfx::Rect mus_bounds = mus_window_->GetBoundsInRoot();
    point->Offset(mus_bounds.x(), mus_bounds.y());
    wm::DefaultScreenPositionClient::ConvertPointFromScreen(window, point);
  }

 private:
  ui::Window* mus_window_;

  DISALLOW_COPY_AND_ASSIGN(ScreenPositionClientMus);
};

class NativeCursorManagerMus : public wm::NativeCursorManager {
 public:
  explicit NativeCursorManagerMus(ui::Window* mus_window)
      : mus_window_(mus_window) {}
  ~NativeCursorManagerMus() override {}

  // wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    // We ignore this entirely, as cursor are set on the client.
  }

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    mus_window_->SetPredefinedCursor(ui::mojom::Cursor(cursor.native_type()));
    delegate->CommitCursor(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);

    if (visible)
      SetCursor(delegate->GetCursor(), delegate);
    else
      mus_window_->SetPredefinedCursor(ui::mojom::Cursor::NONE);
  }

  void SetCursorSet(ui::CursorSetType cursor_set,
                    wm::NativeCursorManagerDelegate* delegate) override {
    // TODO(erg): For now, ignore the difference between SET_NORMAL and
    // SET_LARGE here. This feels like a thing that mus should decide instead.
    //
    // Also, it's NOTIMPLEMENTED() in the desktop version!? Including not
    // acknowledging the call in the delegate.
    NOTIMPLEMENTED();
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    // TODO(erg): How do we actually implement this?
    //
    // Mouse event dispatch is potentially done in a different process,
    // definitely in a different mojo service. Each app is fairly locked down.
    delegate->CommitMouseEventsEnabled(enabled);
    NOTIMPLEMENTED();
  }

 private:
  ui::Window* mus_window_;

  DISALLOW_COPY_AND_ASSIGN(NativeCursorManagerMus);
};

// As the window manager renderers the non-client decorations this class does
// very little but honor the client area insets from the window manager.
class ClientSideNonClientFrameView : public NonClientFrameView {
 public:
  explicit ClientSideNonClientFrameView(views::Widget* widget)
      : widget_(widget) {}
  ~ClientSideNonClientFrameView() override {}

 private:
  // Returns the default values of client area insets from the window manager.
  static gfx::Insets GetDefaultWindowManagerInsets(bool is_maximized) {
    const WindowManagerFrameValues& values =
        WindowManagerFrameValues::instance();
    return is_maximized ? values.maximized_insets : values.normal_insets;
  }

  // NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override {
    gfx::Rect result(GetLocalBounds());
    if (widget_->IsFullscreen())
      return result;
    result.Inset(GetDefaultWindowManagerInsets(widget_->IsMaximized()));
    return result;
  }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    if (widget_->IsFullscreen())
      return client_bounds;

    const gfx::Insets insets(
        GetDefaultWindowManagerInsets(widget_->IsMaximized()));
    return gfx::Rect(client_bounds.x() - insets.left(),
                     client_bounds.y() - insets.top(),
                     client_bounds.width() + insets.width(),
                     client_bounds.height() + insets.height());
  }
  int NonClientHitTest(const gfx::Point& point) override { return HTNOWHERE; }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {
    // The window manager provides the shape; do nothing.
  }
  void ResetWindowControls() override {
    // TODO(sky): push to wm?
  }

  // These have no implementation. The Window Manager handles the actual
  // rendering of the icon/title. See NonClientFrameViewMash. The values
  // associated with these methods are pushed to the server by the way of
  // NativeWidgetMus functions.
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  gfx::Size GetPreferredSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetPreferredSize()))
        .size();
  }
  gfx::Size GetMinimumSize() const override {
    return widget_->non_client_view()
        ->GetWindowBoundsForClientBounds(
            gfx::Rect(widget_->client_view()->GetMinimumSize()))
        .size();
  }
  gfx::Size GetMaximumSize() const override {
    gfx::Size max_size = widget_->client_view()->GetMaximumSize();
    gfx::Size converted_size =
        widget_->non_client_view()
            ->GetWindowBoundsForClientBounds(gfx::Rect(max_size))
            .size();
    return gfx::Size(max_size.width() == 0 ? 0 : converted_size.width(),
                     max_size.height() == 0 ? 0 : converted_size.height());
  }

  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideNonClientFrameView);
};

int ResizeBehaviorFromDelegate(WidgetDelegate* delegate) {
  if (!delegate)
    return ui::mojom::kResizeBehaviorNone;

  int32_t behavior = ui::mojom::kResizeBehaviorNone;
  if (delegate->CanResize())
    behavior |= ui::mojom::kResizeBehaviorCanResize;
  if (delegate->CanMaximize())
    behavior |= ui::mojom::kResizeBehaviorCanMaximize;
  if (delegate->CanMinimize())
    behavior |= ui::mojom::kResizeBehaviorCanMinimize;
  return behavior;
}

// Returns the 1x window app icon or an empty SkBitmap if no icon is available.
// TODO(jamescook): Support other scale factors.
SkBitmap AppIconFromDelegate(WidgetDelegate* delegate) {
  if (!delegate)
    return SkBitmap();
  gfx::ImageSkia app_icon = delegate->GetWindowAppIcon();
  if (app_icon.isNull())
    return SkBitmap();
  return app_icon.GetRepresentation(1.f).sk_bitmap();
}

// Handles acknowledgment of an input event, either immediately when a nested
// message loop starts, or upon destruction.
class EventAckHandler : public base::MessageLoop::NestingObserver {
 public:
  explicit EventAckHandler(
      std::unique_ptr<base::Callback<void(EventResult)>> ack_callback)
      : ack_callback_(std::move(ack_callback)) {
    DCHECK(ack_callback_);
    base::MessageLoop::current()->AddNestingObserver(this);
  }

  ~EventAckHandler() override {
    base::MessageLoop::current()->RemoveNestingObserver(this);
    if (ack_callback_) {
      ack_callback_->Run(handled_ ? EventResult::HANDLED
                                  : EventResult::UNHANDLED);
    }
  }

  void set_handled(bool handled) { handled_ = handled; }

  // base::MessageLoop::NestingObserver:
  void OnBeginNestedMessageLoop() override {
    // Acknowledge the event immediately if a nested message loop starts.
    // Otherwise we appear unresponsive for the life of the nested message loop.
    if (ack_callback_) {
      ack_callback_->Run(EventResult::HANDLED);
      ack_callback_.reset();
    }
  }

 private:
  std::unique_ptr<base::Callback<void(EventResult)>> ack_callback_;
  bool handled_ = false;

  DISALLOW_COPY_AND_ASSIGN(EventAckHandler);
};

void OnMoveLoopEnd(bool* out_success,
                   base::Closure quit_closure,
                   bool in_success) {
  *out_success = in_success;
  quit_closure.Run();
}

ui::mojom::ShowState GetShowState(const ui::Window* window) {
  if (!window ||
      !window->HasSharedProperty(
          ui::mojom::WindowManager::kShowState_Property)) {
    return ui::mojom::ShowState::DEFAULT;
  }

  return static_cast<ui::mojom::ShowState>(window->GetSharedProperty<int32_t>(
      ui::mojom::WindowManager::kShowState_Property));
}

}  // namespace

class NativeWidgetMus::MusWindowObserver : public ui::WindowObserver {
 public:
  explicit MusWindowObserver(NativeWidgetMus* native_widget_mus)
      : native_widget_mus_(native_widget_mus),
        show_state_(ui::mojom::ShowState::DEFAULT) {
    mus_window()->AddObserver(this);
  }

  ~MusWindowObserver() override {
    mus_window()->RemoveObserver(this);
  }

  // ui::WindowObserver:
  void OnWindowVisibilityChanging(ui::Window* window, bool visible) override {
    native_widget_mus_->OnMusWindowVisibilityChanging(window, visible);
  }
  void OnWindowVisibilityChanged(ui::Window* window, bool visible) override {
    native_widget_mus_->OnMusWindowVisibilityChanged(window, visible);
  }
  void OnWindowPredefinedCursorChanged(ui::Window* window,
                                       ui::mojom::Cursor cursor) override {
    DCHECK_EQ(window, mus_window());
    native_widget_mus_->set_last_cursor(cursor);
  }
  void OnWindowSharedPropertyChanged(
      ui::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    if (name != ui::mojom::WindowManager::kShowState_Property)
      return;
    const ui::mojom::ShowState show_state = GetShowState(window);
    if (show_state == show_state_)
      return;
    show_state_ = show_state;
    ui::PlatformWindowState state = ui::PLATFORM_WINDOW_STATE_UNKNOWN;
    switch (show_state_) {
      case ui::mojom::ShowState::MINIMIZED:
        state = ui::PLATFORM_WINDOW_STATE_MINIMIZED;
        break;
      case ui::mojom::ShowState::MAXIMIZED:
        state = ui::PLATFORM_WINDOW_STATE_MAXIMIZED;
        break;
      case ui::mojom::ShowState::DEFAULT:
      case ui::mojom::ShowState::INACTIVE:
      case ui::mojom::ShowState::NORMAL:
      case ui::mojom::ShowState::DOCKED:
        // TODO(sky): support docked.
        state = ui::PLATFORM_WINDOW_STATE_NORMAL;
        break;
      case ui::mojom::ShowState::FULLSCREEN:
        state = ui::PLATFORM_WINDOW_STATE_FULLSCREEN;
        break;
    }
    platform_window_delegate()->OnWindowStateChanged(state);
  }
  void OnWindowDestroyed(ui::Window* window) override {
    DCHECK_EQ(mus_window(), window);
    platform_window_delegate()->OnClosed();
  }
  void OnWindowBoundsChanging(ui::Window* window,
                              const gfx::Rect& old_bounds,
                              const gfx::Rect& new_bounds) override {
    DCHECK_EQ(window, mus_window());
    window_tree_host()->SetBounds(new_bounds);
  }
  void OnWindowFocusChanged(ui::Window* gained_focus,
                            ui::Window* lost_focus) override {
    if (gained_focus == mus_window())
      platform_window_delegate()->OnActivationChanged(true);
    else if (lost_focus == mus_window())
      platform_window_delegate()->OnActivationChanged(false);
  }
  void OnRequestClose(ui::Window* window) override {
    platform_window_delegate()->OnCloseRequest();
  }

 private:
  ui::Window* mus_window() { return native_widget_mus_->window(); }
  WindowTreeHostMus* window_tree_host() {
    return native_widget_mus_->window_tree_host();
  }
  ui::PlatformWindowDelegate* platform_window_delegate() {
    return native_widget_mus_->window_tree_host();
  }

  NativeWidgetMus* native_widget_mus_;
  ui::mojom::ShowState show_state_;

  DISALLOW_COPY_AND_ASSIGN(MusWindowObserver);
};

class NativeWidgetMus::MusCaptureClient
    : public aura::client::DefaultCaptureClient {
 public:
  MusCaptureClient(aura::Window* root_window,
                   aura::Window* aura_window,
                   ui::Window* mus_window)
      : aura::client::DefaultCaptureClient(root_window),
        aura_window_(aura_window),
        mus_window_(mus_window) {}
  ~MusCaptureClient() override {}

  // aura::client::DefaultCaptureClient:
  void SetCapture(aura::Window* window) override {
    aura::client::DefaultCaptureClient::SetCapture(window);
    if (aura_window_ == window)
      mus_window_->SetCapture();
  }
  void ReleaseCapture(aura::Window* window) override {
    aura::client::DefaultCaptureClient::ReleaseCapture(window);
    if (aura_window_ == window)
      mus_window_->ReleaseCapture();
  }

 private:
  aura::Window* aura_window_;
  ui::Window* mus_window_;

  DISALLOW_COPY_AND_ASSIGN(MusCaptureClient);
};

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, public:

NativeWidgetMus::NativeWidgetMus(
    internal::NativeWidgetDelegate* delegate,
    ui::Window* window,
    ui::mojom::CompositorFrameSinkType compositor_frame_sink_type)
    : window_(window),
      last_cursor_(ui::mojom::Cursor::CURSOR_NULL),
      native_widget_delegate_(delegate),
      compositor_frame_sink_type_(compositor_frame_sink_type),
      show_state_before_fullscreen_(ui::mojom::ShowState::DEFAULT),
      ownership_(Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET),
      content_(new aura::Window(this)),
      last_drop_operation_(ui::DragDropTypes::DRAG_NONE),
      close_widget_factory_(this) {
  window_->set_input_event_handler(this);
  mus_window_observer_ = base::MakeUnique<MusWindowObserver>(this);

  // TODO(fsamuel): Figure out lifetime of |window_|.
  aura::SetMusWindow(content_, window_);
  window->SetLocalProperty(kNativeWidgetMusKey, this);
  window_tree_host_ = base::MakeUnique<WindowTreeHostMus>(this, window_);
  input_method_ =
      base::MakeUnique<InputMethodMus>(window_tree_host_.get(), window_);
  window_tree_host_->SetSharedInputMethod(input_method_.get());
}

NativeWidgetMus::~NativeWidgetMus() {
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET) {
    DCHECK(!window_);
    delete native_widget_delegate_;
  } else {
    if (window_)
      window_->set_input_event_handler(nullptr);
    CloseNow();
  }
}

// static
void NativeWidgetMus::NotifyFrameChanged(ui::WindowTreeClient* client) {
  for (ui::Window* window : client->GetRoots()) {
    NativeWidgetMus* native_widget =
        window->GetLocalProperty(kNativeWidgetMusKey);
    if (native_widget && native_widget->GetWidget()->non_client_view()) {
      native_widget->GetWidget()->non_client_view()->Layout();
      native_widget->GetWidget()->non_client_view()->SchedulePaint();
      native_widget->UpdateClientArea();
      native_widget->UpdateHitTestMask();
    }
  }
}

// static
NativeWidgetMus* NativeWidgetMus::GetForWindow(ui::Window* window) {
  DCHECK(window);
  NativeWidgetMus* native_widget =
      window->GetLocalProperty(kNativeWidgetMusKey);
  return native_widget;
}

// static
Widget* NativeWidgetMus::GetWidgetForWindow(ui::Window* window) {
  NativeWidgetMus* native_widget = GetForWindow(window);
  if (!native_widget)
    return nullptr;
  return native_widget->GetWidget();
}

aura::Window* NativeWidgetMus::GetRootWindow() {
  return window_tree_host_->window();
}

void NativeWidgetMus::OnPlatformWindowClosed() {
  native_widget_delegate_->OnNativeWidgetDestroying();

  tooltip_manager_.reset();
  if (tooltip_controller_.get()) {
    window_tree_host_->window()->RemovePreTargetHandler(
        tooltip_controller_.get());
    aura::client::SetTooltipClient(window_tree_host_->window(), NULL);
    tooltip_controller_.reset();
  }

  window_parenting_client_.reset();  // Uses |content_|.
  capture_client_.reset();      // Uses |content_|.

  window_tree_host_->RemoveObserver(this);
  window_tree_host_.reset();

  cursor_manager_.reset();  // Uses |window_|.

  mus_window_observer_.reset(nullptr);

  window_ = nullptr;
  content_ = nullptr;

  native_widget_delegate_->OnNativeWidgetDestroyed();
  if (ownership_ == Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
    delete this;
}

void NativeWidgetMus::OnActivationChanged(bool active) {
  if (!native_widget_delegate_)
    return;
  if (active) {
    native_widget_delegate_->OnNativeFocus();
    GetWidget()->GetFocusManager()->RestoreFocusedView();
  } else {
    native_widget_delegate_->OnNativeBlur();
    GetWidget()->GetFocusManager()->StoreFocusedView(true);
  }
  native_widget_delegate_->OnNativeWidgetActivationChanged(active);
}

void NativeWidgetMus::UpdateClientArea() {
  if (is_parallel_widget_in_window_manager())
    return;

  NonClientView* non_client_view =
      native_widget_delegate_->AsWidget()->non_client_view();
  if (!non_client_view || !non_client_view->client_view())
    return;

  const gfx::Rect client_area_rect(non_client_view->client_view()->bounds());
  window_->SetClientArea(gfx::Insets(
      client_area_rect.y(), client_area_rect.x(),
      non_client_view->bounds().height() - client_area_rect.bottom(),
      non_client_view->bounds().width() - client_area_rect.right()));
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, private:

// static
void NativeWidgetMus::ConfigurePropertiesForNewWindow(
    const Widget::InitParams& init_params,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  properties->insert(init_params.mus_properties.begin(),
                     init_params.mus_properties.end());
  if (!init_params.bounds.IsEmpty()) {
    (*properties)[ui::mojom::WindowManager::kUserSetBounds_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(init_params.bounds);
  }
  if (!init_params.name.empty()) {
    (*properties)[ui::mojom::WindowManager::kName_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(init_params.name);
  }
  (*properties)[ui::mojom::WindowManager::kAlwaysOnTop_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(init_params.keep_on_top);

  if (!Widget::RequiresNonClientView(init_params.type))
    return;

  (*properties)[ui::mojom::WindowManager::kWindowType_Property] =
      mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int32_t>(
          mojo::ConvertTo<ui::mojom::WindowType>(init_params.type)));
  if (init_params.delegate &&
      properties->count(ui::mojom::WindowManager::kResizeBehavior_Property) ==
          0) {
    (*properties)[ui::mojom::WindowManager::kResizeBehavior_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(
            ResizeBehaviorFromDelegate(init_params.delegate));
  }
  SkBitmap app_icon = AppIconFromDelegate(init_params.delegate);
  if (!app_icon.isNull()) {
    (*properties)[ui::mojom::WindowManager::kWindowAppIcon_Property] =
        mojo::ConvertTo<std::vector<uint8_t>>(app_icon);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, internal::NativeWidgetPrivate implementation:

NonClientFrameView* NativeWidgetMus::CreateNonClientFrameView() {
  return new ClientSideNonClientFrameView(GetWidget());
}

void NativeWidgetMus::InitNativeWidget(const Widget::InitParams& params) {
  NativeWidgetAura::RegisterNativeWidgetForWindow(this, content_);
  aura::Window* hosted_window = window_tree_host_->window();

  ownership_ = params.ownership;
  window_->SetCanFocus(params.activatable ==
                       Widget::InitParams::ACTIVATABLE_YES);
  window_->SetCanAcceptEvents(params.accept_events);

  window_tree_host_->AddObserver(this);
  window_tree_host_->InitHost();
  hosted_window->SetProperty(kMusWindow, window_);

  // TODO(moshayedi): crbug.com/641039. Investigate whether there are any cases
  // where we need input method but don't have the WindowManagerConnection here.
  if (WindowManagerConnection::Exists())
    input_method_->Init(WindowManagerConnection::Get()->connector());

  focus_client_ =
      base::MakeUnique<FocusControllerMus>(new FocusRulesImpl(hosted_window));

  aura::client::SetFocusClient(hosted_window, focus_client_.get());
  aura::client::SetActivationClient(hosted_window, focus_client_.get());
  screen_position_client_ = base::MakeUnique<ScreenPositionClientMus>(window_);
  aura::client::SetScreenPositionClient(hosted_window,
                                        screen_position_client_.get());

  drag_drop_client_ = base::MakeUnique<DragDropClientMus>(window_);
  aura::client::SetDragDropClient(hosted_window, drag_drop_client_.get());
  drop_target_ = base::MakeUnique<DropTargetMus>(content_);
  window_->SetCanAcceptDrops(drop_target_.get());
  drop_helper_ = base::MakeUnique<DropHelper>(GetWidget()->GetRootView());
  aura::client::SetDragDropDelegate(content_, this);

  if (params.type != Widget::InitParams::TYPE_TOOLTIP) {
    tooltip_manager_ = base::MakeUnique<TooltipManagerAura>(GetWidget());
    tooltip_controller_ = base::MakeUnique<corewm::TooltipController>(
        base::MakeUnique<corewm::TooltipAura>());
    aura::client::SetTooltipClient(window_tree_host_->window(),
                                   tooltip_controller_.get());
    window_tree_host_->window()->AddPreTargetHandler(tooltip_controller_.get());
  }

  // TODO(erg): Remove this check when ash/mus/move_event_handler.cc's
  // direct usage of ui::Window::SetPredefinedCursor() is switched to a
  // private method on WindowManagerClient.
  if (!is_parallel_widget_in_window_manager()) {
    cursor_manager_ = base::MakeUnique<wm::CursorManager>(
        base::MakeUnique<NativeCursorManagerMus>(window_));
    aura::client::SetCursorClient(hosted_window, cursor_manager_.get());
  }

  window_parenting_client_ =
      base::MakeUnique<NativeWidgetMusWindowParentingClient>(hosted_window);
  hosted_window->AddPreTargetHandler(focus_client_.get());
  hosted_window->SetLayoutManager(
      new ContentWindowLayoutManager(hosted_window, content_));
  capture_client_ =
      base::MakeUnique<MusCaptureClient>(hosted_window, content_, window_);

  content_->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  content_->Init(params.layer_type);
  if (window_->visible())
    content_->Show();
  content_->SetTransparent(true);
  content_->SetFillsBoundsCompletely(false);
  content_->set_ignore_events(!params.accept_events);
  hosted_window->AddChild(content_);

  ui::Window* parent_mus = params.parent_mus;

  // Set-up transiency if appropriate.
  if (params.parent && !params.child) {
    aura::Window* parent_root_aura = params.parent->GetRootWindow();
    ui::Window* parent_root_mus = parent_root_aura->GetProperty(kMusWindow);
    if (parent_root_mus) {
      parent_root_mus->AddTransientWindow(window_);
      if (!parent_mus)
        parent_mus = parent_root_mus;
    }
  }

  if (parent_mus)
    parent_mus->AddChild(window_);

  // TODO(sky): deal with show state.
  if (!params.bounds.size().IsEmpty())
    SetBounds(params.bounds);

  // TODO(beng): much else, see [Desktop]NativeWidgetAura.

  native_widget_delegate_->OnNativeWidgetCreated(false);
}

void NativeWidgetMus::OnWidgetInitDone() {
  // The client area is calculated from the NonClientView. During
  // InitNativeWidget() the NonClientView has not been created. When this
  // function is called the NonClientView has been created, so that we can
  // correctly calculate the client area and push it to the ui::Window.
  UpdateClientArea();
  UpdateHitTestMask();
}

bool NativeWidgetMus::ShouldUseNativeFrame() const {
  NOTIMPLEMENTED();
  return false;
}

bool NativeWidgetMus::ShouldWindowContentsBeTransparent() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::FrameTypeChanged() {
  NOTIMPLEMENTED();
}

Widget* NativeWidgetMus::GetWidget() {
  return native_widget_delegate_->AsWidget();
}

const Widget* NativeWidgetMus::GetWidget() const {
  return native_widget_delegate_->AsWidget();
}

gfx::NativeView NativeWidgetMus::GetNativeView() const {
  return content_;
}

gfx::NativeWindow NativeWidgetMus::GetNativeWindow() const {
  return content_;
}

Widget* NativeWidgetMus::GetTopLevelWidget() {
  return GetWidget();
}

const ui::Compositor* NativeWidgetMus::GetCompositor() const {
  return window_tree_host_->compositor();
}

const ui::Layer* NativeWidgetMus::GetLayer() const {
  return content_ ? content_->layer() : nullptr;
}

void NativeWidgetMus::ReorderNativeViews() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::ViewRemoved(View* view) {
  NOTIMPLEMENTED();
}

// These methods are wrong in mojo. They're not usually used to associate data
// with a window; they are used to pass data from one layer to another (and in
// chrome/ to unsafely pass raw pointers around--I can only find two places
// where we do the "safe" thing and even that requires casting an integer to a
// void*). They can't be used safely in a world where we separate things with
// mojo.
//
// It's also used to communicate between views and aura; in views, we set
// properties on a widget, and read these properties directly in aura code.
void NativeWidgetMus::SetNativeWindowProperty(const char* name, void* value) {
  if (content_)
    content_->SetNativeWindowProperty(name, value);
}

void* NativeWidgetMus::GetNativeWindowProperty(const char* name) const {
  return content_ ? content_->GetNativeWindowProperty(name) : nullptr;
}

TooltipManager* NativeWidgetMus::GetTooltipManager() const {
  return tooltip_manager_.get();
}

void NativeWidgetMus::SetCapture() {
  if (content_)
    content_->SetCapture();
}

void NativeWidgetMus::ReleaseCapture() {
  if (content_)
    content_->ReleaseCapture();
}

bool NativeWidgetMus::HasCapture() const {
  return content_ && content_->HasCapture();
}

ui::InputMethod* NativeWidgetMus::GetInputMethod() {
  return window_tree_host_ ? window_tree_host_->GetInputMethod() : nullptr;
}

void NativeWidgetMus::CenterWindow(const gfx::Size& size) {
  if (is_parallel_widget_in_window_manager())
    return;
  // TODO(beng): clear user-placed property and set preferred size property.
  window_->SetSharedProperty<gfx::Size>(
      ui::mojom::WindowManager::kPreferredSize_Property, size);

  gfx::Rect bounds = display::Screen::GetScreen()
                         ->GetDisplayNearestWindow(content_)
                         .work_area();
  bounds.ClampToCenteredSize(size);
  window_->SetBounds(bounds);
}

void NativeWidgetMus::GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* maximized) const {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::SetWindowTitle(const base::string16& title) {
  if (!window_ || is_parallel_widget_in_window_manager())
    return false;
  const char* kWindowTitle_Property =
      ui::mojom::WindowManager::kWindowTitle_Property;
  const base::string16 current_title =
      window_->HasSharedProperty(kWindowTitle_Property)
          ? window_->GetSharedProperty<base::string16>(kWindowTitle_Property)
          : base::string16();
  if (current_title == title)
    return false;
  window_->SetSharedProperty<base::string16>(kWindowTitle_Property, title);
  return true;
}

void NativeWidgetMus::SetWindowIcons(const gfx::ImageSkia& window_icon,
                                     const gfx::ImageSkia& app_icon) {
  if (is_parallel_widget_in_window_manager())
    return;

  const char* const kWindowAppIcon_Property =
      ui::mojom::WindowManager::kWindowAppIcon_Property;

  if (!app_icon.isNull()) {
    // Send the app icon 1x bitmap to the window manager.
    // TODO(jamescook): Support other scale factors.
    window_->SetSharedProperty<SkBitmap>(
        kWindowAppIcon_Property, app_icon.GetRepresentation(1.f).sk_bitmap());
  } else if (window_->HasSharedProperty(kWindowAppIcon_Property)) {
    // Remove the existing icon.
    window_->ClearSharedProperty(kWindowAppIcon_Property);
  }
}

void NativeWidgetMus::InitModalType(ui::ModalType modal_type) {
  if (modal_type != ui::MODAL_TYPE_NONE)
    window_->SetModal();
}

gfx::Rect NativeWidgetMus::GetWindowBoundsInScreen() const {
  if (!window_)
    return gfx::Rect();

  // Correct for the origin of the display.
  const int64_t window_display_id = window_->GetRoot()->display_id();
  for (display::Display display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.id() == window_display_id) {
      gfx::Point display_origin = display.bounds().origin();
      gfx::Rect bounds_in_screen = window_->GetBoundsInRoot();
      bounds_in_screen.Offset(display_origin.x(), display_origin.y());
      return bounds_in_screen;
    }
  }
  // Unknown display, assume primary display at 0,0.
  return window_->GetBoundsInRoot();
}

gfx::Rect NativeWidgetMus::GetClientAreaBoundsInScreen() const {
  // View-to-screen coordinate system transformations depend on this returning
  // the full window bounds, for example View::ConvertPointToScreen().
  return GetWindowBoundsInScreen();
}

gfx::Rect NativeWidgetMus::GetRestoredBounds() const {
  // Restored bounds should only be relevant if the window is minimized,
  // maximized, fullscreen or docked. However, in some places the code expects
  // GetRestoredBounds() to return the current window bounds if the window is
  // not in either state.
  if (IsMinimized() || IsMaximized() || IsFullscreen()) {
    const char* kRestoreBounds_Property =
        ui::mojom::WindowManager::kRestoreBounds_Property;
    if (window_->HasSharedProperty(kRestoreBounds_Property))
      return window_->GetSharedProperty<gfx::Rect>(kRestoreBounds_Property);
  }
  return GetWindowBoundsInScreen();
}

std::string NativeWidgetMus::GetWorkspace() const {
  return std::string();
}

void NativeWidgetMus::SetBounds(const gfx::Rect& bounds_in_screen) {
  if (!(window_ && window_tree_host_))
    return;

  // TODO(jamescook): Needs something like aura::ScreenPositionClient so higher
  // level code can move windows between displays. crbug.com/645291
  gfx::Point origin(bounds_in_screen.origin());
  const gfx::Point display_origin = display::Screen::GetScreen()
                                        ->GetDisplayMatching(bounds_in_screen)
                                        .bounds()
                                        .origin();
  origin.Offset(-display_origin.x(), -display_origin.y());

  gfx::Size size(bounds_in_screen.size());
  const gfx::Size min_size = GetMinimumSize();
  const gfx::Size max_size = GetMaximumSize();
  if (!max_size.IsEmpty())
    size.SetToMin(max_size);
  size.SetToMax(min_size);
  window_->SetBounds(gfx::Rect(origin, size));
  // Observer on |window_tree_host_| expected to synchronously update bounds.
  DCHECK(window_->bounds() == window_tree_host_->GetBounds());
}

void NativeWidgetMus::SetSize(const gfx::Size& size) {
  if (!window_tree_host_)
    return;

  gfx::Rect bounds = window_tree_host_->GetBounds();
  SetBounds(gfx::Rect(bounds.origin(), size));
}

void NativeWidgetMus::StackAbove(gfx::NativeView native_view) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::StackAtTop() {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetShape(std::unique_ptr<SkRegion> shape) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::Close() {
  Hide();
  if (!close_widget_factory_.HasWeakPtrs()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&NativeWidgetMus::CloseNow,
                              close_widget_factory_.GetWeakPtr()));
  }
}

void NativeWidgetMus::CloseNow() {
  // Depending upon ownership |window_| may have been destroyed.
  if (window_)
    window_->Destroy();
}

void NativeWidgetMus::Show() {
  ShowWithWindowState(ui::SHOW_STATE_NORMAL);
}

void NativeWidgetMus::Hide() {
  if (!(window_ && window_tree_host_))
    return;

  // NOTE: |window_tree_host_| and |window_| visibility is updated in
  // OnMusWindowVisibilityChanged().
  window_->SetVisible(false);
}

void NativeWidgetMus::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  if (!window_)
    return;

  window_->SetSharedProperty<gfx::Rect>(
      ui::mojom::WindowManager::kRestoreBounds_Property, restored_bounds);
  ShowWithWindowState(ui::SHOW_STATE_MAXIMIZED);
}

void NativeWidgetMus::ShowWithWindowState(ui::WindowShowState state) {
  if (!(window_ && window_tree_host_))
    return;

  // Matches NativeWidgetAura.
  switch (state) {
    case ui::SHOW_STATE_MAXIMIZED:
      SetShowState(ui::mojom::ShowState::MAXIMIZED);
      break;
    case ui::SHOW_STATE_FULLSCREEN:
      SetShowState(ui::mojom::ShowState::FULLSCREEN);
      break;
    case ui::SHOW_STATE_DOCKED:
      SetShowState(ui::mojom::ShowState::DOCKED);
      break;
    default:
      break;
  }

  // NOTE: |window_tree_host_| and |window_| visibility is updated in
  // OnMusWindowVisibilityChanged().
  window_->SetVisible(true);
  if (native_widget_delegate_->CanActivate()) {
    if (state != ui::SHOW_STATE_INACTIVE)
      Activate();
    GetWidget()->SetInitialFocus(state);
  }

  // Matches NativeWidgetAura.
  if (state == ui::SHOW_STATE_MINIMIZED)
    Minimize();
}

bool NativeWidgetMus::IsVisible() const {
  // TODO(beng): this should probably be wired thru PlatformWindow.
  return window_ && window_->visible();
}

void NativeWidgetMus::Activate() {
  if (!window_)
    return;

  static_cast<aura::client::ActivationClient*>(focus_client_.get())
      ->ActivateWindow(content_);
}

void NativeWidgetMus::Deactivate() {
  if (IsActive())
    window_->window_tree()->ClearFocus();
}

bool NativeWidgetMus::IsActive() const {
  ui::Window* focused =
      window_ ? window_->window_tree()->GetFocusedWindow() : nullptr;
  return focused && window_->Contains(focused);
}

void NativeWidgetMus::SetAlwaysOnTop(bool always_on_top) {
  if (window_ && !is_parallel_widget_in_window_manager()) {
    window_->SetSharedProperty<bool>(
        ui::mojom::WindowManager::kAlwaysOnTop_Property, always_on_top);
  }
}

bool NativeWidgetMus::IsAlwaysOnTop() const {
  return window_ &&
         window_->HasSharedProperty(
             ui::mojom::WindowManager::kAlwaysOnTop_Property) &&
         window_->GetSharedProperty<bool>(
             ui::mojom::WindowManager::kAlwaysOnTop_Property);
}

void NativeWidgetMus::SetVisibleOnAllWorkspaces(bool always_visible) {
  // Not needed for chromeos.
}

bool NativeWidgetMus::IsVisibleOnAllWorkspaces() const {
  return false;
}

void NativeWidgetMus::Maximize() {
  SetShowState(ui::mojom::ShowState::MAXIMIZED);
}

void NativeWidgetMus::Minimize() {
  SetShowState(ui::mojom::ShowState::MINIMIZED);
}

bool NativeWidgetMus::IsMaximized() const {
  return GetShowState(window_) == ui::mojom::ShowState::MAXIMIZED;
}

bool NativeWidgetMus::IsMinimized() const {
  return GetShowState(window_) == ui::mojom::ShowState::MINIMIZED;
}

void NativeWidgetMus::Restore() {
  SetShowState(ui::mojom::ShowState::NORMAL);
}

void NativeWidgetMus::SetFullscreen(bool fullscreen) {
  if (IsFullscreen() == fullscreen)
    return;
  if (fullscreen) {
    show_state_before_fullscreen_ = GetShowState(window_);
    SetShowState(ui::mojom::ShowState::FULLSCREEN);
  } else {
    switch (show_state_before_fullscreen_) {
      case ui::mojom::ShowState::MAXIMIZED:
        Maximize();
        break;
      case ui::mojom::ShowState::MINIMIZED:
        Minimize();
        break;
      case ui::mojom::ShowState::DEFAULT:
      case ui::mojom::ShowState::NORMAL:
      case ui::mojom::ShowState::INACTIVE:
      case ui::mojom::ShowState::FULLSCREEN:
      case ui::mojom::ShowState::DOCKED:
        // TODO(sad): This may not be sufficient.
        Restore();
        break;
    }
  }
}

bool NativeWidgetMus::IsFullscreen() const {
  return GetShowState(window_) == ui::mojom::ShowState::FULLSCREEN;
}

void NativeWidgetMus::SetOpacity(float opacity) {
  if (window_)
    window_->SetOpacity(opacity);
}

void NativeWidgetMus::FlashFrame(bool flash_frame) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::RunShellDrag(View* view,
                                   const ui::OSExchangeData& data,
                                   const gfx::Point& location,
                                   int drag_operations,
                                   ui::DragDropTypes::DragEventSource source) {
  if (window_)
    views::RunShellDrag(content_, data, location, drag_operations, source);
}

void NativeWidgetMus::SchedulePaintInRect(const gfx::Rect& rect) {
  if (content_)
    content_->SchedulePaintInRect(rect);
}

void NativeWidgetMus::SetCursor(gfx::NativeCursor cursor) {
  if (!window_)
    return;

  // TODO(erg): In aura, our incoming cursor is really two
  // parts. cursor.native_type() is an integer for standard cursors and is all
  // we support right now. If native_type() == kCursorCustom, than we should
  // also send an image, but as the cursor code is currently written, the image
  // is in a platform native format that's already uploaded to the window
  // server.
  ui::mojom::Cursor new_cursor = ui::mojom::Cursor(cursor.native_type());
  if (last_cursor_ != new_cursor)
    window_->SetPredefinedCursor(new_cursor);
}

bool NativeWidgetMus::IsMouseEventsEnabled() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::ClearNativeFocus() {
  if (!IsActive())
    return;
  ui::Window* focused =
      window_ ? window_->window_tree()->GetFocusedWindow() : nullptr;
  if (focused && window_->Contains(focused) && focused != window_)
    window_->SetFocus();
  // Move aura-focus back to |content_|, so that the Widget still receives
  // events correctly.
  aura::client::GetFocusClient(content_)->ResetFocusWithinActiveWindow(
      content_);
}

gfx::Rect NativeWidgetMus::GetWorkAreaBoundsInScreen() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

Widget::MoveLoopResult NativeWidgetMus::RunMoveLoop(
    const gfx::Vector2d& drag_offset,
    Widget::MoveLoopSource source,
    Widget::MoveLoopEscapeBehavior escape_behavior) {
  ReleaseCapture();

  base::MessageLoopForUI* loop = base::MessageLoopForUI::current();
  base::MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop;

  ui::mojom::MoveLoopSource mus_source =
      source == Widget::MOVE_LOOP_SOURCE_MOUSE
          ? ui::mojom::MoveLoopSource::MOUSE
          : ui::mojom::MoveLoopSource::TOUCH;

  bool success = false;
  gfx::Point cursor_location =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  window_->PerformWindowMove(
      mus_source, cursor_location,
      base::Bind(OnMoveLoopEnd, &success, run_loop.QuitClosure()));

  run_loop.Run();

  return success ? Widget::MOVE_LOOP_SUCCESSFUL : Widget::MOVE_LOOP_CANCELED;
}

void NativeWidgetMus::EndMoveLoop() {
  window_->CancelWindowMove();
}

void NativeWidgetMus::SetVisibilityChangedAnimationsEnabled(bool value) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityAnimationDuration(
    const base::TimeDelta& duration) {
  NOTIMPLEMENTED();
}

void NativeWidgetMus::SetVisibilityAnimationTransition(
    Widget::VisibilityTransition transition) {
  NOTIMPLEMENTED();
}

ui::NativeTheme* NativeWidgetMus::GetNativeTheme() const {
  return ui::NativeThemeAura::instance();
}

void NativeWidgetMus::OnRootViewLayout() {
  NOTIMPLEMENTED();
}

bool NativeWidgetMus::IsTranslucentWindowOpacitySupported() const {
  NOTIMPLEMENTED();
  return true;
}

void NativeWidgetMus::OnSizeConstraintsChanged() {
  if (!window_ || is_parallel_widget_in_window_manager())
    return;

  window_->SetSharedProperty<int32_t>(
      ui::mojom::WindowManager::kResizeBehavior_Property,
      ResizeBehaviorFromDelegate(GetWidget()->widget_delegate()));
}

void NativeWidgetMus::RepostNativeEvent(gfx::NativeEvent native_event) {
  NOTIMPLEMENTED();
}

std::string NativeWidgetMus::GetName() const {
  return window_->GetName();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, aura::WindowDelegate implementation:

gfx::Size NativeWidgetMus::GetMinimumSize() const {
  return native_widget_delegate_->GetMinimumSize();
}

gfx::Size NativeWidgetMus::GetMaximumSize() const {
  return native_widget_delegate_->GetMaximumSize();
}

void NativeWidgetMus::OnBoundsChanged(const gfx::Rect& old_bounds,
                                      const gfx::Rect& new_bounds) {
  // This is handled in OnHost{Resized,Moved}() like DesktopNativeWidgetAura
  // instead of here like in NativeWidgetAura.
}

gfx::NativeCursor NativeWidgetMus::GetCursor(const gfx::Point& point) {
  return gfx::NativeCursor();
}

int NativeWidgetMus::GetNonClientComponent(const gfx::Point& point) const {
  return native_widget_delegate_->GetNonClientComponent(point);
}

bool NativeWidgetMus::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  views::WidgetDelegate* widget_delegate = GetWidget()->widget_delegate();
  return !widget_delegate ||
      widget_delegate->ShouldDescendIntoChildForEventHandling(child, location);
}

bool NativeWidgetMus::CanFocus() {
  return true;
}

void NativeWidgetMus::OnCaptureLost() {
  native_widget_delegate_->OnMouseCaptureLost();
}

void NativeWidgetMus::OnPaint(const ui::PaintContext& context) {
  native_widget_delegate_->OnNativeWidgetPaint(context);
}

void NativeWidgetMus::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

void NativeWidgetMus::OnWindowDestroying(aura::Window* window) {
}

void NativeWidgetMus::OnWindowDestroyed(aura::Window* window) {
  // Cleanup happens in OnPlatformWindowClosed().
}

void NativeWidgetMus::OnWindowTargetVisibilityChanged(bool visible) {
}

bool NativeWidgetMus::HasHitTestMask() const {
  return native_widget_delegate_->HasHitTestMask();
}

void NativeWidgetMus::GetHitTestMask(gfx::Path* mask) const {
  native_widget_delegate_->GetHitTestMask(mask);
}

void NativeWidgetMus::SetShowState(ui::mojom::ShowState show_state) {
  if (!window_)
    return;
  window_->SetSharedProperty<int32_t>(
      ui::mojom::WindowManager::kShowState_Property,
      static_cast<int32_t>(show_state));
}

void NativeWidgetMus::OnKeyEvent(ui::KeyEvent* event) {
  if (event->is_char()) {
    // If a ui::InputMethod object is attached to the root window, character
    // events are handled inside the object and are not passed to this function.
    // If such object is not attached, character events might be sent (e.g. on
    // Windows). In this case, we just skip these.
    return;
  }
  // Renderer may send a key event back to us if the key event wasn't handled,
  // and the window may be invisible by that time.
  if (!content_->IsVisible())
    return;

  native_widget_delegate_->OnKeyEvent(event);
}

void NativeWidgetMus::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(content_->IsVisible());

  if (tooltip_manager_.get())
    tooltip_manager_->UpdateTooltip();
  TooltipManagerAura::UpdateTooltipManagerForCapture(GetWidget());

  native_widget_delegate_->OnMouseEvent(event);
  // WARNING: we may have been deleted.
}

void NativeWidgetMus::OnScrollEvent(ui::ScrollEvent* event) {
  if (event->type() == ui::ET_SCROLL) {
    native_widget_delegate_->OnScrollEvent(event);
    if (event->handled())
      return;

    // Convert unprocessed scroll events into wheel events.
    ui::MouseWheelEvent mwe(*event->AsScrollEvent());
    native_widget_delegate_->OnMouseEvent(&mwe);
    if (mwe.handled())
      event->SetHandled();
  } else {
    native_widget_delegate_->OnScrollEvent(event);
  }
}

void NativeWidgetMus::OnGestureEvent(ui::GestureEvent* event) {
  native_widget_delegate_->OnGestureEvent(event);
}

void NativeWidgetMus::OnHostResized(const aura::WindowTreeHost* host) {
  native_widget_delegate_->OnNativeWidgetSizeChanged(
      host->window()->bounds().size());
  UpdateClientArea();
  UpdateHitTestMask();
}

void NativeWidgetMus::OnHostMoved(const aura::WindowTreeHost* host,
                                  const gfx::Point& new_origin) {
  native_widget_delegate_->OnNativeWidgetMove();
}

void NativeWidgetMus::OnHostCloseRequested(const aura::WindowTreeHost* host) {
  GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, aura::WindowDragDropDelegate implementation:

void NativeWidgetMus::OnDragEntered(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_);
  last_drop_operation_ = drop_helper_->OnDragOver(
      event.data(), event.location(), event.source_operations());
}

int NativeWidgetMus::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_);
  last_drop_operation_ = drop_helper_->OnDragOver(
      event.data(), event.location(), event.source_operations());
  return last_drop_operation_;
}

void NativeWidgetMus::OnDragExited() {
  DCHECK(drop_helper_);
  drop_helper_->OnDragExit();
}

int NativeWidgetMus::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(drop_helper_);
  return drop_helper_->OnDrop(event.data(), event.location(),
                              last_drop_operation_);
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetMus, ui::InputEventHandler implementation:

void NativeWidgetMus::OnWindowInputEvent(
    ui::Window* view,
    const ui::Event& event_in,
    std::unique_ptr<base::Callback<void(EventResult)>>* ack_callback) {
  std::unique_ptr<ui::Event> event = ui::Event::Clone(event_in);

  if (event->IsKeyEvent()) {
    input_method_->DispatchKeyEvent(event->AsKeyEvent(),
                                    std::move(*ack_callback));
    return;
  }

  // Take ownership of the callback, indicating that we will handle it.
  EventAckHandler ack_handler(std::move(*ack_callback));

  // TODO(markdittmer): This should be this->OnEvent(event.get()), but that
  // can't happen until IME is refactored out of in WindowTreeHostMus.
  platform_window_delegate()->DispatchEvent(event.get());
  // NOTE: |this| may be deleted.

  ack_handler.set_handled(event->handled());
  // |ack_handler| acks the event on destruction if necessary.
}

void NativeWidgetMus::OnMusWindowVisibilityChanging(ui::Window* window,
                                                    bool visible) {
  if (window == window_)
    native_widget_delegate_->OnNativeWidgetVisibilityChanging(visible);
}

void NativeWidgetMus::OnMusWindowVisibilityChanged(ui::Window* window,
                                                   bool visible) {
  if (window != window_)
    return;

  if (visible) {
    window_tree_host_->Show();
    GetNativeWindow()->Show();
  } else {
    window_tree_host_->Hide();
    GetNativeWindow()->Hide();
  }
  native_widget_delegate_->OnNativeWidgetVisibilityChanged(visible);
}

void NativeWidgetMus::UpdateHitTestMask() {
  // The window manager (or other underlay window provider) is not allowed to
  // set a hit test mask, as that could interfere with a client app mask.
  if (is_parallel_widget_in_window_manager())
    return;

  if (!native_widget_delegate_->HasHitTestMask()) {
    window_->ClearHitTestMask();
    return;
  }

  gfx::Path mask_path;
  native_widget_delegate_->GetHitTestMask(&mask_path);
  // TODO(jamescook): Use the full path for the mask.
  gfx::Rect mask_rect =
      gfx::ToEnclosingRect(gfx::SkRectToRectF(mask_path.getBounds()));
  window_->SetHitTestMask(mask_rect);
}

}  // namespace views
