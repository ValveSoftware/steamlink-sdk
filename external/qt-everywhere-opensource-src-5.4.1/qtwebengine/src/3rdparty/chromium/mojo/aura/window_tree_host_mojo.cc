// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/window_tree_host_mojo.h"

#include <vector>

#include "mojo/aura/window_tree_host_mojo_delegate.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace mojo {
namespace {

const char kTreeHostsKey[] = "tree_hosts";

typedef std::vector<WindowTreeHostMojo*> Managers;

class TreeHosts : public base::SupportsUserData::Data {
 public:
  TreeHosts() {}
  virtual ~TreeHosts() {}

  static TreeHosts* Get() {
    TreeHosts* hosts = static_cast<TreeHosts*>(
        aura::Env::GetInstance()->GetUserData(kTreeHostsKey));
    if (!hosts) {
      hosts = new TreeHosts;
      aura::Env::GetInstance()->SetUserData(kTreeHostsKey, hosts);
    }
    return hosts;
  }

  void Add(WindowTreeHostMojo* manager) {
    managers_.push_back(manager);
  }

  void Remove(WindowTreeHostMojo* manager) {
    Managers::iterator i = std::find(managers_.begin(), managers_.end(),
                                     manager);
    DCHECK(i != managers_.end());
    managers_.erase(i);
  }

  const std::vector<WindowTreeHostMojo*> managers() const {
    return managers_;
  }

 private:
  Managers managers_;

  DISALLOW_COPY_AND_ASSIGN(TreeHosts);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, public:

WindowTreeHostMojo::WindowTreeHostMojo(view_manager::Node* node,
                                       WindowTreeHostMojoDelegate* delegate)
    : node_(node),
      bounds_(node->bounds()),
      delegate_(delegate) {
  node_->AddObserver(this);
  CreateCompositor(GetAcceleratedWidget());

  TreeHosts::Get()->Add(this);
}

WindowTreeHostMojo::~WindowTreeHostMojo() {
  node_->RemoveObserver(this);
  TreeHosts::Get()->Remove(this);
  DestroyCompositor();
  DestroyDispatcher();
}

// static
WindowTreeHostMojo* WindowTreeHostMojo::ForCompositor(
    ui::Compositor* compositor) {
  const Managers& managers = TreeHosts::Get()->managers();
  for (size_t i = 0; i < managers.size(); ++i) {
    if (managers[i]->compositor() == compositor)
      return managers[i];
  }
  return NULL;
}

void WindowTreeHostMojo::SetContents(const SkBitmap& contents) {
  delegate_->CompositorContentsChanged(contents);
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, aura::WindowTreeHost implementation:

ui::EventSource* WindowTreeHostMojo::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostMojo::GetAcceleratedWidget() {
  return gfx::kNullAcceleratedWidget;
}

void WindowTreeHostMojo::Show() {
  window()->Show();
}

void WindowTreeHostMojo::Hide() {
}

gfx::Rect WindowTreeHostMojo::GetBounds() const {
  return bounds_;
}

void WindowTreeHostMojo::SetBounds(const gfx::Rect& bounds) {
  window()->SetBounds(gfx::Rect(bounds.size()));
}

gfx::Point WindowTreeHostMojo::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void WindowTreeHostMojo::SetCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::PostNativeEvent(
    const base::NativeEvent& native_event) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::SetCursorNative(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::MoveCursorToNative(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, ui::EventSource implementation:

ui::EventProcessor* WindowTreeHostMojo::GetEventProcessor() {
  return dispatcher();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, view_manager::NodeObserver implementation:

void WindowTreeHostMojo::OnNodeBoundsChange(
    view_manager::Node* node,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    view_manager::NodeObserver::DispositionChangePhase phase) {
  bounds_ = new_bounds;
  if (old_bounds.origin() != new_bounds.origin())
    OnHostMoved(bounds_.origin());
  if (old_bounds.size() != new_bounds.size())
    OnHostResized(bounds_.size());
}

}  // namespace mojo
