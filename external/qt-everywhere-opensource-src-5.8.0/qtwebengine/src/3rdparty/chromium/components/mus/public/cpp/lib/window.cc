// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "components/mus/common/transient_window_utils.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_surface.h"
#include "components/mus/public/cpp/window_tracker.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace mus {

namespace {

void NotifyWindowTreeChangeAtReceiver(
    Window* receiver,
    const WindowObserver::TreeChangeParams& params,
    bool change_applied) {
  WindowObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  if (change_applied) {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(receiver).observers(),
                      OnTreeChanged(local_params));
  } else {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(receiver).observers(),
                      OnTreeChanging(local_params));
  }
}

void NotifyWindowTreeChangeUp(Window* start_at,
                              const WindowObserver::TreeChangeParams& params,
                              bool change_applied) {
  for (Window* current = start_at; current; current = current->parent())
    NotifyWindowTreeChangeAtReceiver(current, params, change_applied);
}

void NotifyWindowTreeChangeDown(Window* start_at,
                                const WindowObserver::TreeChangeParams& params,
                                bool change_applied) {
  NotifyWindowTreeChangeAtReceiver(start_at, params, change_applied);
  Window::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyWindowTreeChangeDown(*it, params, change_applied);
}

void NotifyWindowTreeChange(const WindowObserver::TreeChangeParams& params,
                            bool change_applied) {
  NotifyWindowTreeChangeDown(params.target, params, change_applied);
  if (params.old_parent)
    NotifyWindowTreeChangeUp(params.old_parent, params, change_applied);
  if (params.new_parent)
    NotifyWindowTreeChangeUp(params.new_parent, params, change_applied);
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(Window* target, Window* old_parent, Window* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyWindowTreeChange(params_, false);
  }
  ~ScopedTreeNotifier() { NotifyWindowTreeChange(params_, true); }

 private:
  WindowObserver::TreeChangeParams params_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

void RemoveChildImpl(Window* child, Window::Children* children) {
  Window::Children::iterator it =
      std::find(children->begin(), children->end(), child);
  if (it != children->end()) {
    children->erase(it);
    WindowPrivate(child).ClearParent();
  }
}

class OrderChangedNotifier {
 public:
  OrderChangedNotifier(Window* window,
                       Window* relative_window,
                       mojom::OrderDirection direction)
      : window_(window),
        relative_window_(relative_window),
        direction_(direction) {}

  ~OrderChangedNotifier() {}

  void NotifyWindowReordering() {
    FOR_EACH_OBSERVER(
        WindowObserver, *WindowPrivate(window_).observers(),
        OnWindowReordering(window_, relative_window_, direction_));
  }

  void NotifyWindowReordered() {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window_).observers(),
                      OnWindowReordered(window_, relative_window_, direction_));
  }

 private:
  Window* window_;
  Window* relative_window_;
  mojom::OrderDirection direction_;

  DISALLOW_COPY_AND_ASSIGN(OrderChangedNotifier);
};

class ScopedSetBoundsNotifier {
 public:
  ScopedSetBoundsNotifier(Window* window,
                          const gfx::Rect& old_bounds,
                          const gfx::Rect& new_bounds)
      : window_(window), old_bounds_(old_bounds), new_bounds_(new_bounds) {
    FOR_EACH_OBSERVER(
        WindowObserver, *WindowPrivate(window_).observers(),
        OnWindowBoundsChanging(window_, old_bounds_, new_bounds_));
  }
  ~ScopedSetBoundsNotifier() {
    FOR_EACH_OBSERVER(WindowObserver, *WindowPrivate(window_).observers(),
                      OnWindowBoundsChanged(window_, old_bounds_, new_bounds_));
  }

 private:
  Window* window_;
  const gfx::Rect old_bounds_;
  const gfx::Rect new_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSetBoundsNotifier);
};

// Some operations are only permitted in the client that created the window.
bool OwnsWindow(WindowTreeClient* client, Window* window) {
  return !client || client->OwnsWindow(window);
}

bool IsClientRoot(Window* window) {
  return window->window_tree() &&
         window->window_tree()->GetRoots().count(window) > 0;
}

bool OwnsWindowOrIsRoot(Window* window) {
  return OwnsWindow(window->window_tree(), window) || IsClientRoot(window);
}

void EmptyEmbedCallback(bool result) {}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Window, public:

void Window::Destroy() {
  if (!OwnsWindowOrIsRoot(this))
    return;

  if (client_)
    client_->DestroyWindow(this);
  while (!children_.empty()) {
    Window* child = children_.front();
    if (!OwnsWindow(client_, child)) {
      WindowPrivate(child).ClearParent();
      children_.erase(children_.begin());
    } else {
      child->Destroy();
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    }
  }
  LocalDestroy();
}

void Window::SetBounds(const gfx::Rect& bounds) {
  if (!OwnsWindowOrIsRoot(this))
    return;
  if (bounds_ == bounds)
    return;
  if (client_)
    client_->SetBounds(this, bounds_, bounds);
  LocalSetBounds(bounds_, bounds);
}

gfx::Rect Window::GetBoundsInRoot() const {
  gfx::Vector2d offset;
  for (const Window* w = parent(); w != nullptr; w = w->parent())
    offset += w->bounds().OffsetFromOrigin();
  return bounds() + offset;
}

void Window::SetClientArea(
    const gfx::Insets& client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  if (!OwnsWindowOrIsRoot(this))
    return;

  if (client_)
    client_->SetClientArea(server_id_, client_area,
                           additional_client_areas);
  LocalSetClientArea(client_area, additional_client_areas);
}

void Window::SetHitTestMask(const gfx::Rect& mask) {
  if (!OwnsWindowOrIsRoot(this))
    return;

  if (hit_test_mask_ && *hit_test_mask_ == mask)
    return;

  if (client_)
    client_->SetHitTestMask(server_id_, mask);
  hit_test_mask_.reset(new gfx::Rect(mask));
}

void Window::ClearHitTestMask() {
  if (!OwnsWindowOrIsRoot(this))
    return;

  if (!hit_test_mask_)
    return;

  if (client_)
    client_->ClearHitTestMask(server_id_);
  hit_test_mask_.reset();
}

void Window::SetVisible(bool value) {
  if (visible_ == value)
    return;

  if (client_)
    client_->SetVisible(this, value);
  LocalSetVisible(value);
}

void Window::SetOpacity(float opacity) {
  if (client_)
    client_->SetOpacity(this, opacity);
  LocalSetOpacity(opacity);
}

void Window::SetPredefinedCursor(mus::mojom::Cursor cursor_id) {
  if (cursor_id_ == cursor_id)
    return;

  if (client_)
    client_->SetPredefinedCursor(server_id_, cursor_id);
  LocalSetPredefinedCursor(cursor_id);
}

bool Window::IsDrawn() const {
  if (!visible_)
    return false;
  return parent_ ? parent_->IsDrawn() : parent_drawn_;
}

std::unique_ptr<WindowSurface> Window::RequestSurface(mojom::SurfaceType type) {
  std::unique_ptr<WindowSurfaceBinding> surface_binding;
  std::unique_ptr<WindowSurface> surface =
      WindowSurface::Create(&surface_binding);
  AttachSurface(type, std::move(surface_binding));
  return surface;
}

void Window::AttachSurface(
    mojom::SurfaceType type,
    std::unique_ptr<WindowSurfaceBinding> surface_binding) {
  window_tree()->AttachSurface(
      server_id_, type, std::move(surface_binding->surface_request_),
      mojo::MakeProxy(std::move(surface_binding->surface_client_)));
}

void Window::ClearSharedProperty(const std::string& name) {
  SetSharedPropertyInternal(name, nullptr);
}

bool Window::HasSharedProperty(const std::string& name) const {
  return properties_.count(name) > 0;
}

void Window::AddObserver(WindowObserver* observer) {
  observers_.AddObserver(observer);
}

void Window::RemoveObserver(WindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

const Window* Window::GetRoot() const {
  const Window* root = this;
  for (const Window* parent = this; parent; parent = parent->parent())
    root = parent;
  return root;
}

void Window::AddChild(Window* child) {
  // TODO(beng): not necessarily valid to all clients, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (client_)
    CHECK_EQ(child->client_, client_);
  // Roots can not be added as children of other windows.
  if (window_tree() && window_tree()->IsRoot(child))
    return;
  LocalAddChild(child);
  if (client_)
    client_->AddChild(this, child->server_id());
}

void Window::RemoveChild(Window* child) {
  // TODO(beng): not necessarily valid to all clients, but possibly to the
  //             embeddee in an embedder-embeddee relationship.
  if (client_)
    CHECK_EQ(child->client_, client_);
  LocalRemoveChild(child);
  if (client_)
    client_->RemoveChild(this, child->server_id());
}

void Window::Reorder(Window* relative, mojom::OrderDirection direction) {
  if (!LocalReorder(relative, direction))
    return;
  if (client_)
    client_->Reorder(this, relative->server_id(), direction);
}

void Window::MoveToFront() {
  if (!parent_ || parent_->children_.back() == this)
    return;
  Reorder(parent_->children_.back(), mojom::OrderDirection::ABOVE);
}

void Window::MoveToBack() {
  if (!parent_ || parent_->children_.front() == this)
    return;
  Reorder(parent_->children_.front(), mojom::OrderDirection::BELOW);
}

bool Window::Contains(const Window* child) const {
  if (!child)
    return false;
  if (child == this)
    return true;
  if (client_)
    CHECK_EQ(child->client_, client_);
  for (const Window* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

void Window::AddTransientWindow(Window* transient_window) {
  // A system modal window cannot become a transient child.
  DCHECK(!transient_window->is_modal() || transient_window->transient_parent());

  if (client_)
    CHECK_EQ(transient_window->client_, client_);
  LocalAddTransientWindow(transient_window);
  if (client_)
    client_->AddTransientWindow(this, transient_window->server_id());
}

void Window::RemoveTransientWindow(Window* transient_window) {
  if (client_)
    CHECK_EQ(transient_window->window_tree(), client_);
  LocalRemoveTransientWindow(transient_window);
  if (client_)
    client_->RemoveTransientWindowFromParent(transient_window);
}

void Window::SetModal() {
  if (is_modal_)
    return;

  LocalSetModal();
  if (client_)
    client_->SetModal(this);
}

Window* Window::GetChildByLocalId(int id) {
  if (id == local_id_)
    return this;
  // TODO(beng): this could be improved depending on how we decide to own
  // windows.
  for (Window* child : children_) {
    Window* matching_child = child->GetChildByLocalId(id);
    if (matching_child)
      return matching_child;
  }
  return nullptr;
}

void Window::SetTextInputState(mojo::TextInputStatePtr state) {
  if (client_)
    client_->SetWindowTextInputState(server_id_, std::move(state));
}

void Window::SetImeVisibility(bool visible, mojo::TextInputStatePtr state) {
  // SetImeVisibility() shouldn't be used if the window is not editable.
  DCHECK(state.is_null() || state->type != mojo::TextInputType::NONE);
  if (client_)
    client_->SetImeVisibility(server_id_, visible, std::move(state));
}

bool Window::HasCapture() const {
  return client_ && client_->GetCaptureWindow() == this;
}

void Window::SetCapture() {
  if (client_)
    client_->SetCapture(this);
}

void Window::ReleaseCapture() {
  if (client_)
    client_->ReleaseCapture(this);
}

void Window::SetFocus() {
  if (client_ && IsDrawn())
    client_->SetFocus(this);
}

bool Window::HasFocus() const {
  return client_ && client_->GetFocusedWindow() == this;
}

void Window::SetCanFocus(bool can_focus) {
  if (client_)
    client_->SetCanFocus(server_id_, can_focus);
}

void Window::Embed(mus::mojom::WindowTreeClientPtr client, uint32_t flags) {
  Embed(std::move(client), base::Bind(&EmptyEmbedCallback), flags);
}

void Window::Embed(mus::mojom::WindowTreeClientPtr client,
                   const EmbedCallback& callback,
                   uint32_t flags) {
  if (PrepareForEmbed())
    client_->Embed(server_id_, std::move(client), flags, callback);
  else
    callback.Run(false);
}

void Window::RequestClose() {
  if (client_)
    client_->RequestClose(this);
}

std::string Window::GetName() const {
  if (HasSharedProperty(mojom::WindowManager::kName_Property))
    return GetSharedProperty<std::string>(mojom::WindowManager::kName_Property);

  return std::string();
}

////////////////////////////////////////////////////////////////////////////////
// Window, protected:

Window::Window() : Window(nullptr, static_cast<Id>(-1)) {}

Window::~Window() {
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDestroying(this));
  if (client_)
    client_->OnWindowDestroying(this);

  if (HasFocus()) {
    // The focused window is being removed. When this happens the server
    // advances focus. We don't want to randomly pick a Window to get focus, so
    // we update local state only, and wait for the next focus change from the
    // server.
    client_->LocalSetFocus(nullptr);
  }

  // Remove from transient parent.
  if (transient_parent_)
    transient_parent_->LocalRemoveTransientWindow(this);

  // Remove transient children.
  while (!transient_children_.empty()) {
    Window* transient_child = transient_children_.front();
    LocalRemoveTransientWindow(transient_child);
    transient_child->LocalDestroy();
    DCHECK(transient_children_.empty() ||
           transient_children_.front() != transient_child);
  }

  if (parent_)
    parent_->LocalRemoveChild(this);

  // We may still have children. This can happen if the embedder destroys the
  // root while we're still alive.
  while (!children_.empty()) {
    Window* child = children_.front();
    LocalRemoveChild(child);
    DCHECK(children_.empty() || children_.front() != child);
  }

  // Clear properties.
  for (auto& pair : prop_map_) {
    if (pair.second.deallocator)
      (*pair.second.deallocator)(pair.second.value);
  }
  prop_map_.clear();

  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDestroyed(this));

  // Invoke after observers so that can clean up any internal state observers
  // may have changed.
  if (window_tree())
    window_tree()->OnWindowDestroyed(this);
}

////////////////////////////////////////////////////////////////////////////////
// Window, private:

Window::Window(WindowTreeClient* client, Id id)
    : client_(client),
      server_id_(id),
      parent_(nullptr),
      stacking_target_(nullptr),
      transient_parent_(nullptr),
      is_modal_(false),
      // Matches aura, see aura::Window for details.
      observers_(base::ObserverList<WindowObserver>::NOTIFY_EXISTING_ONLY),
      input_event_handler_(nullptr),
      visible_(false),
      opacity_(1.0f),
      display_id_(display::Display::kInvalidDisplayID),
      cursor_id_(mojom::Cursor::CURSOR_NULL),
      parent_drawn_(false) {}

void Window::SetSharedPropertyInternal(const std::string& name,
                                       const std::vector<uint8_t>* value) {
  if (!OwnsWindowOrIsRoot(this))
    return;

  if (client_) {
    mojo::Array<uint8_t> transport_value(nullptr);
    if (value) {
      transport_value.resize(value->size());
      if (value->size())
        memcpy(&transport_value.front(), &(value->front()), value->size());
    }
    // TODO: add test coverage of this (450303).
    client_->SetProperty(this, name, std::move(transport_value));
  }
  LocalSetSharedProperty(name, value);
}

int64_t Window::SetLocalPropertyInternal(const void* key,
                                         const char* name,
                                         PropertyDeallocator deallocator,
                                         int64_t value,
                                         int64_t default_value) {
  int64_t old = GetLocalPropertyInternal(key, default_value);
  if (value == default_value) {
    prop_map_.erase(key);
  } else {
    Value prop_value;
    prop_value.name = name;
    prop_value.value = value;
    prop_value.deallocator = deallocator;
    prop_map_[key] = prop_value;
  }
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowLocalPropertyChanged(this, key, old));
  return old;
}

int64_t Window::GetLocalPropertyInternal(const void* key,
                                         int64_t default_value) const {
  std::map<const void*, Value>::const_iterator iter = prop_map_.find(key);
  if (iter == prop_map_.end())
    return default_value;
  return iter->second.value;
}

void Window::LocalDestroy() {
  delete this;
}

void Window::LocalAddChild(Window* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  if (child->parent())
    RemoveChildImpl(child, &child->parent_->children_);
  children_.push_back(child);
  child->parent_ = this;
  child->display_id_ = display_id_;
}

void Window::LocalRemoveChild(Window* child) {
  DCHECK_EQ(this, child->parent());
  ScopedTreeNotifier notifier(child, this, nullptr);
  RemoveChildImpl(child, &children_);
}

void Window::LocalAddTransientWindow(Window* transient_window) {
  if (transient_window->transient_parent())
    RemoveTransientWindowImpl(transient_window);
  transient_children_.push_back(transient_window);
  transient_window->transient_parent_ = this;

  // Restack |transient_window| properly above its transient parent, if they
  // share the same parent.
  if (transient_window->parent() == parent())
    RestackTransientDescendants(this, &GetStackingTarget,
                                &ReorderWithoutNotification);

  // TODO(fsamuel): We might want a notification here.
}

void Window::LocalRemoveTransientWindow(Window* transient_window) {
  DCHECK_EQ(this, transient_window->transient_parent());
  RemoveTransientWindowImpl(transient_window);
  // TODO(fsamuel): We might want a notification here.
}

void Window::LocalSetModal() {
  is_modal_ = true;
}

bool Window::LocalReorder(Window* relative, mojom::OrderDirection direction) {
  OrderChangedNotifier notifier(this, relative, direction);
  return ReorderImpl(this, relative, direction, &notifier);
}

void Window::LocalSetBounds(const gfx::Rect& old_bounds,
                            const gfx::Rect& new_bounds) {
  // If this client owns the window, then it should be the only one to change
  // the bounds.
  DCHECK(!OwnsWindow(client_, this) || old_bounds == bounds_);
  ScopedSetBoundsNotifier notifier(this, old_bounds, new_bounds);
  bounds_ = new_bounds;
}

void Window::LocalSetClientArea(
    const gfx::Insets& new_client_area,
    const std::vector<gfx::Rect>& additional_client_areas) {
  const std::vector<gfx::Rect> old_additional_client_areas =
      additional_client_areas_;
  const gfx::Insets old_client_area = client_area_;
  client_area_ = new_client_area;
  additional_client_areas_ = additional_client_areas;
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowClientAreaChanged(this, old_client_area,
                                              old_additional_client_areas));
}

void Window::LocalSetDisplay(int64_t display_id) {
  display_id_ = display_id;
  // TODO(sad): Notify observers (of this window, and of the descendant windows)
  // when a window moves from one display into another. https://crbug.com/614887
}

void Window::LocalSetParentDrawn(bool value) {
  if (parent_drawn_ == value)
    return;

  // As IsDrawn() is derived from |visible_| and |parent_drawn_|, only send
  // drawn notification is the value of IsDrawn() is really changing.
  if (IsDrawn() == value) {
    parent_drawn_ = value;
    return;
  }
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDrawnChanging(this));
  parent_drawn_ = value;
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDrawnChanged(this));
}

void Window::LocalSetVisible(bool visible) {
  if (visible_ == visible)
    return;

  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowVisibilityChanging(this));
  visible_ = visible;
  NotifyWindowVisibilityChanged(this);
}

void Window::LocalSetOpacity(float opacity) {
  if (opacity_ == opacity)
    return;

  float old_opacity = opacity_;
  opacity_ = opacity;
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowOpacityChanged(this, old_opacity, opacity_));
}

void Window::LocalSetPredefinedCursor(mojom::Cursor cursor_id) {
  if (cursor_id_ == cursor_id)
    return;

  cursor_id_ = cursor_id;
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowPredefinedCursorChanged(this, cursor_id));
}

void Window::LocalSetSharedProperty(const std::string& name,
                                    const std::vector<uint8_t>* value) {
  std::vector<uint8_t> old_value;
  std::vector<uint8_t>* old_value_ptr = nullptr;
  auto it = properties_.find(name);
  if (it != properties_.end()) {
    old_value = it->second;
    old_value_ptr = &old_value;

    if (value && old_value == *value)
      return;
  } else if (!value) {
    // This property isn't set in |properties_| and |value| is nullptr, so
    // there's no change.
    return;
  }

  if (value) {
    properties_[name] = *value;
  } else if (it != properties_.end()) {
    properties_.erase(it);
  }

  FOR_EACH_OBSERVER(
      WindowObserver, observers_,
      OnWindowSharedPropertyChanged(this, name, old_value_ptr, value));
}

void Window::NotifyWindowStackingChanged() {
  if (stacking_target_) {
    Children::const_iterator window_i = std::find(
        parent()->children().begin(), parent()->children().end(), this);
    DCHECK(window_i != parent()->children().end());
    if (window_i != parent()->children().begin() &&
        (*(window_i - 1) == stacking_target_))
      return;
  }
  RestackTransientDescendants(this, &GetStackingTarget,
                              &ReorderWithoutNotification);
}

void Window::NotifyWindowVisibilityChanged(Window* target) {
  if (!NotifyWindowVisibilityChangedDown(target)) {
    return;  // |this| has been deleted.
  }
  NotifyWindowVisibilityChangedUp(target);
}

bool Window::NotifyWindowVisibilityChangedAtReceiver(Window* target) {
  // |this| may be deleted during a call to OnWindowVisibilityChanged() on one
  // of the observers. We create an local observer for that. In that case we
  // exit without further access to any members.
  WindowTracker tracker;
  tracker.Add(this);
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowVisibilityChanged(target));
  return tracker.Contains(this);
}

bool Window::NotifyWindowVisibilityChangedDown(Window* target) {
  if (!NotifyWindowVisibilityChangedAtReceiver(target))
    return false;  // |this| was deleted.
  std::set<const Window*> child_already_processed;
  bool child_destroyed = false;
  do {
    child_destroyed = false;
    for (Window::Children::const_iterator it = children_.begin();
         it != children_.end(); ++it) {
      if (!child_already_processed.insert(*it).second)
        continue;
      if (!(*it)->NotifyWindowVisibilityChangedDown(target)) {
        // |*it| was deleted, |it| is invalid and |children_| has changed.  We
        // exit the current for-loop and enter a new one.
        child_destroyed = true;
        break;
      }
    }
  } while (child_destroyed);
  return true;
}

void Window::NotifyWindowVisibilityChangedUp(Window* target) {
  // Start with the parent as we already notified |this|
  // in NotifyWindowVisibilityChangedDown.
  for (Window* window = parent(); window; window = window->parent()) {
    bool ret = window->NotifyWindowVisibilityChangedAtReceiver(target);
    DCHECK(ret);
  }
}

bool Window::PrepareForEmbed() {
  if (!OwnsWindow(client_, this))
    return false;

  while (!children_.empty())
    RemoveChild(children_[0]);
  return true;
}

void Window::RemoveTransientWindowImpl(Window* transient_window) {
  Window::Children::iterator it = std::find(
      transient_children_.begin(), transient_children_.end(), transient_window);
  if (it != transient_children_.end()) {
    transient_children_.erase(it);
    transient_window->transient_parent_ = nullptr;
  }
  // If |transient_window| and its former transient parent share the same
  // parent, |transient_window| should be restacked properly so it is not among
  // transient children of its former parent, anymore.
  if (parent() == transient_window->parent())
    RestackTransientDescendants(this, &GetStackingTarget,
                                &ReorderWithoutNotification);

  // TOOD(fsamuel): We might want to notify observers here.
}

// static
void Window::ReorderWithoutNotification(Window* window,
                                        Window* relative,
                                        mojom::OrderDirection direction) {
  ReorderImpl(window, relative, direction, nullptr);
}

// static
// Returns true if the order actually changed.
bool Window::ReorderImpl(Window* window,
                         Window* relative,
                         mojom::OrderDirection direction,
                         OrderChangedNotifier* notifier) {
  DCHECK(relative);
  DCHECK_NE(window, relative);
  DCHECK_EQ(window->parent(), relative->parent());
  DCHECK(window->parent());

  if (!AdjustStackingForTransientWindows(&window, &relative, &direction,
                                         window->stacking_target_))
    return false;

  const size_t child_i = std::find(window->parent_->children_.begin(),
                                   window->parent_->children_.end(), window) -
                         window->parent_->children_.begin();
  const size_t target_i =
      std::find(window->parent_->children_.begin(),
                window->parent_->children_.end(), relative) -
      window->parent_->children_.begin();
  if ((direction == mojom::OrderDirection::ABOVE && child_i == target_i + 1) ||
      (direction == mojom::OrderDirection::BELOW && child_i + 1 == target_i)) {
    return false;
  }

  if (notifier)
    notifier->NotifyWindowReordering();

  const size_t dest_i = direction == mojom::OrderDirection::ABOVE
                            ? (child_i < target_i ? target_i : target_i + 1)
                            : (child_i < target_i ? target_i - 1 : target_i);
  window->parent_->children_.erase(window->parent_->children_.begin() +
                                   child_i);
  window->parent_->children_.insert(window->parent_->children_.begin() + dest_i,
                                    window);

  window->NotifyWindowStackingChanged();

  if (notifier)
    notifier->NotifyWindowReordered();

  return true;
}

// static
Window** Window::GetStackingTarget(Window* window) {
  return &window->stacking_target_;
}
}  // namespace mus
