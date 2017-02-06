// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/platform_display.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "cc/ipc/quads.mojom.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/surface_draw_quad.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/surfaces/display_compositor.h"
#include "components/mus/surfaces/surfaces_state.h"
#include "components/mus/ws/platform_display_factory.h"
#include "components/mus/ws/server_window.h"
#include "components/mus/ws/server_window_surface.h"
#include "components/mus/ws/server_window_surface_manager.h"
#include "components/mus/ws/window_coordinate_conversions.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "third_party/skia/include/core/SkXfermode.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/platform_window/platform_ime_controller.h"
#include "ui/platform_window/platform_window.h"

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#elif defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#elif defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#elif defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace mus {

namespace ws {
namespace {

// DrawWindowTree recursively visits ServerWindows, creating a SurfaceDrawQuad
// for each that lacks one.
void DrawWindowTree(cc::RenderPass* pass,
                    ServerWindow* window,
                    const gfx::Vector2d& parent_to_root_origin_offset,
                    float opacity) {
  if (!window->visible())
    return;

  ServerWindowSurface* default_surface =
      window->surface_manager() ? window->surface_manager()->GetDefaultSurface()
                                : nullptr;

  const gfx::Rect absolute_bounds =
      window->bounds() + parent_to_root_origin_offset;
  std::vector<ServerWindow*> children(window->GetChildren());
  // TODO(rjkroege, fsamuel): Make sure we're handling alpha correctly.
  const float combined_opacity = opacity * window->opacity();
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    DrawWindowTree(pass, *it, absolute_bounds.OffsetFromOrigin(),
                   combined_opacity);
  }

  if (!window->surface_manager() || !window->surface_manager()->ShouldDraw())
    return;

  ServerWindowSurface* underlay_surface =
      window->surface_manager()->GetUnderlaySurface();
  if (!default_surface && !underlay_surface)
    return;

  if (default_surface) {
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(absolute_bounds.x(),
                                       absolute_bounds.y());

    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();

    const gfx::Rect bounds_at_origin(window->bounds().size());
    // TODO(fsamuel): These clipping and visible rects are incorrect. They need
    // to be populated from CompositorFrame structs.
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                window->opacity(), SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);
    auto quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 default_surface->id());
  }
  if (underlay_surface) {
    const gfx::Rect underlay_absolute_bounds =
        absolute_bounds - window->underlay_offset();
    gfx::Transform quad_to_target_transform;
    quad_to_target_transform.Translate(underlay_absolute_bounds.x(),
                                       underlay_absolute_bounds.y());
    cc::SharedQuadState* sqs = pass->CreateAndAppendSharedQuadState();
    const gfx::Rect bounds_at_origin(
        underlay_surface->last_submitted_frame_size());
    sqs->SetAll(quad_to_target_transform,
                bounds_at_origin.size() /* layer_bounds */,
                bounds_at_origin /* visible_layer_bounds */,
                bounds_at_origin /* clip_rect */, false /* is_clipped */,
                window->opacity(), SkXfermode::kSrcOver_Mode,
                0 /* sorting-context_id */);

    auto quad = pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
    quad->SetAll(sqs, bounds_at_origin /* rect */,
                 gfx::Rect() /* opaque_rect */,
                 bounds_at_origin /* visible_rect */, true /* needs_blending*/,
                 underlay_surface->id());
  }
}

}  // namespace

// static
PlatformDisplayFactory* PlatformDisplay::factory_ = nullptr;

// static
PlatformDisplay* PlatformDisplay::Create(
    const PlatformDisplayInitParams& init_params) {
  if (factory_)
    return factory_->CreatePlatformDisplay();

  return new DefaultPlatformDisplay(init_params);
}

DefaultPlatformDisplay::DefaultPlatformDisplay(
    const PlatformDisplayInitParams& init_params)
    : display_id_(init_params.display_id),
      gpu_state_(init_params.gpu_state),
      surfaces_state_(init_params.surfaces_state),
      delegate_(nullptr),
      draw_timer_(false, false),
      frame_pending_(false),
#if !defined(OS_ANDROID)
      cursor_loader_(ui::CursorLoader::Create()),
#endif
      weak_factory_(this) {
  metrics_.size_in_pixels = init_params.display_bounds.size();
  // TODO(rjkroege): Preserve the display_id when Ozone platform can use it.
}

void DefaultPlatformDisplay::Init(PlatformDisplayDelegate* delegate) {
  delegate_ = delegate;

  gfx::Rect bounds(metrics_.size_in_pixels);
#if defined(OS_WIN)
  platform_window_.reset(new ui::WinWindow(this, bounds));
#elif defined(USE_X11)
  platform_window_.reset(new ui::X11Window(this));
#elif defined(OS_ANDROID)
  platform_window_.reset(new ui::PlatformWindowAndroid(this));
#elif defined(USE_OZONE)
  platform_window_ =
      ui::OzonePlatform::GetInstance()->CreatePlatformWindow(this, bounds);
#else
  NOTREACHED() << "Unsupported platform";
#endif
  platform_window_->SetBounds(bounds);
  platform_window_->Show();
}

DefaultPlatformDisplay::~DefaultPlatformDisplay() {
  // Don't notify the delegate from the destructor.
  delegate_ = nullptr;

  // Invalidate WeakPtrs now to avoid callbacks back into the
  // DefaultPlatformDisplay during destruction of |display_compositor_|.
  weak_factory_.InvalidateWeakPtrs();
  display_compositor_.reset();
  // Destroy the PlatformWindow early on as it may call us back during
  // destruction and we want to be in a known state. But destroy the surface
  // first because it can still be using the platform window.
  platform_window_.reset();
}

void DefaultPlatformDisplay::SchedulePaint(const ServerWindow* window,
                                           const gfx::Rect& bounds) {
  DCHECK(window);
  if (!window->IsDrawn())
    return;
  const gfx::Rect root_relative_rect =
      ConvertRectBetweenWindows(window, delegate_->GetRootWindow(), bounds);
  if (root_relative_rect.IsEmpty())
    return;
  dirty_rect_.Union(root_relative_rect);
  WantToDraw();
}

void DefaultPlatformDisplay::SetViewportSize(const gfx::Size& size) {
  platform_window_->SetBounds(gfx::Rect(size));
}

void DefaultPlatformDisplay::SetTitle(const base::string16& title) {
  platform_window_->SetTitle(title);
}

void DefaultPlatformDisplay::SetCapture() {
  platform_window_->SetCapture();
}

void DefaultPlatformDisplay::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

void DefaultPlatformDisplay::SetCursorById(int32_t cursor_id) {
#if !defined(OS_ANDROID)
  // TODO(erg): This still isn't sufficient, and will only use native cursors
  // that chrome would use, not custom image cursors. For that, we should
  // delegate to the window manager to load images from resource packs.
  //
  // We probably also need to deal with different DPIs.
  ui::Cursor cursor(cursor_id);
  cursor_loader_->SetPlatformCursor(&cursor);
  platform_window_->SetCursor(cursor.platform());
#endif
}

float DefaultPlatformDisplay::GetDeviceScaleFactor() {
  return metrics_.device_scale_factor;
}

mojom::Rotation DefaultPlatformDisplay::GetRotation() {
  // TODO(sky): implement me.
  return mojom::Rotation::VALUE_0;
}

void DefaultPlatformDisplay::UpdateTextInputState(
    const ui::TextInputState& state) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->UpdateTextInputState(state);
}

void DefaultPlatformDisplay::SetImeVisibility(bool visible) {
  ui::PlatformImeController* ime = platform_window_->GetPlatformImeController();
  if (ime)
    ime->SetImeVisibility(visible);
}

void DefaultPlatformDisplay::Draw() {
  if (!delegate_->GetRootWindow()->visible())
    return;

  // TODO(fsamuel): We should add a trace for generating a top level frame.
  cc::CompositorFrame frame(GenerateCompositorFrame());
  frame_pending_ = true;
  if (display_compositor_) {
    display_compositor_->SubmitCompositorFrame(
        std::move(frame), base::Bind(&DefaultPlatformDisplay::DidDraw,
                                     weak_factory_.GetWeakPtr()));
  }
  dirty_rect_ = gfx::Rect();
}

void DefaultPlatformDisplay::DidDraw(cc::SurfaceDrawStatus status) {
  frame_pending_ = false;
  delegate_->OnCompositorFrameDrawn();
  if (!dirty_rect_.IsEmpty())
    WantToDraw();
}

bool DefaultPlatformDisplay::IsFramePending() const {
  return frame_pending_;
}

int64_t DefaultPlatformDisplay::GetDisplayId() const {
  return display_id_;
}

void DefaultPlatformDisplay::WantToDraw() {
  if (draw_timer_.IsRunning() || frame_pending_)
    return;

  // TODO(rjkroege): Use vblank to kick off Draw.
  draw_timer_.Start(
      FROM_HERE, base::TimeDelta(),
      base::Bind(&DefaultPlatformDisplay::Draw, weak_factory_.GetWeakPtr()));
}

void DefaultPlatformDisplay::UpdateMetrics(const gfx::Size& size,
                                           float device_scale_factor) {
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();
  if (metrics_.size_in_pixels == size &&
      metrics_.device_scale_factor == device_scale_factor)
    return;

  ViewportMetrics old_metrics = metrics_;
  metrics_.size_in_pixels = size;
  metrics_.device_scale_factor = device_scale_factor;
  delegate_->OnViewportMetricsChanged(old_metrics, metrics_);
}

cc::CompositorFrame DefaultPlatformDisplay::GenerateCompositorFrame() {
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->damage_rect = dirty_rect_;
  render_pass->output_rect = gfx::Rect(metrics_.size_in_pixels);

  DrawWindowTree(render_pass.get(), delegate_->GetRootWindow(), gfx::Vector2d(),
                 1.0f);

  std::unique_ptr<cc::DelegatedFrameData> frame_data(
      new cc::DelegatedFrameData);
  frame_data->render_pass_list.push_back(std::move(render_pass));

  cc::CompositorFrame frame;
  frame.delegated_frame_data = std::move(frame_data);
  return frame;
}

void DefaultPlatformDisplay::OnBoundsChanged(const gfx::Rect& new_bounds) {
  UpdateMetrics(new_bounds.size(), metrics_.device_scale_factor);
}

void DefaultPlatformDisplay::OnDamageRect(const gfx::Rect& damaged_region) {
  dirty_rect_.Union(damaged_region);
  WantToDraw();
}

void DefaultPlatformDisplay::DispatchEvent(ui::Event* event) {
  if (event->IsScrollEvent()) {
    // TODO(moshayedi): crbug.com/602859. Dispatch scroll events as
    // they are once we have proper support for scroll events.
    delegate_->OnEvent(ui::MouseWheelEvent(*event->AsScrollEvent()));
  } else if (event->IsMouseEvent() && !event->IsMouseWheelEvent()) {
    delegate_->OnEvent(ui::PointerEvent(*event->AsMouseEvent()));
  } else if (event->IsTouchEvent()) {
    delegate_->OnEvent(ui::PointerEvent(*event->AsTouchEvent()));
  } else {
    delegate_->OnEvent(*event);
  }

#if defined(USE_X11) || defined(USE_OZONE)
  // We want to emulate the WM_CHAR generation behaviour of Windows.
  //
  // On Linux, we've previously inserted characters by having
  // InputMethodAuraLinux take all key down events and send a character event
  // to the TextInputClient. This causes a mismatch in code that has to be
  // shared between Windows and Linux, including blink code. Now that we're
  // trying to have one way of doing things, we need to standardize on and
  // emulate Windows character events.
  //
  // This is equivalent to what we're doing in the current Linux port, but
  // done once instead of done multiple times in different places.
  if (event->type() == ui::ET_KEY_PRESSED) {
    ui::KeyEvent* key_press_event = event->AsKeyEvent();
    ui::KeyEvent char_event(key_press_event->GetCharacter(),
                            key_press_event->key_code(),
                            key_press_event->flags());
    DCHECK_EQ(key_press_event->GetCharacter(), char_event.GetCharacter());
    DCHECK_EQ(key_press_event->key_code(), char_event.key_code());
    DCHECK_EQ(key_press_event->flags(), char_event.flags());
    delegate_->OnEvent(char_event);
  }
#endif
}

void DefaultPlatformDisplay::OnCloseRequest() {
  platform_window_->Close();
}

void DefaultPlatformDisplay::OnClosed() {
  if (delegate_)
    delegate_->OnDisplayClosed();
}

void DefaultPlatformDisplay::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {}

void DefaultPlatformDisplay::OnLostCapture() {
  delegate_->OnNativeCaptureLost();
}

void DefaultPlatformDisplay::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_scale_factor) {
  if (widget != gfx::kNullAcceleratedWidget) {
    display_compositor_.reset(
        new DisplayCompositor(base::ThreadTaskRunnerHandle::Get(), widget,
                              gpu_state_, surfaces_state_));
  }
  UpdateMetrics(metrics_.size_in_pixels, device_scale_factor);
}

void DefaultPlatformDisplay::OnAcceleratedWidgetDestroyed() {
  NOTREACHED();
}

void DefaultPlatformDisplay::OnActivationChanged(bool active) {}

void DefaultPlatformDisplay::RequestCopyOfOutput(
    std::unique_ptr<cc::CopyOutputRequest> output_request) {
  if (display_compositor_)
    display_compositor_->RequestCopyOfOutput(std::move(output_request));
}

}  // namespace ws

}  // namespace mus
