// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/ws/server_window.h"

#include <inttypes.h>
#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "components/mus/common/transient_window_utils.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "components/mus/ws/server_window_delegate.h"
#include "components/mus/ws/server_window_observer.h"
#include "components/mus/ws/server_window_surface_manager.h"

namespace mus {

namespace ws {

ServerWindow::ServerWindow(ServerWindowDelegate* delegate, const WindowId& id)
    : ServerWindow(delegate, id, Properties()) {}

ServerWindow::ServerWindow(ServerWindowDelegate* delegate,
                           const WindowId& id,
                           const Properties& properties)
    : delegate_(delegate),
      id_(id),
      parent_(nullptr),
      stacking_target_(nullptr),
      transient_parent_(nullptr),
      is_modal_(false),
      visible_(false),
      cursor_id_(mojom::Cursor::CURSOR_NULL),
      non_client_cursor_id_(mojom::Cursor::CURSOR_NULL),
      opacity_(1),
      can_focus_(true),
      properties_(properties),
      // Don't notify newly added observers during notification. This causes
      // problems for code that adds an observer as part of an observer
      // notification (such as ServerWindowDrawTracker).
      observers_(
          base::ObserverList<ServerWindowObserver>::NOTIFY_EXISTING_ONLY) {
  DCHECK(delegate);  // Must provide a delegate.
}

ServerWindow::~ServerWindow() {
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_, OnWindowDestroying(this));

  if (transient_parent_)
    transient_parent_->RemoveTransientWindow(this);

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  Windows transient_children(transient_children_);
  STLDeleteElements(&transient_children);
  DCHECK(transient_children_.empty());

  while (!children_.empty())
    children_.front()->parent()->Remove(children_.front());

  if (parent_)
    parent_->Remove(this);

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_, OnWindowDestroyed(this));
}

void ServerWindow::AddObserver(ServerWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void ServerWindow::RemoveObserver(ServerWindowObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

void ServerWindow::CreateSurface(mojom::SurfaceType surface_type,
                                 mojo::InterfaceRequest<mojom::Surface> request,
                                 mojom::SurfaceClientPtr client) {
  GetOrCreateSurfaceManager()->CreateSurface(surface_type, std::move(request),
                                             std::move(client));
}

void ServerWindow::Add(ServerWindow* child) {
  // We assume validation checks happened already.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(!child->Contains(this));
  if (child->parent() == this) {
    if (children_.size() == 1)
      return;  // Already in the right position.
    child->Reorder(children_.back(), mojom::OrderDirection::ABOVE);
    return;
  }

  ServerWindow* old_parent = child->parent();
  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWillChangeWindowHierarchy(child, this, old_parent));

  if (child->parent())
    child->parent()->RemoveImpl(child);

  child->parent_ = this;
  children_.push_back(child);

  // Stack the child properly if it is a transient child of a sibling.
  if (child->transient_parent_ && child->transient_parent_->parent() == this)
    RestackTransientDescendants(child->transient_parent_, &GetStackingTarget,
                                &ReorderImpl);

  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWindowHierarchyChanged(child, this, old_parent));
}

void ServerWindow::Remove(ServerWindow* child) {
  // We assume validation checks happened else where.
  DCHECK(child);
  DCHECK(child != this);
  DCHECK(child->parent() == this);

  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWillChangeWindowHierarchy(child, nullptr, this));
  RemoveImpl(child);

  // Stack the child properly if it is a transient child of a sibling.
  if (child->transient_parent_ && child->transient_parent_->parent() == this)
    RestackTransientDescendants(child->transient_parent_, &GetStackingTarget,
                                &ReorderImpl);

  FOR_EACH_OBSERVER(ServerWindowObserver, child->observers_,
                    OnWindowHierarchyChanged(child, nullptr, this));
}

void ServerWindow::Reorder(ServerWindow* relative,
                           mojom::OrderDirection direction) {
  ReorderImpl(this, relative, direction);
}

void ServerWindow::StackChildAtBottom(ServerWindow* child) {
  // There's nothing to do if the child is already at the bottom.
  if (children_.size() <= 1 || child == children_.front())
    return;
  child->Reorder(children_.front(), mojom::OrderDirection::BELOW);
}

void ServerWindow::StackChildAtTop(ServerWindow* child) {
  // There's nothing to do if the child is already at the top.
  if (children_.size() <= 1 || child == children_.back())
    return;
  child->Reorder(children_.back(), mojom::OrderDirection::ABOVE);
}

void ServerWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;

  // TODO(fsamuel): figure out how will this work with CompositorFrames.

  const gfx::Rect old_bounds = bounds_;
  bounds_ = bounds;
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowBoundsChanged(this, old_bounds, bounds));
}

void ServerWindow::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {
  if (client_area_ == insets &&
      additional_client_areas == additional_client_areas_) {
    return;
  }

  additional_client_areas_ = additional_client_areas;
  client_area_ = insets;
  FOR_EACH_OBSERVER(
      ServerWindowObserver, observers_,
      OnWindowClientAreaChanged(this, insets, additional_client_areas));
}

void ServerWindow::SetHitTestMask(const gfx::Rect& mask) {
  hit_test_mask_.reset(new gfx::Rect(mask));
}

void ServerWindow::ClearHitTestMask() {
  hit_test_mask_.reset();
}

const ServerWindow* ServerWindow::GetRoot() const {
  return delegate_->GetRootWindow(this);
}

std::vector<const ServerWindow*> ServerWindow::GetChildren() const {
  std::vector<const ServerWindow*> children;
  children.reserve(children_.size());
  for (size_t i = 0; i < children_.size(); ++i)
    children.push_back(children_[i]);
  return children;
}

std::vector<ServerWindow*> ServerWindow::GetChildren() {
  // TODO(sky): rename to children() and fix return type.
  return children_;
}

ServerWindow* ServerWindow::GetChildWindow(const WindowId& window_id) {
  if (id_ == window_id)
    return this;

  for (ServerWindow* child : children_) {
    ServerWindow* window = child->GetChildWindow(window_id);
    if (window)
      return window;
  }

  return nullptr;
}

bool ServerWindow::AddTransientWindow(ServerWindow* child) {
  // A system modal window cannot become a transient child.
  if (child->is_modal() && !child->transient_parent())
    return false;

  if (child->transient_parent())
    child->transient_parent()->RemoveTransientWindow(child);

  DCHECK(std::find(transient_children_.begin(), transient_children_.end(),
                   child) == transient_children_.end());
  transient_children_.push_back(child);
  child->transient_parent_ = this;

  // Restack |child| properly above its transient parent, if they share the same
  // parent.
  if (child->parent() == parent())
    RestackTransientDescendants(this, &GetStackingTarget, &ReorderImpl);

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnTransientWindowAdded(this, child));
  return true;
}

void ServerWindow::RemoveTransientWindow(ServerWindow* child) {
  Windows::iterator i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  DCHECK_EQ(this, child->transient_parent());
  child->transient_parent_ = nullptr;

  // If |child| and its former transient parent share the same parent, |child|
  // should be restacked properly so it is not among transient children of its
  // former parent, anymore.
  if (parent() == child->parent())
    RestackTransientDescendants(this, &GetStackingTarget, &ReorderImpl);

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnTransientWindowRemoved(this, child));
}

void ServerWindow::SetModal() {
  is_modal_ = true;
}

bool ServerWindow::Contains(const ServerWindow* window) const {
  for (const ServerWindow* parent = window; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

void ServerWindow::SetVisible(bool value) {
  if (visible_ == value)
    return;

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWillChangeWindowVisibility(this));
  visible_ = value;
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowVisibilityChanged(this));
}

void ServerWindow::SetOpacity(float value) {
  if (value == opacity_)
    return;
  float old_opacity = opacity_;
  opacity_ = value;
  delegate_->OnScheduleWindowPaint(this);
  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowOpacityChanged(this, old_opacity, opacity_));
}

void ServerWindow::SetPredefinedCursor(mus::mojom::Cursor value) {
  if (value == cursor_id_)
    return;
  cursor_id_ = value;
  FOR_EACH_OBSERVER(
      ServerWindowObserver, observers_,
      OnWindowPredefinedCursorChanged(this, static_cast<int32_t>(value)));
}

void ServerWindow::SetNonClientCursor(mus::mojom::Cursor value) {
  if (value == non_client_cursor_id_)
    return;
  non_client_cursor_id_ = value;
  FOR_EACH_OBSERVER(
      ServerWindowObserver, observers_,
      OnWindowNonClientCursorChanged(this, static_cast<int32_t>(value)));
}

void ServerWindow::SetTransform(const gfx::Transform& transform) {
  if (transform_ == transform)
    return;

  transform_ = transform;
  delegate_->OnScheduleWindowPaint(this);
}

void ServerWindow::SetProperty(const std::string& name,
                               const std::vector<uint8_t>* value) {
  auto it = properties_.find(name);
  if (it != properties_.end()) {
    if (value && it->second == *value)
      return;
  } else if (!value) {
    // This property isn't set in |properties_| and |value| is nullptr, so
    // there's
    // no change.
    return;
  }

  if (value) {
    properties_[name] = *value;
  } else if (it != properties_.end()) {
    properties_.erase(it);
  }

  FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                    OnWindowSharedPropertyChanged(this, name, value));
}

std::string ServerWindow::GetName() const {
  auto it = properties_.find(mojom::WindowManager::kName_Property);
  if (it == properties_.end())
    return std::string();
  return std::string(it->second.begin(), it->second.end());
}

void ServerWindow::SetTextInputState(const ui::TextInputState& state) {
  const bool changed = !(text_input_state_ == state);
  if (changed) {
    text_input_state_ = state;
    // keyboard even if the state is not changed. So we have to notify
    // |observers_|.
    FOR_EACH_OBSERVER(ServerWindowObserver, observers_,
                      OnWindowTextInputStateChanged(this, state));
  }
}

bool ServerWindow::IsDrawn() const {
  const ServerWindow* root = delegate_->GetRootWindow(this);
  if (!root || !root->visible())
    return false;
  const ServerWindow* window = this;
  while (window && window != root && window->visible())
    window = window->parent();
  return root == window;
}

void ServerWindow::DestroySurfacesScheduledForDestruction() {
  if (!surface_manager_)
    return;
  ServerWindowSurface* surface = surface_manager_->GetDefaultSurface();
  if (surface)
    surface->DestroySurfacesScheduledForDestruction();

  surface = surface_manager_->GetUnderlaySurface();
  if (surface)
    surface->DestroySurfacesScheduledForDestruction();
}

ServerWindowSurfaceManager* ServerWindow::GetOrCreateSurfaceManager() {
  if (!surface_manager_.get())
    surface_manager_.reset(new ServerWindowSurfaceManager(this));
  return surface_manager_.get();
}

void ServerWindow::SetUnderlayOffset(const gfx::Vector2d& offset) {
  if (offset == underlay_offset_)
    return;

  underlay_offset_ = offset;
  delegate_->OnScheduleWindowPaint(this);
}

#if !defined(NDEBUG)
std::string ServerWindow::GetDebugWindowHierarchy() const {
  std::string result;
  BuildDebugInfo(std::string(), &result);
  return result;
}

void ServerWindow::BuildDebugInfo(const std::string& depth,
                                  std::string* result) const {
  std::string name = GetName();
  *result += base::StringPrintf(
      "%sid=%d,%d visible=%s bounds=%d,%d %dx%d %s\n", depth.c_str(),
      static_cast<int>(id_.client_id), static_cast<int>(id_.window_id),
      visible_ ? "true" : "false", bounds_.x(), bounds_.y(), bounds_.width(),
      bounds_.height(), !name.empty() ? name.c_str() : "(no name)");
  for (const ServerWindow* child : children_)
    child->BuildDebugInfo(depth + "  ", result);
}
#endif

void ServerWindow::RemoveImpl(ServerWindow* window) {
  window->parent_ = nullptr;
  children_.erase(std::find(children_.begin(), children_.end(), window));
}

void ServerWindow::OnStackingChanged() {
  if (stacking_target_) {
    Windows::const_iterator window_i = std::find(
        parent()->children().begin(), parent()->children().end(), this);
    DCHECK(window_i != parent()->children().end());
    if (window_i != parent()->children().begin() &&
        (*(window_i - 1) == stacking_target_)) {
      return;
    }
  }
  RestackTransientDescendants(this, &GetStackingTarget, &ReorderImpl);
}

// static
void ServerWindow::ReorderImpl(ServerWindow* window,
                               ServerWindow* relative,
                               mojom::OrderDirection direction) {
  DCHECK(relative);
  DCHECK_NE(window, relative);
  DCHECK_EQ(window->parent(), relative->parent());

  if (!AdjustStackingForTransientWindows(&window, &relative, &direction,
                                         window->stacking_target_))
    return;

  window->parent_->children_.erase(std::find(window->parent_->children_.begin(),
                                             window->parent_->children_.end(),
                                             window));
  Windows::iterator i = std::find(window->parent_->children_.begin(),
                                  window->parent_->children_.end(), relative);
  if (direction == mojom::OrderDirection::ABOVE) {
    DCHECK(i != window->parent_->children_.end());
    window->parent_->children_.insert(++i, window);
  } else if (direction == mojom::OrderDirection::BELOW) {
    DCHECK(i != window->parent_->children_.end());
    window->parent_->children_.insert(i, window);
  }
  FOR_EACH_OBSERVER(ServerWindowObserver, window->observers_,
                    OnWindowReordered(window, relative, direction));
  window->OnStackingChanged();
}

// static
ServerWindow** ServerWindow::GetStackingTarget(ServerWindow* window) {
  return &window->stacking_target_;
}

}  // namespace ws

}  // namespace mus
