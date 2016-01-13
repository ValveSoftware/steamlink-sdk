// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host.h"

#include "base/debug/trace_event.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/dip_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size_conversions.h"

namespace aura {

const char kWindowTreeHostForAcceleratedWidget[] =
    "__AURA_WINDOW_TREE_HOST_ACCELERATED_WIDGET__";

float GetDeviceScaleFactorFromDisplay(Window* window) {
  gfx::Display display = gfx::Screen::GetScreenFor(window)->
      GetDisplayNearestWindow(window);
  DCHECK(display.is_valid());
  return display.device_scale_factor();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, public:

WindowTreeHost::~WindowTreeHost() {
  DCHECK(!compositor_) << "compositor must be destroyed before root window";
}

#if defined(OS_ANDROID)
// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  // This is only hit for tests and ash, right now these aren't an issue so
  // adding the CHECK.
  // TODO(sky): decide if we want a factory.
  CHECK(false);
  return NULL;
}
#endif

// static
WindowTreeHost* WindowTreeHost::GetForAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  return reinterpret_cast<WindowTreeHost*>(
      ui::ViewProp::GetValue(widget, kWindowTreeHostForAcceleratedWidget));
}

void WindowTreeHost::InitHost() {
  InitCompositor();
  UpdateRootWindowSize(GetBounds().size());
  Env::GetInstance()->NotifyHostInitialized(this);
  window()->Show();
}

void WindowTreeHost::InitCompositor() {
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(window()),
                               GetBounds().size());
  compositor_->SetRootLayer(window()->layer());
}

void WindowTreeHost::AddObserver(WindowTreeHostObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowTreeHost::RemoveObserver(WindowTreeHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

ui::EventProcessor* WindowTreeHost::event_processor() {
  return dispatcher();
}

gfx::Transform WindowTreeHost::GetRootTransform() const {
  float scale = ui::GetDeviceScaleFactor(window()->layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= window()->layer()->transform();
  return transform;
}

void WindowTreeHost::SetRootTransform(const gfx::Transform& transform) {
  window()->SetTransform(transform);
  UpdateRootWindowSize(GetBounds().size());
}

gfx::Transform WindowTreeHost::GetInverseRootTransform() const {
  gfx::Transform invert;
  gfx::Transform transform = GetRootTransform();
  if (!transform.GetInverse(&invert))
    return transform;
  return invert;
}

void WindowTreeHost::UpdateRootWindowSize(const gfx::Size& host_size) {
  gfx::Rect bounds(host_size);
  gfx::RectF new_bounds(ui::ConvertRectToDIP(window()->layer(), bounds));
  window()->layer()->transform().TransformRect(&new_bounds);
  window()->SetBounds(gfx::Rect(gfx::ToFlooredSize(new_bounds.size())));
}

void WindowTreeHost::ConvertPointToNativeScreen(gfx::Point* point) const {
  ConvertPointToHost(point);
  gfx::Point location = GetLocationOnNativeScreen();
  point->Offset(location.x(), location.y());
}

void WindowTreeHost::ConvertPointFromNativeScreen(gfx::Point* point) const {
  gfx::Point location = GetLocationOnNativeScreen();
  point->Offset(-location.x(), -location.y());
  ConvertPointFromHost(point);
}

void WindowTreeHost::ConvertPointToHost(gfx::Point* point) const {
  gfx::Point3F point_3f(*point);
  GetRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void WindowTreeHost::ConvertPointFromHost(gfx::Point* point) const {
  gfx::Point3F point_3f(*point);
  GetInverseRootTransform().TransformPoint(&point_3f);
  *point = gfx::ToFlooredPoint(point_3f.AsPointF());
}

void WindowTreeHost::SetCursor(gfx::NativeCursor cursor) {
  last_cursor_ = cursor;
  // A lot of code seems to depend on NULL cursors actually showing an arrow,
  // so just pass everything along to the host.
  SetCursorNative(cursor);
}

void WindowTreeHost::OnCursorVisibilityChanged(bool show) {
  // Clear any existing mouse hover effects when the cursor becomes invisible.
  // Note we do not need to dispatch a mouse enter when the cursor becomes
  // visible because that can only happen in response to a mouse event, which
  // will trigger its own mouse enter.
  if (!show) {
    dispatcher()->DispatchMouseExitAtPoint(
        dispatcher()->GetLastMouseLocationInRoot());
  }

  OnCursorVisibilityChangedNative(show);
}

void WindowTreeHost::MoveCursorTo(const gfx::Point& location_in_dip) {
  gfx::Point host_location(location_in_dip);
  ConvertPointToHost(&host_location);
  MoveCursorToInternal(location_in_dip, host_location);
}

void WindowTreeHost::MoveCursorToHostLocation(const gfx::Point& host_location) {
  gfx::Point root_location(host_location);
  ConvertPointFromHost(&root_location);
  MoveCursorToInternal(root_location, host_location);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, protected:

WindowTreeHost::WindowTreeHost()
    : window_(new Window(NULL)),
      last_cursor_(ui::kCursorNull) {
}

void WindowTreeHost::DestroyCompositor() {
  compositor_.reset();
}

void WindowTreeHost::DestroyDispatcher() {
  delete window_;
  window_ = NULL;
  dispatcher_.reset();

  // TODO(beng): this comment is no longer quite valid since this function
  // isn't called from WED, and WED isn't a subclass of Window. So it seems
  // like we could just rely on ~Window now.
  // Destroy child windows while we're still valid. This is also done by
  // ~Window, but by that time any calls to virtual methods overriden here (such
  // as GetRootWindow()) result in Window's implementation. By destroying here
  // we ensure GetRootWindow() still returns this.
  //window()->RemoveOrDestroyChildren();
}

void WindowTreeHost::CreateCompositor(
    gfx::AcceleratedWidget accelerated_widget) {
  DCHECK(Env::GetInstance());
  ui::ContextFactory* context_factory = Env::GetInstance()->context_factory();
  DCHECK(context_factory);
  compositor_.reset(
      new ui::Compositor(GetAcceleratedWidget(), context_factory));
  // TODO(beng): I think this setup should probably all move to a "accelerated
  // widget available" function.
  if (!dispatcher()) {
    window()->Init(WINDOW_LAYER_NOT_DRAWN);
    window()->set_host(this);
    window()->SetName("RootWindow");
    window()->SetEventTargeter(
        scoped_ptr<ui::EventTargeter>(new WindowTargeter()));
    prop_.reset(new ui::ViewProp(GetAcceleratedWidget(),
                                 kWindowTreeHostForAcceleratedWidget,
                                 this));
    dispatcher_.reset(new WindowEventDispatcher(this));
  }
}

void WindowTreeHost::OnHostMoved(const gfx::Point& new_location) {
  TRACE_EVENT1("ui", "WindowTreeHost::OnHostMoved",
               "origin", new_location.ToString());

  FOR_EACH_OBSERVER(WindowTreeHostObserver, observers_,
                    OnHostMoved(this, new_location));
}

void WindowTreeHost::OnHostResized(const gfx::Size& new_size) {
  // The compositor should have the same size as the native root window host.
  // Get the latest scale from display because it might have been changed.
  compositor_->SetScaleAndSize(GetDeviceScaleFactorFromDisplay(window()),
                               new_size);

  gfx::Size layer_size = GetBounds().size();
  // The layer, and the observers should be notified of the
  // transformed size of the root window.
  UpdateRootWindowSize(layer_size);
  FOR_EACH_OBSERVER(WindowTreeHostObserver, observers_, OnHostResized(this));
}

void WindowTreeHost::OnHostCloseRequested() {
  FOR_EACH_OBSERVER(WindowTreeHostObserver, observers_,
                    OnHostCloseRequested(this));
}

void WindowTreeHost::OnHostActivated() {
  Env::GetInstance()->NotifyHostActivated(this);
}

void WindowTreeHost::OnHostLostWindowCapture() {
  Window* capture_window = client::GetCaptureWindow(window());
  if (capture_window && capture_window->GetRootWindow() == window())
    capture_window->ReleaseCapture();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHost, private:

void WindowTreeHost::MoveCursorToInternal(const gfx::Point& root_location,
                                          const gfx::Point& host_location) {
  last_cursor_request_position_in_host_ = host_location;
  MoveCursorToNative(host_location);
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client) {
    const gfx::Display& display =
        gfx::Screen::GetScreenFor(window())->GetDisplayNearestWindow(window());
    cursor_client->SetDisplay(display);
  }
  dispatcher()->OnCursorMovedToRootLocation(root_location);
}

}  // namespace aura
