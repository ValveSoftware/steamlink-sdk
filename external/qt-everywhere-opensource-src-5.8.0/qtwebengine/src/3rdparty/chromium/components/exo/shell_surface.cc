// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/window_resizer.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/window_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/surface.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/path.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

DECLARE_WINDOW_PROPERTY_TYPE(std::string*)

namespace exo {
namespace {

// This is a struct for accelerator keys used to close ShellSurfaces.
const struct Accelerator {
  ui::KeyboardCode keycode;
  int modifiers;
} kCloseWindowAccelerators[] = {
    {ui::VKEY_W, ui::EF_CONTROL_DOWN},
    {ui::VKEY_W, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
    {ui::VKEY_F4, ui::EF_ALT_DOWN}};

void UpdateShelfStateForFullscreenChange(views::Widget* widget) {
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(widget->GetNativeWindow());
  window_state->set_shelf_mode_in_fullscreen(
      ash::wm::WindowState::SHELF_AUTO_HIDE_INVISIBLE);
  ash::Shell::GetInstance()->UpdateShelfVisibility();
}

class CustomFrameView : public views::NonClientFrameView {
 public:
  explicit CustomFrameView(views::Widget* widget) : widget_(widget) {}
  ~CustomFrameView() override {}

  // Overridden from views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return client_bounds;
  }
  int NonClientHitTest(const gfx::Point& point) override {
    return widget_->client_view()->NonClientHitTest(point);
  }
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 private:
  views::Widget* const widget_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameView);
};

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  CustomWindowTargeter() {}
  ~CustomWindowTargeter() override {}

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    Surface* surface = ShellSurface::GetMainSurface(window);
    if (!surface)
      return false;

    gfx::Point local_point = event.location();
    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);

    aura::Window::ConvertPointToTarget(window, surface->window(), &local_point);
    return surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1)));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

// Handles a user's fullscreen request (Shift+F4/F4).
class CustomWindowStateDelegate : public ash::wm::WindowStateDelegate,
                                  public views::WidgetObserver {
 public:
  explicit CustomWindowStateDelegate(views::Widget* widget) : widget_(widget) {
    widget_->AddObserver(this);
  }
  ~CustomWindowStateDelegate() override {
    if (widget_)
      widget_->RemoveObserver(this);
  }

  // Overridden from ash::wm::WindowStateDelegate:
  bool ToggleFullscreen(ash::wm::WindowState* window_state) override {
    if (widget_) {
      bool enter_fullscreen = !window_state->IsFullscreen();
      widget_->SetFullscreen(enter_fullscreen);
      UpdateShelfStateForFullscreenChange(widget_);
    }
    return true;
  }

  // Overridden from views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowStateDelegate);
};

class ShellSurfaceWidget : public views::Widget {
 public:
  explicit ShellSurfaceWidget(ShellSurface* shell_surface)
      : shell_surface_(shell_surface) {}

  // Overridden from views::Widget
  void Close() override { shell_surface_->Close(); }
  void OnKeyEvent(ui::KeyEvent* event) override {
    // TODO(hidehiko): Handle ESC + SHIFT + COMMAND accelerator key
    // to escape pinned mode.
    // Handle only accelerators. Do not call Widget::OnKeyEvent that eats focus
    // management keys (like the tab key) as well.
    if (GetFocusManager()->ProcessAccelerator(ui::Accelerator(*event)))
      event->StopPropagation();
  }

 private:
  ShellSurface* const shell_surface_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurfaceWidget);
};

}  // namespace

// Helper class used to coalesce a number of changes into one "configure"
// callback. Callbacks are suppressed while an instance of this class is
// instantiated and instead called when the instance is destroyed.
// If |force_configure_| is true ShellSurface::Configure() will be called
// even if no changes to shell surface took place during the lifetime of the
// ScopedConfigure instance.
class ShellSurface::ScopedConfigure {
 public:
  ScopedConfigure(ShellSurface* shell_surface, bool force_configure);
  ~ScopedConfigure();

  void set_needs_configure() { needs_configure_ = true; }

 private:
  ShellSurface* const shell_surface_;
  const bool force_configure_;
  bool needs_configure_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedConfigure);
};

// Helper class used to temporarily disable animations. Restores the
// animations disabled property when instance is destroyed.
class ShellSurface::ScopedAnimationsDisabled {
 public:
  explicit ScopedAnimationsDisabled(ShellSurface* shell_surface);
  ~ScopedAnimationsDisabled();

 private:
  ShellSurface* const shell_surface_;
  bool saved_animations_disabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedAnimationsDisabled);
};

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedConfigure:

ShellSurface::ScopedConfigure::ScopedConfigure(ShellSurface* shell_surface,
                                               bool force_configure)
    : shell_surface_(shell_surface), force_configure_(force_configure) {
  // ScopedConfigure instances cannot be nested.
  DCHECK(!shell_surface_->scoped_configure_);
  shell_surface_->scoped_configure_ = this;
}

ShellSurface::ScopedConfigure::~ScopedConfigure() {
  DCHECK_EQ(shell_surface_->scoped_configure_, this);
  shell_surface_->scoped_configure_ = nullptr;
  if (needs_configure_ || force_configure_)
    shell_surface_->Configure();
  // ScopedConfigure instance might have suppressed a widget bounds update.
  if (shell_surface_->widget_) {
    shell_surface_->UpdateWidgetBounds();
    shell_surface_->UpdateShadow();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, ScopedAnimationsDisabled:

ShellSurface::ScopedAnimationsDisabled::ScopedAnimationsDisabled(
    ShellSurface* shell_surface)
    : shell_surface_(shell_surface) {
  if (shell_surface_->widget_) {
    aura::Window* window = shell_surface_->widget_->GetNativeWindow();
    saved_animations_disabled_ =
        window->GetProperty(aura::client::kAnimationsDisabledKey);
    window->SetProperty(aura::client::kAnimationsDisabledKey, true);
  }
}

ShellSurface::ScopedAnimationsDisabled::~ScopedAnimationsDisabled() {
  if (shell_surface_->widget_) {
    aura::Window* window = shell_surface_->widget_->GetNativeWindow();
    DCHECK_EQ(window->GetProperty(aura::client::kAnimationsDisabledKey), true);
    window->SetProperty(aura::client::kAnimationsDisabledKey,
                        saved_animations_disabled_);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(std::string*, kApplicationIdKey, nullptr)
DEFINE_LOCAL_WINDOW_PROPERTY_KEY(Surface*, kMainSurfaceKey, nullptr)

ShellSurface::ShellSurface(Surface* surface,
                           ShellSurface* parent,
                           const gfx::Rect& initial_bounds,
                           bool activatable,
                           int container)
    : widget_(nullptr),
      surface_(surface),
      parent_(parent ? parent->GetWidget()->GetNativeWindow() : nullptr),
      initial_bounds_(initial_bounds),
      activatable_(activatable),
      container_(container) {
  ash::Shell::GetInstance()->activation_client()->AddObserver(this);
  surface_->SetSurfaceDelegate(this);
  surface_->AddSurfaceObserver(this);
  surface_->window()->Show();
  set_owned_by_client();
  if (parent_)
    parent_->AddObserver(this);
}

ShellSurface::ShellSurface(Surface* surface)
    : ShellSurface(surface,
                   nullptr,
                   gfx::Rect(),
                   true,
                   ash::kShellWindowId_DefaultContainer) {}

ShellSurface::~ShellSurface() {
  DCHECK(!scoped_configure_);
  if (resizer_)
    EndDrag(false /* revert */);
  if (widget_) {
    ash::wm::GetWindowState(widget_->GetNativeWindow())->RemoveObserver(this);
    widget_->GetNativeWindow()->RemoveObserver(this);
    if (widget_->IsVisible())
      widget_->Hide();
    widget_->CloseNow();
  }
  ash::Shell::GetInstance()->activation_client()->RemoveObserver(this);
  if (parent_)
    parent_->RemoveObserver(this);
  if (surface_) {
    if (scale_ != 1.0)
      surface_->window()->SetTransform(gfx::Transform());
    surface_->SetSurfaceDelegate(nullptr);
    surface_->RemoveSurfaceObserver(this);
  }
}

void ShellSurface::AcknowledgeConfigure(uint32_t serial) {
  TRACE_EVENT1("exo", "ShellSurface::AcknowledgeConfigure", "serial", serial);

  // Apply all configs that are older or equal to |serial|. The result is that
  // the origin of the main surface will move and the resize direction will
  // change to reflect the acknowledgement of configure request with |serial|
  // at the next call to Commit().
  while (!pending_configs_.empty()) {
    auto config = pending_configs_.front();
    pending_configs_.pop_front();

    // Add the config offset to the accumulated offset that will be applied when
    // Commit() is called.
    pending_origin_offset_ += config.origin_offset;

    // Set the resize direction that will be applied when Commit() is called.
    pending_resize_component_ = config.resize_component;

    if (config.serial == serial)
      break;
  }

  if (widget_)
    UpdateWidgetBounds();
}

void ShellSurface::SetParent(ShellSurface* parent) {
  TRACE_EVENT1("exo", "ShellSurface::SetParent", "parent",
               parent ? base::UTF16ToASCII(parent->title_) : "null");

  if (parent_) {
    parent_->RemoveObserver(this);
    if (widget_)
      wm::RemoveTransientChild(parent_, widget_->GetNativeWindow());
  }
  parent_ = parent ? parent->GetWidget()->GetNativeWindow() : nullptr;
  if (parent_) {
    parent_->AddObserver(this);
    if (widget_)
      wm::AddTransientChild(parent_, widget_->GetNativeWindow());
  }
}

void ShellSurface::Activate() {
  TRACE_EVENT0("exo", "ShellSurface::Activate");

  if (!widget_ || widget_->IsActive())
    return;

  widget_->Activate();
}

void ShellSurface::Maximize() {
  TRACE_EVENT0("exo", "ShellSurface::Maximize");

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_MAXIMIZED);

  // Note: This will ask client to configure its surface even if already
  // maximized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Maximize();
}

void ShellSurface::Minimize() {
  TRACE_EVENT0("exo", "ShellSurface::Minimize");

  if (!widget_)
    return;

  // Note: This will ask client to configure its surface even if already
  // minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Minimize();
}

void ShellSurface::Restore() {
  TRACE_EVENT0("exo", "ShellSurface::Restore");

  if (!widget_)
    return;

  // Note: This will ask client to configure its surface even if not already
  // maximized or minimized.
  ScopedConfigure scoped_configure(this, true);
  widget_->Restore();
}

void ShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ShellSurface::SetFullscreen", "fullscreen", fullscreen);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_FULLSCREEN);

  // Note: This will ask client to configure its surface even if fullscreen
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->SetFullscreen(fullscreen);
  UpdateShelfStateForFullscreenChange(widget_);
}

void ShellSurface::SetPinned(bool pinned) {
  TRACE_EVENT1("exo", "ShellSurface::SetPinned", "pinned", pinned);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  // Note: This will ask client to configure its surface even if pinned
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  if (pinned) {
    ash::wm::PinWindow(widget_->GetNativeWindow());
  } else {
    // At the moment, we cannot just unpin the window state, due to ash
    // implementation. Instead, we call Restore() to unpin, if it is Pinned
    // state. In this implementation, we may loose the previous state,
    // if the previous state is fullscreen, etc.
    if (ash::wm::GetWindowState(widget_->GetNativeWindow())->IsPinned())
      widget_->Restore();
  }
}

void ShellSurface::SetTitle(const base::string16& title) {
  TRACE_EVENT1("exo", "ShellSurface::SetTitle", "title",
               base::UTF16ToUTF8(title));

  title_ = title;
  if (widget_)
    widget_->UpdateWindowTitle();
}

void ShellSurface::SetSystemModal(bool system_modal) {
  // System modal container is used by clients to implement client side
  // managed system modal dialogs using a single ShellSurface instance.
  // Hit-test region will be non-empty when at least one dialog exists on
  // the client side. Here we detect the transition between no client side
  // dialog and at least one dialog so activatable state is properly
  // updated.
  if (container_ != ash::kShellWindowId_SystemModalContainer) {
    LOG(ERROR)
        << "Only a window in SystemModalContainer can change the modality";
    return;
  }
  widget_->GetNativeWindow()->SetProperty(
      aura::client::kModalKey,
      system_modal ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_NONE);
}

// static
void ShellSurface::SetApplicationId(aura::Window* window,
                                    std::string* application_id) {
  window->SetProperty(kApplicationIdKey, application_id);
}

// static
const std::string ShellSurface::GetApplicationId(aura::Window* window) {
  std::string* string_ptr = window->GetProperty(kApplicationIdKey);
  return string_ptr ? *string_ptr : std::string();
}

void ShellSurface::SetApplicationId(const std::string& application_id) {
  TRACE_EVENT1("exo", "ShellSurface::SetApplicationId", "application_id",
               application_id);

  application_id_ = application_id;
}

void ShellSurface::Move() {
  TRACE_EVENT0("exo", "ShellSurface::Move");

  if (widget_ && !widget_->movement_disabled())
    AttemptToStartDrag(HTCAPTION);
}

void ShellSurface::Resize(int component) {
  TRACE_EVENT1("exo", "ShellSurface::Resize", "component", component);

  if (widget_ && !widget_->movement_disabled())
    AttemptToStartDrag(component);
}

void ShellSurface::Close() {
  if (!close_callback_.is_null())
    close_callback_.Run();
}

void ShellSurface::SetGeometry(const gfx::Rect& geometry) {
  TRACE_EVENT1("exo", "ShellSurface::SetGeometry", "geometry",
               geometry.ToString());

  if (geometry.IsEmpty()) {
    DLOG(WARNING) << "Surface geometry must be non-empty";
    return;
  }

  pending_geometry_ = geometry;
}

void ShellSurface::SetRectangularShadow(const gfx::Rect& content_bounds) {
  TRACE_EVENT1("exo", "ShellSurface::SetRectangularShadow", "content_bounds",
               content_bounds.ToString());

  shadow_content_bounds_ = content_bounds;
}

void ShellSurface::SetRectangularShadowBackgroundOpacity(float opacity) {
  TRACE_EVENT1("exo", "ShellSurface::SetRectangularShadowBackgroundOpacity",
               "opacity", opacity);

  rectangular_shadow_background_opacity_ = opacity;
}

void ShellSurface::SetScale(double scale) {
  TRACE_EVENT1("exo", "ShellSurface::SetScale", "scale", scale);

  if (scale <= 0.0) {
    DLOG(WARNING) << "Surface scale must be greater than 0";
    return;
  }

  pending_scale_ = scale;
}

void ShellSurface::SetTopInset(int height) {
  TRACE_EVENT1("exo", "ShellSurface::SetTopInset", "height", height);

  pending_top_inset_height_ = height;
}

// static
void ShellSurface::SetMainSurface(aura::Window* window, Surface* surface) {
  window->SetProperty(kMainSurfaceKey, surface);
}

// static
Surface* ShellSurface::GetMainSurface(const aura::Window* window) {
  return window->GetProperty(kMainSurfaceKey);
}

std::unique_ptr<base::trace_event::TracedValue> ShellSurface::AsTracedValue()
    const {
  std::unique_ptr<base::trace_event::TracedValue> value(
      new base::trace_event::TracedValue());
  value->SetString("title", base::UTF16ToUTF8(title_));
  value->SetString("application_id", application_id_);
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ShellSurface::OnSurfaceCommit() {
  surface_->CheckIfSurfaceHierarchyNeedsCommitToNewSurfaces();
  surface_->CommitSurfaceHierarchy();

  if (enabled() && !widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  // Apply the accumulated pending origin offset to reflect acknowledged
  // configure requests.
  origin_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();

  // Update resize direction to reflect acknowledged configure requests.
  resize_component_ = pending_resize_component_;

  if (widget_) {
    // Apply new window geometry.
    geometry_ = pending_geometry_;

    UpdateWidgetBounds();
    UpdateShadow();

    // Apply new top inset height.
    if (pending_top_inset_height_ != top_inset_height_) {
      widget_->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                              pending_top_inset_height_);
      top_inset_height_ = pending_top_inset_height_;
    }

    gfx::Point surface_origin = GetSurfaceOrigin();

    // System modal container is used by clients to implement overlay
    // windows using a single ShellSurface instance.  If hit-test
    // region is empty, then it is non interactive window and won't be
    // activated.
    if (container_ == ash::kShellWindowId_SystemModalContainer) {
      gfx::Rect hit_test_bounds =
          surface_->GetHitTestBounds() + surface_origin.OffsetFromOrigin();

      // Prevent window from being activated when hit test bounds are empty.
      bool activatable = activatable_ && !hit_test_bounds.IsEmpty();
      if (activatable != CanActivate()) {
        set_can_activate(activatable);
        // Activate or deactivate window if activation state changed.
        if (activatable)
          wm::ActivateWindow(widget_->GetNativeWindow());
        else if (widget_->IsActive())
          wm::DeactivateWindow(widget_->GetNativeWindow());
      }
    }

    // Update surface bounds.
    surface_->window()->SetBounds(
        gfx::Rect(surface_origin, surface_->window()->layer()->size()));

    // Update surface scale.
    if (pending_scale_ != scale_) {
      gfx::Transform transform;
      DCHECK_NE(pending_scale_, 0.0);
      transform.Scale(1.0 / pending_scale_, 1.0 / pending_scale_);
      surface_->window()->SetTransform(transform);
      scale_ = pending_scale_;
    }

    // Show widget if needed.
    if (pending_show_widget_) {
      DCHECK(!widget_->IsClosed());
      DCHECK(!widget_->IsVisible());
      pending_show_widget_ = false;
      widget_->Show();
    }
  }
}

bool ShellSurface::IsSurfaceSynchronized() const {
  // A shell surface is always desynchronized.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void ShellSurface::OnSurfaceDestroying(Surface* surface) {
  if (resizer_)
    EndDrag(false /* revert */);
  if (widget_)
    SetMainSurface(widget_->GetNativeWindow(), nullptr);
  surface->RemoveSurfaceObserver(this);
  surface_ = nullptr;

  // Hide widget before surface is destroyed. This allows hide animations to
  // run using the current surface contents.
  if (widget_)
    widget_->Hide();

  // Note: In its use in the Wayland server implementation, the surface
  // destroyed callback may destroy the ShellSurface instance. This call needs
  // to be last so that the instance can be destroyed.
  if (!surface_destroyed_callback_.is_null())
    surface_destroyed_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

bool ShellSurface::CanResize() const {
  return initial_bounds_.IsEmpty();
}

bool ShellSurface::CanMaximize() const {
  // Shell surfaces in system modal container cannot be maximized.
  return container_ != ash::kShellWindowId_SystemModalContainer;
}

bool ShellSurface::CanMinimize() const {
  // Shell surfaces in system modal container cannot be minimized.
  return container_ != ash::kShellWindowId_SystemModalContainer;
}

base::string16 ShellSurface::GetWindowTitle() const {
  return title_;
}

void ShellSurface::WindowClosing() {
  if (resizer_)
    EndDrag(true /* revert */);
  SetEnabled(false);
  widget_ = nullptr;
  shadow_overlay_ = nullptr;
  shadow_underlay_ = nullptr;
}

views::Widget* ShellSurface::GetWidget() {
  return widget_;
}

const views::Widget* ShellSurface::GetWidget() const {
  return widget_;
}

views::View* ShellSurface::GetContentsView() {
  return this;
}

views::NonClientFrameView* ShellSurface::CreateNonClientFrameView(
    views::Widget* widget) {
  return new CustomFrameView(widget);
}

bool ShellSurface::WidgetHasHitTestMask() const {
  return surface_ ? surface_->HasHitTestMask() : false;
}

void ShellSurface::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(WidgetHasHitTestMask());
  surface_->GetHitTestMask(mask);
  gfx::Point origin = surface_->window()->bounds().origin();
  mask->offset(SkIntToScalar(origin.x()), SkIntToScalar(origin.y()));
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size ShellSurface::GetPreferredSize() const {
  if (!geometry_.IsEmpty())
    return geometry_.size();

  return surface_ ? surface_->window()->layer()->size() : gfx::Size();
}

////////////////////////////////////////////////////////////////////////////////
// ash::wm::WindowStateObserver overrides:

void ShellSurface::OnPreWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::wm::WindowStateType old_type) {
  ash::wm::WindowStateType new_type = window_state->GetStateType();
  if (ash::wm::IsMaximizedOrFullscreenOrPinnedWindowStateType(old_type) ||
      ash::wm::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
    // When transitioning in/out of maximized or fullscreen mode we need to
    // make sure we have a configure callback before we allow the default
    // cross-fade animations. The configure callback provides a mechanism for
    // the client to inform us that a frame has taken the state change into
    // account and without this cross-fade animations are unreliable.
    if (configure_callback_.is_null())
      scoped_animations_disabled_.reset(new ScopedAnimationsDisabled(this));
  }
}

void ShellSurface::OnPostWindowStateTypeChange(
    ash::wm::WindowState* window_state,
    ash::wm::WindowStateType old_type) {
  ash::wm::WindowStateType new_type = window_state->GetStateType();
  if (ash::wm::IsMaximizedOrFullscreenOrPinnedWindowStateType(old_type) ||
      ash::wm::IsMaximizedOrFullscreenOrPinnedWindowStateType(new_type)) {
    Configure();
  }

  if (widget_) {
    UpdateWidgetBounds();
    UpdateShadow();
  }

  if (old_type != new_type && !state_changed_callback_.is_null())
    state_changed_callback_.Run(old_type, new_type);

  // Re-enable animations if they were disabled in pre state change handler.
  scoped_animations_disabled_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void ShellSurface::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  if (!widget_ || !surface_ || ignore_window_bounds_changes_)
    return;

  if (window == widget_->GetNativeWindow()) {
    if (new_bounds.size() == old_bounds.size())
      return;

    // If size changed then give the client a chance to produce new contents
    // before origin on screen is changed by adding offset to the next configure
    // request and offset |origin_| by the same distance.
    gfx::Vector2d origin_offset = new_bounds.origin() - old_bounds.origin();
    pending_origin_config_offset_ += origin_offset;
    origin_ -= origin_offset;

    surface_->window()->SetBounds(
        gfx::Rect(GetSurfaceOrigin(), surface_->window()->layer()->size()));

    // The shadow size may be updated to match the widget. Change it back
    // to the shadow content size.
    // TODO(oshima): When the arc window reiszing is enabled, we may want to
    // implement shadow management here instead of using shadow controller.
    UpdateShadow();

    Configure();
  }
}

void ShellSurface::OnWindowDestroying(aura::Window* window) {
  if (window == parent_) {
    parent_ = nullptr;
    // Disable shell surface in case parent is destroyed before shell surface
    // widget has been created.
    SetEnabled(false);
  }
  window->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// aura::client::ActivationChangeObserver overrides:

void ShellSurface::OnWindowActivated(
    aura::client::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!widget_)
    return;

  if (gained_active == widget_->GetNativeWindow() ||
      lost_active == widget_->GetNativeWindow()) {
    DCHECK(activatable_);
    Configure();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void ShellSurface::OnKeyEvent(ui::KeyEvent* event) {
  if (!resizer_) {
    views::View::OnKeyEvent(event);
    return;
  }

  if (event->type() == ui::ET_KEY_PRESSED &&
      event->key_code() == ui::VKEY_ESCAPE) {
    EndDrag(true /* revert */);
  }
}

void ShellSurface::OnMouseEvent(ui::MouseEvent* event) {
  if (!resizer_) {
    views::View::OnMouseEvent(event);
    return;
  }

  if (event->handled())
    return;

  if ((event->flags() &
       (ui::EF_MIDDLE_MOUSE_BUTTON | ui::EF_RIGHT_MOUSE_BUTTON)) != 0)
    return;

  if (event->type() == ui::ET_MOUSE_CAPTURE_CHANGED) {
    // We complete the drag instead of reverting it, as reverting it will
    // result in a weird behavior when a client produces a modal dialog
    // while the drag is in progress.
    EndDrag(false /* revert */);
    return;
  }

  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED: {
      gfx::Point location(event->location());
      aura::Window::ConvertPointToTarget(widget_->GetNativeWindow(),
                                         widget_->GetNativeWindow()->parent(),
                                         &location);
      ScopedConfigure scoped_configure(this, false);
      resizer_->Drag(location, event->flags());
      event->StopPropagation();
      break;
    }
    case ui::ET_MOUSE_RELEASED: {
      ScopedConfigure scoped_configure(this, false);
      EndDrag(false /* revert */);
      break;
    }
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_PRESSED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// ui::AcceleratorTarget overrides:

bool ShellSurface::AcceleratorPressed(const ui::Accelerator& accelerator) {
  for (const auto& entry : kCloseWindowAccelerators) {
    if (ui::Accelerator(entry.keycode, entry.modifiers) == accelerator) {
      if (!close_callback_.is_null())
        close_callback_.Run();
      return true;
    }
  }
  return views::View::AcceleratorPressed(accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, private:

void ShellSurface::CreateShellSurfaceWidget(ui::WindowShowState show_state) {
  DCHECK(enabled());
  DCHECK(!widget_);

  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET;
  params.delegate = this;
  params.shadow_type = views::Widget::InitParams::SHADOW_TYPE_NONE;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.show_state = show_state;
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(), container_);
  params.bounds = initial_bounds_;
  bool activatable = activatable_;
  // ShellSurfaces in system modal container are only activatable if input
  // region is non-empty. See OnCommitSurface() for more details.
  if (container_ == ash::kShellWindowId_SystemModalContainer)
    activatable &= !surface_->GetHitTestBounds().IsEmpty();
  params.activatable = activatable ? views::Widget::InitParams::ACTIVATABLE_YES
                                   : views::Widget::InitParams::ACTIVATABLE_NO;

  // Note: NativeWidget owns this widget.
  widget_ = new ShellSurfaceWidget(this);
  widget_->Init(params);

  aura::Window* window = widget_->GetNativeWindow();
  window->SetName("ExoShellSurface");
  window->AddChild(surface_->window());
  window->SetEventTargeter(base::WrapUnique(new CustomWindowTargeter));
  SetApplicationId(window, &application_id_);
  SetMainSurface(window, surface_);

  // Start tracking changes to window bounds and window state.
  window->AddObserver(this);
  ash::wm::WindowState* window_state = ash::wm::GetWindowState(window);
  window_state->AddObserver(this);

  // Absolete positioned shell surfaces may request the bounds that does not
  // fill the entire work area / display in maximized / fullscreen state.
  // Allow such clients to update the bounds in these states.
  if (!initial_bounds_.IsEmpty())
    window_state->set_allow_set_bounds_in_maximized(true);

  // Notify client of initial state if different than normal.
  if (window_state->GetStateType() != ash::wm::WINDOW_STATE_TYPE_NORMAL &&
      !state_changed_callback_.is_null()) {
    state_changed_callback_.Run(ash::wm::WINDOW_STATE_TYPE_NORMAL,
                                window_state->GetStateType());
  }

  // Disable movement if initial bounds were specified.
  widget_->set_movement_disabled(!initial_bounds_.IsEmpty());
  window_state->set_ignore_keyboard_bounds_change(!initial_bounds_.IsEmpty());

  // Make shell surface a transient child if |parent_| has been set.
  if (parent_)
    wm::AddTransientChild(parent_, window);

  // Allow Ash to manage the position of a top-level shell surfaces if show
  // state is one that allows auto positioning and |initial_bounds_| has
  // not been set.
  window_state->set_window_position_managed(
      ash::wm::ToWindowShowState(ash::wm::WINDOW_STATE_TYPE_AUTO_POSITIONED) ==
          show_state &&
      initial_bounds_.IsEmpty());

  // Register close window accelerators.
  views::FocusManager* focus_manager = widget_->GetFocusManager();
  for (const auto& entry : kCloseWindowAccelerators) {
    focus_manager->RegisterAccelerator(
        ui::Accelerator(entry.keycode, entry.modifiers),
        ui::AcceleratorManager::kNormalPriority, this);
  }

  // Set delegate for handling of fullscreening.
  window_state->SetDelegate(std::unique_ptr<ash::wm::WindowStateDelegate>(
      new CustomWindowStateDelegate(widget_)));

  // Show widget next time Commit() is called.
  pending_show_widget_ = true;
}

void ShellSurface::Configure() {
  DCHECK(widget_);

  // Delay configure callback if |scoped_configure_| is set.
  if (scoped_configure_) {
    scoped_configure_->set_needs_configure();
    return;
  }

  gfx::Vector2d origin_offset = pending_origin_config_offset_;
  pending_origin_config_offset_ = gfx::Vector2d();

  // If surface is being resized, save the resize direction.
  int resize_component =
      resizer_ ? resizer_->details().window_component : HTCAPTION;

  if (configure_callback_.is_null()) {
    pending_origin_offset_ += origin_offset;
    pending_resize_component_ = resize_component;
    return;
  }

  uint32_t serial = configure_callback_.Run(
      widget_->GetWindowBoundsInScreen().size(),
      ash::wm::GetWindowState(widget_->GetNativeWindow())->GetStateType(),
      IsResizing(), widget_->IsActive());

  // Apply origin offset and resize component at the first Commit() after this
  // configure request has been acknowledged.
  pending_configs_.push_back({serial, origin_offset, resize_component});
  LOG_IF(WARNING, pending_configs_.size() > 100)
      << "Number of pending configure acks for shell surface has reached: "
      << pending_configs_.size();
}

void ShellSurface::AttemptToStartDrag(int component) {
  DCHECK(widget_);

  // Cannot start another drag if one is already taking place.
  if (resizer_)
    return;

  if (widget_->GetNativeWindow()->HasCapture())
    return;

  aura::Window* root_window = widget_->GetNativeWindow()->GetRootWindow();
  gfx::Point drag_location =
      root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot();
  aura::Window::ConvertPointToTarget(
      root_window, widget_->GetNativeWindow()->parent(), &drag_location);

  // Set the cursor before calling CreateWindowResizer(), as that will
  // eventually call LockCursor() and prevent the cursor from changing.
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window);
  DCHECK(cursor_client);

  switch (component) {
    case HTCAPTION:
      cursor_client->SetCursor(ui::kCursorPointer);
      break;
    case HTTOP:
      cursor_client->SetCursor(ui::kCursorNorthResize);
      break;
    case HTTOPRIGHT:
      cursor_client->SetCursor(ui::kCursorNorthEastResize);
      break;
    case HTRIGHT:
      cursor_client->SetCursor(ui::kCursorEastResize);
      break;
    case HTBOTTOMRIGHT:
      cursor_client->SetCursor(ui::kCursorSouthEastResize);
      break;
    case HTBOTTOM:
      cursor_client->SetCursor(ui::kCursorSouthResize);
      break;
    case HTBOTTOMLEFT:
      cursor_client->SetCursor(ui::kCursorSouthWestResize);
      break;
    case HTLEFT:
      cursor_client->SetCursor(ui::kCursorWestResize);
      break;
    case HTTOPLEFT:
      cursor_client->SetCursor(ui::kCursorNorthWestResize);
      break;
    default:
      NOTREACHED();
      break;
  }

  resizer_ = ash::CreateWindowResizer(
      ash::WmWindowAura::Get(widget_->GetNativeWindow()), drag_location,
      component, aura::client::WINDOW_MOVE_SOURCE_MOUSE);
  if (!resizer_)
    return;

  // Apply pending origin offsets and resize direction before starting a new
  // resize operation. These can still be pending if the client has acknowledged
  // the configure request but not yet called Commit().
  origin_ += pending_origin_offset_;
  pending_origin_offset_ = gfx::Vector2d();
  resize_component_ = pending_resize_component_;

  ash::Shell::GetInstance()->AddPreTargetHandler(this);
  widget_->GetNativeWindow()->SetCapture();

  // Notify client that resizing state has changed.
  if (IsResizing())
    Configure();
}

void ShellSurface::EndDrag(bool revert) {
  DCHECK(widget_);
  DCHECK(resizer_);

  bool was_resizing = IsResizing();

  if (revert)
    resizer_->RevertDrag();
  else
    resizer_->CompleteDrag();

  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
  widget_->GetNativeWindow()->ReleaseCapture();
  resizer_.reset();

  // Notify client that resizing state has changed.
  if (was_resizing)
    Configure();

  UpdateWidgetBounds();
}

bool ShellSurface::IsResizing() const {
  if (!resizer_)
    return false;

  return resizer_->details().bounds_change &
         ash::WindowResizer::kBoundsChange_Resizes;
}

gfx::Rect ShellSurface::GetVisibleBounds() const {
  // Use |geometry_| if set, otherwise use the visual bounds of the surface.
  return geometry_.IsEmpty() ? gfx::Rect(surface_->window()->layer()->size())
                             : geometry_;
}

gfx::Point ShellSurface::GetSurfaceOrigin() const {
  gfx::Rect window_bounds = widget_->GetNativeWindow()->bounds();

  // If initial bounds were specified then surface origin is always relative
  // to those bounds.
  if (!initial_bounds_.IsEmpty())
    return initial_bounds_.origin() - window_bounds.OffsetFromOrigin();

  gfx::Rect visible_bounds = GetVisibleBounds();
  switch (resize_component_) {
    case HTCAPTION:
      return origin_ - visible_bounds.OffsetFromOrigin();
    case HTBOTTOM:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
      return gfx::Point() - visible_bounds.OffsetFromOrigin();
    case HTTOP:
    case HTTOPRIGHT:
      return gfx::Point(0, window_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
      break;
    case HTLEFT:
    case HTBOTTOMLEFT:
      return gfx::Point(window_bounds.width() - visible_bounds.width(), 0) -
             visible_bounds.OffsetFromOrigin();
    case HTTOPLEFT:
      return gfx::Point(window_bounds.width() - visible_bounds.width(),
                        window_bounds.height() - visible_bounds.height()) -
             visible_bounds.OffsetFromOrigin();
    default:
      NOTREACHED();
      return gfx::Point();
  }
}

void ShellSurface::UpdateWidgetBounds() {
  DCHECK(widget_);

  // Return early if the shell is currently managing the bounds of the widget.
  // 1) When a window is either maximized/fullscreen/pinned, and the bounds
  // isn't controlled by a client.
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(widget_->GetNativeWindow());
  if (window_state->IsMaximizedOrFullscreenOrPinned() &&
      !window_state->allow_set_bounds_in_maximized()) {
    return;
  }

  // 2) When a window is being dragged.
  if (IsResizing())
    return;

  // Return early if there is pending configure requests.
  if (!pending_configs_.empty() || scoped_configure_)
    return;

  gfx::Rect visible_bounds = GetVisibleBounds();
  gfx::Rect new_widget_bounds = visible_bounds;

  // Avoid changing widget origin unless initial bounds were specificed and
  // widget origin is always relative to it.
  if (initial_bounds_.IsEmpty())
    new_widget_bounds.set_origin(widget_->GetNativeWindow()->bounds().origin());

  // Update widget origin using the surface origin if the current location of
  // surface is being anchored to one side of the widget as a result of a
  // resize operation.
  if (resize_component_ != HTCAPTION) {
    gfx::Point new_widget_origin =
        GetSurfaceOrigin() + visible_bounds.OffsetFromOrigin();
    aura::Window::ConvertPointToTarget(widget_->GetNativeWindow(),
                                       widget_->GetNativeWindow()->parent(),
                                       &new_widget_origin);
    new_widget_bounds.set_origin(new_widget_origin);
  }

  // Set |ignore_window_bounds_changes_| as this change to window bounds
  // should not result in a configure request.
  DCHECK(!ignore_window_bounds_changes_);
  ignore_window_bounds_changes_ = true;
  if (widget_->GetNativeWindow()->bounds() != new_widget_bounds)
    widget_->SetBounds(new_widget_bounds);
  ignore_window_bounds_changes_ = false;

  // A change to the widget size requires surface bounds to be re-adjusted.
  surface_->window()->SetBounds(
      gfx::Rect(GetSurfaceOrigin(), surface_->window()->layer()->size()));
}

void ShellSurface::UpdateShadow() {
  if (!widget_)
    return;
  aura::Window* window = widget_->GetNativeWindow();
  if (shadow_content_bounds_.IsEmpty()) {
    wm::SetShadowType(window, wm::SHADOW_TYPE_NONE);
    if (shadow_underlay_)
      shadow_underlay_->Hide();
  } else {
    wm::SetShadowType(window, wm::SHADOW_TYPE_RECTANGULAR);

    // TODO(oshima): Adjust the coordinates from client screen to
    // chromeos screen when multi displays are supported.
    gfx::Point origin = window->bounds().origin();
    gfx::Point shadow_origin = shadow_content_bounds_.origin();
    shadow_origin -= origin.OffsetFromOrigin();
    gfx::Rect shadow_bounds(shadow_origin, shadow_content_bounds_.size());

    // Always create and show the underlay, even in maximized/fullscreen.
    if (!shadow_underlay_) {
      shadow_underlay_ = new aura::Window(nullptr);
      DCHECK(shadow_underlay_->owned_by_parent());
      shadow_underlay_->set_ignore_events(true);
      // Ensure the background area inside the shadow is solid black.
      // Clients that provide translucent contents should not be using
      // rectangular shadows as this method requires opaque contents to
      // cast a shadow that represent it correctly.
      shadow_underlay_->Init(ui::LAYER_SOLID_COLOR);
      shadow_underlay_->layer()->SetColor(SK_ColorBLACK);
      DCHECK(shadow_underlay_->layer()->fills_bounds_opaquely());
      window->AddChild(shadow_underlay_);
      window->StackChildAtBottom(shadow_underlay_);
    }

    float shadow_underlay_opacity = rectangular_shadow_background_opacity_;
    // Put the black background layer behind the window if
    // 1) the window is in immersive fullscreen.
    // 2) the window can control the bounds of the window in fullscreen (
    //    thus the background can be visible).
    // 3) the window has no transform (the transformed background may
    //    not cover the entire background, e.g. overview mode).
    if (widget_->IsFullscreen() &&
        ash::wm::GetWindowState(window)->allow_set_bounds_in_maximized() &&
        window->layer()->transform().IsIdentity()) {
      gfx::Point origin;
      origin -= window->bounds().origin().OffsetFromOrigin();
      shadow_bounds.set_origin(origin);
      shadow_bounds.set_size(window->parent()->bounds().size());
      shadow_underlay_opacity = 1.0f;
    }

    shadow_underlay_->SetBounds(shadow_bounds);

    // TODO(oshima): Setting to the same value should be no-op.
    // crbug.com/642223.
    if (shadow_underlay_opacity !=
        shadow_underlay_->layer()->GetTargetOpacity()) {
      shadow_underlay_->layer()->SetOpacity(shadow_underlay_opacity);
    }

    shadow_underlay_->Show();

    wm::Shadow* shadow = wm::ShadowController::GetShadowForWindow(window);
    // Maximized/Fullscreen window does not create a shadow.
    if (!shadow)
      return;

    if (!shadow_overlay_) {
      shadow_overlay_ = new aura::Window(nullptr);
      DCHECK(shadow_overlay_->owned_by_parent());
      shadow_overlay_->set_ignore_events(true);
      shadow_overlay_->Init(ui::LAYER_NOT_DRAWN);
      shadow_overlay_->layer()->Add(shadow->layer());
      window->AddChild(shadow_overlay_);
      shadow_overlay_->Show();
    }
    shadow_overlay_->SetBounds(shadow_bounds);
    shadow->SetContentBounds(gfx::Rect(shadow_bounds.size()));
  }
}

}  // namespace exo
