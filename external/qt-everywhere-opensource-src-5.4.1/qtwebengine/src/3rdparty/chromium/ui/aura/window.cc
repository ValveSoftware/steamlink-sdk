// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/event_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/visibility_client.h"
#include "ui/aura/client/window_stacking_client.h"
#include "ui/aura/env.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_target_iterator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/screen.h"

namespace aura {

namespace {

ui::LayerType WindowLayerTypeToUILayerType(WindowLayerType window_layer_type) {
  switch (window_layer_type) {
    case WINDOW_LAYER_NONE:
      break;
    case WINDOW_LAYER_NOT_DRAWN:
      return ui::LAYER_NOT_DRAWN;
    case WINDOW_LAYER_TEXTURED:
      return ui::LAYER_TEXTURED;
    case WINDOW_LAYER_SOLID_COLOR:
      return ui::LAYER_SOLID_COLOR;
  }
  NOTREACHED();
  return ui::LAYER_NOT_DRAWN;
}

// Used when searching for a Window to stack relative to.
template <class T>
T IteratorForDirectionBegin(aura::Window* window);

template <>
Window::Windows::const_iterator IteratorForDirectionBegin(
    aura::Window* window) {
  return window->children().begin();
}

template <>
Window::Windows::const_reverse_iterator IteratorForDirectionBegin(
    aura::Window* window) {
  return window->children().rbegin();
}

template <class T>
T IteratorForDirectionEnd(aura::Window* window);

template <>
Window::Windows::const_iterator IteratorForDirectionEnd(aura::Window* window) {
  return window->children().end();
}

template <>
Window::Windows::const_reverse_iterator IteratorForDirectionEnd(
    aura::Window* window) {
  return window->children().rend();
}

// Depth first search for the first Window with a layer to stack relative
// to. Starts at target. Does not descend into |ignore|.
template <class T>
ui::Layer* FindStackingTargetLayerDown(aura::Window* target,
                                       aura::Window* ignore) {
  if (target == ignore)
    return NULL;

  if (target->layer())
    return target->layer();

  for (T i = IteratorForDirectionBegin<T>(target);
       i != IteratorForDirectionEnd<T>(target); ++i) {
    ui::Layer* layer = FindStackingTargetLayerDown<T>(*i, ignore);
    if (layer)
      return layer;
  }
  return NULL;
}

// Depth first search through the siblings of |target||. This does not search
// all the siblings, only those before/after |target| (depening upon the
// template type) and ignoring |ignore|. Returns the Layer of the first Window
// encountered with a Layer.
template <class T>
ui::Layer* FindStackingLayerInSiblings(aura::Window* target,
                                       aura::Window* ignore) {
  aura::Window* parent = target->parent();
  for (T i = std::find(IteratorForDirectionBegin<T>(parent),
                  IteratorForDirectionEnd<T>(parent), target);
       i != IteratorForDirectionEnd<T>(parent); ++i) {
    ui::Layer* layer = FindStackingTargetLayerDown<T>(*i, ignore);
    if (layer)
      return layer;
  }
  return NULL;
}

// Returns the first Window that has a Layer. This does a depth first search
// through the descendants of |target| first, then ascends up doing a depth
// first search through siblings of all ancestors until a Layer is found or an
// ancestor with a layer is found. This is intended to locate a layer to stack
// other layers relative to.
template <class T>
ui::Layer* FindStackingTargetLayer(aura::Window* target, aura::Window* ignore) {
  ui::Layer* result = FindStackingTargetLayerDown<T>(target, ignore);
  if (result)
    return result;
  while (target->parent()) {
    ui::Layer* result = FindStackingLayerInSiblings<T>(target, ignore);
    if (result)
      return result;
    target = target->parent();
    if (target->layer())
      return NULL;
  }
  return NULL;
}

// Does a depth first search for all descendants of |child| that have layers.
// This stops at any descendants that have layers (and adds them to |layers|).
void GetLayersToStack(aura::Window* child, std::vector<ui::Layer*>* layers) {
  if (child->layer()) {
    layers->push_back(child->layer());
    return;
  }
  for (size_t i = 0; i < child->children().size(); ++i)
    GetLayersToStack(child->children()[i], layers);
}

}  // namespace

class ScopedCursorHider {
 public:
  explicit ScopedCursorHider(Window* window)
      : window_(window),
        hid_cursor_(false) {
    if (!window_->IsRootWindow())
      return;
    const bool cursor_is_in_bounds = window_->GetBoundsInScreen().Contains(
        Env::GetInstance()->last_mouse_location());
    client::CursorClient* cursor_client = client::GetCursorClient(window_);
    if (cursor_is_in_bounds && cursor_client &&
        cursor_client->IsCursorVisible()) {
      cursor_client->HideCursor();
      hid_cursor_ = true;
    }
  }
  ~ScopedCursorHider() {
    if (!window_->IsRootWindow())
      return;

    // Update the device scale factor of the cursor client only when the last
    // mouse location is on this root window.
    if (hid_cursor_) {
      client::CursorClient* cursor_client = client::GetCursorClient(window_);
      if (cursor_client) {
        const gfx::Display& display =
            gfx::Screen::GetScreenFor(window_)->GetDisplayNearestWindow(
                window_);
        cursor_client->SetDisplay(display);
        cursor_client->ShowCursor();
      }
    }
  }

 private:
  Window* window_;
  bool hid_cursor_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCursorHider);
};

Window::Window(WindowDelegate* delegate)
    : host_(NULL),
      type_(ui::wm::WINDOW_TYPE_UNKNOWN),
      owned_by_parent_(true),
      delegate_(delegate),
      parent_(NULL),
      visible_(false),
      id_(-1),
      transparent_(false),
      user_data_(NULL),
      ignore_events_(false),
      // Don't notify newly added observers during notification. This causes
      // problems for code that adds an observer as part of an observer
      // notification (such as the workspace code).
      observers_(ObserverList<WindowObserver>::NOTIFY_EXISTING_ONLY) {
  set_target_handler(delegate_);
}

Window::~Window() {
  // |layer()| can be NULL during tests, or if this Window is layerless.
  if (layer()) {
    if (layer()->owner() == this)
      layer()->CompleteAllAnimations();
    layer()->SuppressPaint();
  }

  // Let the delegate know we're in the processing of destroying.
  if (delegate_)
    delegate_->OnWindowDestroying(this);
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowDestroying(this));

  // TODO(beng): See comment in window_event_dispatcher.h. This shouldn't be
  //             necessary but unfortunately is right now due to ordering
  //             peculiarities. WED must be notified _after_ other observers
  //             are notified of pending teardown but before the hierarchy
  //             is actually torn down.
  WindowTreeHost* host = GetHost();
  if (host)
    host->dispatcher()->OnPostNotifiedWindowDestroying(this);

  // The window should have already had its state cleaned up in
  // WindowEventDispatcher::OnWindowHidden(), but there have been some crashes
  // involving windows being destroyed without being hidden first. See
  // crbug.com/342040. This should help us debug the issue. TODO(tdresser):
  // remove this once we determine why we have windows that are destroyed
  // without being hidden.
  bool window_incorrectly_cleaned_up = CleanupGestureState();
  CHECK(!window_incorrectly_cleaned_up);

  // Then destroy the children.
  RemoveOrDestroyChildren();

  // The window needs to be removed from the parent before calling the
  // WindowDestroyed callbacks of delegate and the observers.
  if (parent_)
    parent_->RemoveChild(this);

  if (delegate_)
    delegate_->OnWindowDestroyed(this);
  ObserverListBase<WindowObserver>::Iterator iter(observers_);
  WindowObserver* observer;
  while ((observer = iter.GetNext())) {
    RemoveObserver(observer);
    observer->OnWindowDestroyed(this);
  }

  // Clear properties.
  for (std::map<const void*, Value>::const_iterator iter = prop_map_.begin();
       iter != prop_map_.end();
       ++iter) {
    if (iter->second.deallocator)
      (*iter->second.deallocator)(iter->second.value);
  }
  prop_map_.clear();

  // If we have layer it will either be destroyed by |layer_owner_|'s dtor, or
  // by whoever acquired it. We don't have a layer if Init() wasn't invoked or
  // we are layerless.
  if (layer())
    layer()->set_delegate(NULL);
  DestroyLayer();
}

void Window::Init(WindowLayerType window_layer_type) {
  if (window_layer_type != WINDOW_LAYER_NONE) {
    SetLayer(new ui::Layer(WindowLayerTypeToUILayerType(window_layer_type)));
    layer()->SetVisible(false);
    layer()->set_delegate(this);
    UpdateLayerName();
    layer()->SetFillsBoundsOpaquely(!transparent_);
  }

  Env::GetInstance()->NotifyWindowInitialized(this);
}

void Window::SetType(ui::wm::WindowType type) {
  // Cannot change type after the window is initialized.
  DCHECK(!layer());
  type_ = type;
}

void Window::SetName(const std::string& name) {
  name_ = name;

  if (layer())
    UpdateLayerName();
}

void Window::SetTransparent(bool transparent) {
  transparent_ = transparent;
  if (layer())
    layer()->SetFillsBoundsOpaquely(!transparent_);
}

void Window::SetFillsBoundsCompletely(bool fills_bounds) {
  if (layer())
    layer()->SetFillsBoundsCompletely(fills_bounds);
}

Window* Window::GetRootWindow() {
  return const_cast<Window*>(
      static_cast<const Window*>(this)->GetRootWindow());
}

const Window* Window::GetRootWindow() const {
  return IsRootWindow() ? this : parent_ ? parent_->GetRootWindow() : NULL;
}

WindowTreeHost* Window::GetHost() {
  return const_cast<WindowTreeHost*>(const_cast<const Window*>(this)->
      GetHost());
}

const WindowTreeHost* Window::GetHost() const {
  const Window* root_window = GetRootWindow();
  return root_window ? root_window->host_ : NULL;
}

void Window::Show() {
  if (layer()) {
    DCHECK_EQ(visible_, layer()->GetTargetVisibility());
    // It is not allowed that a window is visible but the layers alpha is fully
    // transparent since the window would still be considered to be active but
    // could not be seen.
    DCHECK(!(visible_ && layer()->GetTargetOpacity() == 0.0f));
  }
  SetVisible(true);
}

void Window::Hide() {
  // RootWindow::OnVisibilityChanged will call ReleaseCapture.
  SetVisible(false);
}

bool Window::IsVisible() const {
  // Layer visibility can be inconsistent with window visibility, for example
  // when a Window is hidden, we want this function to return false immediately
  // after, even though the client may decide to animate the hide effect (and
  // so the layer will be visible for some time after Hide() is called).
  for (const Window* window = this; window; window = window->parent()) {
    if (!window->visible_)
      return false;
    if (window->layer())
      return window->layer()->IsDrawn();
  }
  return false;
}

gfx::Rect Window::GetBoundsInRootWindow() const {
  // TODO(beng): There may be a better way to handle this, and the existing code
  //             is likely wrong anyway in a multi-display world, but this will
  //             do for now.
  if (!GetRootWindow())
    return bounds();
  gfx::Point origin = bounds().origin();
  ConvertPointToTarget(parent_, GetRootWindow(), &origin);
  return gfx::Rect(origin, bounds().size());
}

gfx::Rect Window::GetBoundsInScreen() const {
  gfx::Rect bounds(GetBoundsInRootWindow());
  const Window* root = GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      gfx::Point origin = bounds.origin();
      screen_position_client->ConvertPointToScreen(root, &origin);
      bounds.set_origin(origin);
    }
  }
  return bounds;
}

void Window::SetTransform(const gfx::Transform& transform) {
  if (!layer()) {
    // Transforms aren't supported on layerless windows.
    NOTREACHED();
    return;
  }
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowTransforming(this));
  layer()->SetTransform(transform);
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowTransformed(this));
}

void Window::SetLayoutManager(LayoutManager* layout_manager) {
  if (layout_manager == layout_manager_)
    return;
  layout_manager_.reset(layout_manager);
  if (!layout_manager)
    return;
  // If we're changing to a new layout manager, ensure it is aware of all the
  // existing child windows.
  for (Windows::const_iterator it = children_.begin();
       it != children_.end();
       ++it)
    layout_manager_->OnWindowAddedToLayout(*it);
}

scoped_ptr<ui::EventTargeter>
Window::SetEventTargeter(scoped_ptr<ui::EventTargeter> targeter) {
  scoped_ptr<ui::EventTargeter> old_targeter = targeter_.Pass();
  targeter_ = targeter.Pass();
  return old_targeter.Pass();
}

void Window::SetBounds(const gfx::Rect& new_bounds) {
  if (parent_ && parent_->layout_manager())
    parent_->layout_manager()->SetChildBounds(this, new_bounds);
  else {
    // Ensure we don't go smaller than our minimum bounds.
    gfx::Rect final_bounds(new_bounds);
    if (delegate_) {
      const gfx::Size& min_size = delegate_->GetMinimumSize();
      final_bounds.set_width(std::max(min_size.width(), final_bounds.width()));
      final_bounds.set_height(std::max(min_size.height(),
                                       final_bounds.height()));
    }
    SetBoundsInternal(final_bounds);
  }
}

void Window::SetBoundsInScreen(const gfx::Rect& new_bounds_in_screen,
                               const gfx::Display& dst_display) {
  Window* root = GetRootWindow();
  if (root) {
    gfx::Point origin = new_bounds_in_screen.origin();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    screen_position_client->SetBounds(this, new_bounds_in_screen, dst_display);
    return;
  }
  SetBounds(new_bounds_in_screen);
}

gfx::Rect Window::GetTargetBounds() const {
  if (!layer())
    return bounds();

  if (!parent_ || parent_->layer())
    return layer()->GetTargetBounds();

  // We have a layer but our parent (who is valid) doesn't. This means the
  // coordinates of the layer are relative to the first ancestor with a layer;
  // convert to be relative to parent.
  gfx::Vector2d offset;
  const aura::Window* ancestor_with_layer =
      parent_->GetAncestorWithLayer(&offset);
  if (!ancestor_with_layer)
    return layer()->GetTargetBounds();

  gfx::Rect layer_target_bounds = layer()->GetTargetBounds();
  layer_target_bounds -= offset;
  return layer_target_bounds;
}

void Window::SchedulePaintInRect(const gfx::Rect& rect) {
  if (!layer() && parent_) {
    // Notification of paint scheduled happens for the window with a layer.
    gfx::Rect parent_rect(bounds().size());
    parent_rect.Intersect(rect);
    if (!parent_rect.IsEmpty()) {
      parent_rect.Offset(bounds().origin().OffsetFromOrigin());
      parent_->SchedulePaintInRect(parent_rect);
    }
  } else if (layer() && layer()->SchedulePaint(rect)) {
    FOR_EACH_OBSERVER(
        WindowObserver, observers_, OnWindowPaintScheduled(this, rect));
  }
}

void Window::StackChildAtTop(Window* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // In the front already.
  StackChildAbove(child, children_.back());
}

void Window::StackChildAbove(Window* child, Window* target) {
  StackChildRelativeTo(child, target, STACK_ABOVE);
}

void Window::StackChildAtBottom(Window* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // At the bottom already.
  StackChildBelow(child, children_.front());
}

void Window::StackChildBelow(Window* child, Window* target) {
  StackChildRelativeTo(child, target, STACK_BELOW);
}

void Window::AddChild(Window* child) {
  WindowObserver::HierarchyChangeParams params;
  params.target = child;
  params.new_parent = this;
  params.old_parent = child->parent();
  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
  NotifyWindowHierarchyChange(params);

  Window* old_root = child->GetRootWindow();

  DCHECK(std::find(children_.begin(), children_.end(), child) ==
      children_.end());
  if (child->parent())
    child->parent()->RemoveChildImpl(child, this);

  gfx::Vector2d offset;
  aura::Window* ancestor_with_layer = GetAncestorWithLayer(&offset);

  child->parent_ = this;

  if (ancestor_with_layer) {
    offset += child->bounds().OffsetFromOrigin();
    child->ReparentLayers(ancestor_with_layer->layer(), offset);
  }

  children_.push_back(child);
  if (layout_manager_)
    layout_manager_->OnWindowAddedToLayout(child);
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowAdded(child));
  child->OnParentChanged();

  Window* root_window = GetRootWindow();
  if (root_window && old_root != root_window) {
    root_window->GetHost()->dispatcher()->OnWindowAddedToRootWindow(child);
    child->NotifyAddedToRootWindow();
  }

  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
  NotifyWindowHierarchyChange(params);
}

void Window::RemoveChild(Window* child) {
  WindowObserver::HierarchyChangeParams params;
  params.target = child;
  params.new_parent = NULL;
  params.old_parent = this;
  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING;
  NotifyWindowHierarchyChange(params);

  RemoveChildImpl(child, NULL);

  params.phase = WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED;
  NotifyWindowHierarchyChange(params);
}

bool Window::Contains(const Window* other) const {
  for (const Window* parent = other; parent; parent = parent->parent_) {
    if (parent == this)
      return true;
  }
  return false;
}

Window* Window::GetChildById(int id) {
  return const_cast<Window*>(const_cast<const Window*>(this)->GetChildById(id));
}

const Window* Window::GetChildById(int id) const {
  Windows::const_iterator i;
  for (i = children_.begin(); i != children_.end(); ++i) {
    if ((*i)->id() == id)
      return *i;
    const Window* result = (*i)->GetChildById(id);
    if (result)
      return result;
  }
  return NULL;
}

// static
void Window::ConvertPointToTarget(const Window* source,
                                  const Window* target,
                                  gfx::Point* point) {
  if (!source)
    return;
  if (source->GetRootWindow() != target->GetRootWindow()) {
    client::ScreenPositionClient* source_client =
        client::GetScreenPositionClient(source->GetRootWindow());
    // |source_client| can be NULL in tests.
    if (source_client)
      source_client->ConvertPointToScreen(source, point);

    client::ScreenPositionClient* target_client =
        client::GetScreenPositionClient(target->GetRootWindow());
    // |target_client| can be NULL in tests.
    if (target_client)
      target_client->ConvertPointFromScreen(target, point);
  } else if ((source != target) && (!source->layer() || !target->layer())) {
    if (!source->layer()) {
      gfx::Vector2d offset_to_layer;
      source = source->GetAncestorWithLayer(&offset_to_layer);
      *point += offset_to_layer;
    }
    if (!target->layer()) {
      gfx::Vector2d offset_to_layer;
      target = target->GetAncestorWithLayer(&offset_to_layer);
      *point -= offset_to_layer;
    }
    ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
  } else {
    ui::Layer::ConvertPointToLayer(source->layer(), target->layer(), point);
  }
}

// static
void Window::ConvertRectToTarget(const Window* source,
                                 const Window* target,
                                 gfx::Rect* rect) {
  DCHECK(rect);
  gfx::Point origin = rect->origin();
  ConvertPointToTarget(source, target, &origin);
  rect->set_origin(origin);
}

void Window::MoveCursorTo(const gfx::Point& point_in_window) {
  Window* root_window = GetRootWindow();
  DCHECK(root_window);
  gfx::Point point_in_root(point_in_window);
  ConvertPointToTarget(this, root_window, &point_in_root);
  root_window->GetHost()->MoveCursorTo(point_in_root);
}

gfx::NativeCursor Window::GetCursor(const gfx::Point& point) const {
  return delegate_ ? delegate_->GetCursor(point) : gfx::kNullCursor;
}

void Window::AddObserver(WindowObserver* observer) {
  observer->OnObservingWindow(this);
  observers_.AddObserver(observer);
}

void Window::RemoveObserver(WindowObserver* observer) {
  observer->OnUnobservingWindow(this);
  observers_.RemoveObserver(observer);
}

bool Window::HasObserver(WindowObserver* observer) {
  return observers_.HasObserver(observer);
}

bool Window::ContainsPointInRoot(const gfx::Point& point_in_root) const {
  const Window* root_window = GetRootWindow();
  if (!root_window)
    return false;
  gfx::Point local_point(point_in_root);
  ConvertPointToTarget(root_window, this, &local_point);
  return gfx::Rect(GetTargetBounds().size()).Contains(local_point);
}

bool Window::ContainsPoint(const gfx::Point& local_point) const {
  return gfx::Rect(bounds().size()).Contains(local_point);
}

Window* Window::GetEventHandlerForPoint(const gfx::Point& local_point) {
  return GetWindowForPoint(local_point, true, true);
}

Window* Window::GetTopWindowContainingPoint(const gfx::Point& local_point) {
  return GetWindowForPoint(local_point, false, false);
}

Window* Window::GetToplevelWindow() {
  Window* topmost_window_with_delegate = NULL;
  for (aura::Window* window = this; window != NULL; window = window->parent()) {
    if (window->delegate())
      topmost_window_with_delegate = window;
  }
  return topmost_window_with_delegate;
}

void Window::Focus() {
  client::FocusClient* client = client::GetFocusClient(this);
  DCHECK(client);
  client->FocusWindow(this);
}

void Window::Blur() {
  client::FocusClient* client = client::GetFocusClient(this);
  DCHECK(client);
  client->FocusWindow(NULL);
}

bool Window::HasFocus() const {
  client::FocusClient* client = client::GetFocusClient(this);
  return client && client->GetFocusedWindow() == this;
}

bool Window::CanFocus() const {
  if (IsRootWindow())
    return IsVisible();

  // NOTE: as part of focusing the window the ActivationClient may make the
  // window visible (by way of making a hidden ancestor visible). For this
  // reason we can't check visibility here and assume the client is doing it.
  if (!parent_ || (delegate_ && !delegate_->CanFocus()))
    return false;

  // The client may forbid certain windows from receiving focus at a given point
  // in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  return parent_->CanFocus();
}

bool Window::CanReceiveEvents() const {
  if (IsRootWindow())
    return IsVisible();

  // The client may forbid certain windows from receiving events at a given
  // point in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  return parent_ && IsVisible() && parent_->CanReceiveEvents();
}

void Window::SetCapture() {
  if (!IsVisible())
    return;

  Window* root_window = GetRootWindow();
  if (!root_window)
    return;
  client::GetCaptureClient(root_window)->SetCapture(this);
}

void Window::ReleaseCapture() {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return;
  client::GetCaptureClient(root_window)->ReleaseCapture(this);
}

bool Window::HasCapture() {
  Window* root_window = GetRootWindow();
  if (!root_window)
    return false;
  client::CaptureClient* capture_client = client::GetCaptureClient(root_window);
  return capture_client && capture_client->GetCaptureWindow() == this;
}

void Window::SuppressPaint() {
  if (layer())
    layer()->SuppressPaint();
}

// {Set,Get,Clear}Property are implemented in window_property.h.

void Window::SetNativeWindowProperty(const char* key, void* value) {
  SetPropertyInternal(
      key, key, NULL, reinterpret_cast<int64>(value), 0);
}

void* Window::GetNativeWindowProperty(const char* key) const {
  return reinterpret_cast<void*>(GetPropertyInternal(key, 0));
}

void Window::OnDeviceScaleFactorChanged(float device_scale_factor) {
  ScopedCursorHider hider(this);
  if (IsRootWindow())
    host_->OnDeviceScaleFactorChanged(device_scale_factor);
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged(device_scale_factor);
}

#if !defined(NDEBUG)
std::string Window::GetDebugInfo() const {
  return base::StringPrintf(
      "%s<%d> bounds(%d, %d, %d, %d) %s %s opacity=%.1f",
      name().empty() ? "Unknown" : name().c_str(), id(),
      bounds().x(), bounds().y(), bounds().width(), bounds().height(),
      visible_ ? "WindowVisible" : "WindowHidden",
      layer() ?
          (layer()->GetTargetVisibility() ? "LayerVisible" : "LayerHidden") :
          "NoLayer",
      layer() ? layer()->opacity() : 1.0f);
}

void Window::PrintWindowHierarchy(int depth) const {
  VLOG(0) << base::StringPrintf(
      "%*s%s", depth * 2, "", GetDebugInfo().c_str());
  for (Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    Window* child = *it;
    child->PrintWindowHierarchy(depth + 1);
  }
}
#endif

void Window::RemoveOrDestroyChildren() {
  while (!children_.empty()) {
    Window* child = children_[0];
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child so remove it from out children_ list.
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    } else {
      // Even if we can't delete the child, we still need to remove it from the
      // parent so that relevant bookkeeping (parent_ back-pointers etc) are
      // updated.
      RemoveChild(child);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Window, private:

int64 Window::SetPropertyInternal(const void* key,
                                  const char* name,
                                  PropertyDeallocator deallocator,
                                  int64 value,
                                  int64 default_value) {
  int64 old = GetPropertyInternal(key, default_value);
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
                    OnWindowPropertyChanged(this, key, old));
  return old;
}

int64 Window::GetPropertyInternal(const void* key,
                                  int64 default_value) const {
  std::map<const void*, Value>::const_iterator iter = prop_map_.find(key);
  if (iter == prop_map_.end())
    return default_value;
  return iter->second.value;
}

bool Window::HitTest(const gfx::Point& local_point) {
  gfx::Rect local_bounds(bounds().size());
  if (!delegate_ || !delegate_->HasHitTestMask())
    return local_bounds.Contains(local_point);

  gfx::Path mask;
  delegate_->GetHitTestMask(&mask);

  SkRegion clip_region;
  clip_region.setRect(local_bounds.x(), local_bounds.y(),
                      local_bounds.width(), local_bounds.height());
  SkRegion mask_region;
  return mask_region.setPath(mask, clip_region) &&
      mask_region.contains(local_point.x(), local_point.y());
}

void Window::SetBoundsInternal(const gfx::Rect& new_bounds) {
  gfx::Rect actual_new_bounds(new_bounds);
  gfx::Rect old_bounds = GetTargetBounds();

  // Always need to set the layer's bounds -- even if it is to the same thing.
  // This may cause important side effects such as stopping animation.
  if (!layer()) {
    const gfx::Vector2d origin_delta = new_bounds.OffsetFromOrigin() -
        bounds_.OffsetFromOrigin();
    bounds_ = new_bounds;
    OffsetLayerBounds(origin_delta);
  } else {
    if (parent_ && !parent_->layer()) {
      gfx::Vector2d offset;
      const aura::Window* ancestor_with_layer =
          parent_->GetAncestorWithLayer(&offset);
      if (ancestor_with_layer)
        actual_new_bounds.Offset(offset);
    }
    layer()->SetBounds(actual_new_bounds);
  }

  // If we are currently not the layer's delegate, we will not get bounds
  // changed notification from the layer (this typically happens after animating
  // hidden). We must notify ourselves.
  if (!layer() || layer()->delegate() != this)
    OnWindowBoundsChanged(old_bounds);
}

void Window::SetVisible(bool visible) {
  if ((layer() && visible == layer()->GetTargetVisibility()) ||
      (!layer() && visible == visible_))
    return;  // No change.

  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowVisibilityChanging(this, visible));

  client::VisibilityClient* visibility_client =
      client::GetVisibilityClient(this);
  if (visibility_client)
    visibility_client->UpdateLayerVisibility(this, visible);
  else if (layer())
    layer()->SetVisible(visible);
  visible_ = visible;
  SchedulePaint();
  if (parent_ && parent_->layout_manager_)
    parent_->layout_manager_->OnChildWindowVisibilityChanged(this, visible);

  if (delegate_)
    delegate_->OnWindowTargetVisibilityChanged(visible);

  NotifyWindowVisibilityChanged(this, visible);
}

void Window::SchedulePaint() {
  SchedulePaintInRect(gfx::Rect(0, 0, bounds().width(), bounds().height()));
}

void Window::Paint(gfx::Canvas* canvas) {
  if (delegate_)
    delegate_->OnPaint(canvas);
  PaintLayerlessChildren(canvas);
}

void Window::PaintLayerlessChildren(gfx::Canvas* canvas) {
  for (size_t i = 0, count = children_.size(); i < count; ++i) {
    Window* child = children_[i];
    if (!child->layer() && child->visible_) {
      gfx::ScopedCanvas scoped_canvas(canvas);
      canvas->ClipRect(child->bounds());
      if (!canvas->IsClipEmpty()) {
        canvas->Translate(child->bounds().OffsetFromOrigin());
        child->Paint(canvas);
      }
    }
  }
}

Window* Window::GetWindowForPoint(const gfx::Point& local_point,
                                  bool return_tightest,
                                  bool for_event_handling) {
  if (!IsVisible())
    return NULL;

  if ((for_event_handling && !HitTest(local_point)) ||
      (!for_event_handling && !ContainsPoint(local_point)))
    return NULL;

  // Check if I should claim this event and not pass it to my children because
  // the location is inside my hit test override area.  For details, see
  // set_hit_test_bounds_override_inner().
  if (for_event_handling && !hit_test_bounds_override_inner_.empty()) {
    gfx::Rect inset_local_bounds(gfx::Point(), bounds().size());
    inset_local_bounds.Inset(hit_test_bounds_override_inner_);
    // We know we're inside the normal local bounds, so if we're outside the
    // inset bounds we must be in the special hit test override area.
    DCHECK(HitTest(local_point));
    if (!inset_local_bounds.Contains(local_point))
      return delegate_ ? this : NULL;
  }

  if (!return_tightest && delegate_)
    return this;

  for (Windows::const_reverse_iterator it = children_.rbegin(),
           rend = children_.rend();
       it != rend; ++it) {
    Window* child = *it;

    if (for_event_handling) {
      if (child->ignore_events_)
        continue;
      // The client may not allow events to be processed by certain subtrees.
      client::EventClient* client = client::GetEventClient(GetRootWindow());
      if (client && !client->CanProcessEventsWithinSubtree(child))
        continue;
      if (delegate_ && !delegate_->ShouldDescendIntoChildForEventHandling(
              child, local_point))
        continue;
    }

    gfx::Point point_in_child_coords(local_point);
    ConvertPointToTarget(this, child, &point_in_child_coords);
    Window* match = child->GetWindowForPoint(point_in_child_coords,
                                             return_tightest,
                                             for_event_handling);
    if (match)
      return match;
  }

  return delegate_ ? this : NULL;
}

void Window::RemoveChildImpl(Window* child, Window* new_parent) {
  if (layout_manager_)
    layout_manager_->OnWillRemoveWindowFromLayout(child);
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWillRemoveWindow(child));
  Window* root_window = child->GetRootWindow();
  Window* new_root_window = new_parent ? new_parent->GetRootWindow() : NULL;
  if (root_window && root_window != new_root_window)
    child->NotifyRemovingFromRootWindow(new_root_window);

  gfx::Vector2d offset;
  GetAncestorWithLayer(&offset);
  child->UnparentLayers(!layer(), offset);
  child->parent_ = NULL;
  Windows::iterator i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->OnParentChanged();
  if (layout_manager_)
    layout_manager_->OnWindowRemovedFromLayout(child);
}

void Window::UnparentLayers(bool has_layerless_ancestor,
                            const gfx::Vector2d& offset) {
  if (!layer()) {
    const gfx::Vector2d new_offset = offset + bounds().OffsetFromOrigin();
    for (size_t i = 0; i < children_.size(); ++i) {
      children_[i]->UnparentLayers(true, new_offset);
    }
  } else {
    // Only remove the layer if we still own it.  Someone else may have acquired
    // ownership of it via AcquireLayer() and may expect the hierarchy to go
    // unchanged as the Window is destroyed.
    if (OwnsLayer()) {
      if (layer()->parent())
        layer()->parent()->Remove(layer());
      if (has_layerless_ancestor) {
        const gfx::Rect real_bounds(bounds_);
        gfx::Rect layer_bounds(layer()->bounds());
        layer_bounds.Offset(-offset);
        layer()->SetBounds(layer_bounds);
        bounds_ = real_bounds;
      }
    }
  }
}

void Window::ReparentLayers(ui::Layer* parent_layer,
                            const gfx::Vector2d& offset) {
  if (!layer()) {
    for (size_t i = 0; i < children_.size(); ++i) {
      children_[i]->ReparentLayers(
          parent_layer,
          offset + children_[i]->bounds().OffsetFromOrigin());
    }
  } else {
    const gfx::Rect real_bounds(bounds());
    parent_layer->Add(layer());
    gfx::Rect layer_bounds(layer()->bounds().size());
    layer_bounds += offset;
    layer()->SetBounds(layer_bounds);
    bounds_ = real_bounds;
  }
}

void Window::OffsetLayerBounds(const gfx::Vector2d& offset) {
  if (!layer()) {
    for (size_t i = 0; i < children_.size(); ++i)
      children_[i]->OffsetLayerBounds(offset);
  } else {
    gfx::Rect layer_bounds(layer()->bounds());
    layer_bounds += offset;
    layer()->SetBounds(layer_bounds);
  }
}

void Window::OnParentChanged() {
  FOR_EACH_OBSERVER(
      WindowObserver, observers_, OnWindowParentChanged(this, parent_));
}

void Window::StackChildRelativeTo(Window* child,
                                  Window* target,
                                  StackDirection direction) {
  DCHECK_NE(child, target);
  DCHECK(child);
  DCHECK(target);
  DCHECK_EQ(this, child->parent());
  DCHECK_EQ(this, target->parent());

  client::WindowStackingClient* stacking_client =
      client::GetWindowStackingClient();
  if (stacking_client &&
      !stacking_client->AdjustStacking(&child, &target, &direction))
    return;

  const size_t child_i =
      std::find(children_.begin(), children_.end(), child) - children_.begin();
  const size_t target_i =
      std::find(children_.begin(), children_.end(), target) - children_.begin();

  // Don't move the child if it is already in the right place.
  if ((direction == STACK_ABOVE && child_i == target_i + 1) ||
      (direction == STACK_BELOW && child_i + 1 == target_i))
    return;

  const size_t dest_i =
      direction == STACK_ABOVE ?
      (child_i < target_i ? target_i : target_i + 1) :
      (child_i < target_i ? target_i - 1 : target_i);
  children_.erase(children_.begin() + child_i);
  children_.insert(children_.begin() + dest_i, child);

  StackChildLayerRelativeTo(child, target, direction);

  child->OnStackingChanged();
}

void Window::StackChildLayerRelativeTo(Window* child,
                                       Window* target,
                                       StackDirection direction) {
  Window* ancestor_with_layer = GetAncestorWithLayer(NULL);
  ui::Layer* ancestor_layer =
      ancestor_with_layer ? ancestor_with_layer->layer() : NULL;
  if (!ancestor_layer)
    return;

  if (child->layer() && target->layer()) {
    if (direction == STACK_ABOVE)
      ancestor_layer->StackAbove(child->layer(), target->layer());
    else
      ancestor_layer->StackBelow(child->layer(), target->layer());
    return;
  }
  typedef std::vector<ui::Layer*> Layers;
  Layers layers;
  GetLayersToStack(child, &layers);
  if (layers.empty())
    return;

  ui::Layer* target_layer;
  if (direction == STACK_ABOVE) {
    target_layer =
        FindStackingTargetLayer<Windows::const_reverse_iterator>(target, child);
  } else {
    target_layer =
        FindStackingTargetLayer<Windows::const_iterator>(target, child);
  }

  if (!target_layer) {
    if (direction == STACK_ABOVE) {
      for (Layers::const_reverse_iterator i = layers.rbegin(),
               rend = layers.rend(); i != rend; ++i) {
        ancestor_layer->StackAtBottom(*i);
      }
    } else {
      for (Layers::const_iterator i = layers.begin(); i != layers.end(); ++i)
        ancestor_layer->StackAtTop(*i);
    }
    return;
  }

  if (direction == STACK_ABOVE) {
    for (Layers::const_reverse_iterator i = layers.rbegin(),
             rend = layers.rend(); i != rend; ++i) {
      ancestor_layer->StackAbove(*i, target_layer);
    }
  } else {
    for (Layers::const_iterator i = layers.begin(); i != layers.end(); ++i)
      ancestor_layer->StackBelow(*i, target_layer);
  }
}

void Window::OnStackingChanged() {
  FOR_EACH_OBSERVER(WindowObserver, observers_, OnWindowStackingChanged(this));
}

void Window::NotifyRemovingFromRootWindow(Window* new_root) {
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowRemovingFromRootWindow(this, new_root));
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyRemovingFromRootWindow(new_root);
  }
}

void Window::NotifyAddedToRootWindow() {
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowAddedToRootWindow(this));
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyAddedToRootWindow();
  }
}

void Window::NotifyWindowHierarchyChange(
    const WindowObserver::HierarchyChangeParams& params) {
  params.target->NotifyWindowHierarchyChangeDown(params);
  switch (params.phase) {
  case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING:
    if (params.old_parent)
      params.old_parent->NotifyWindowHierarchyChangeUp(params);
    break;
  case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED:
    if (params.new_parent)
      params.new_parent->NotifyWindowHierarchyChangeUp(params);
    break;
  default:
    NOTREACHED();
    break;
  }
}

void Window::NotifyWindowHierarchyChangeDown(
    const WindowObserver::HierarchyChangeParams& params) {
  NotifyWindowHierarchyChangeAtReceiver(params);
  for (Window::Windows::const_iterator it = children_.begin();
       it != children_.end(); ++it) {
    (*it)->NotifyWindowHierarchyChangeDown(params);
  }
}

void Window::NotifyWindowHierarchyChangeUp(
    const WindowObserver::HierarchyChangeParams& params) {
  for (Window* window = this; window; window = window->parent())
    window->NotifyWindowHierarchyChangeAtReceiver(params);
}

void Window::NotifyWindowHierarchyChangeAtReceiver(
    const WindowObserver::HierarchyChangeParams& params) {
  WindowObserver::HierarchyChangeParams local_params = params;
  local_params.receiver = this;

  switch (params.phase) {
  case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGING:
    FOR_EACH_OBSERVER(WindowObserver, observers_,
                      OnWindowHierarchyChanging(local_params));
    break;
  case WindowObserver::HierarchyChangeParams::HIERARCHY_CHANGED:
    FOR_EACH_OBSERVER(WindowObserver, observers_,
                      OnWindowHierarchyChanged(local_params));
    break;
  default:
    NOTREACHED();
    break;
  }
}

void Window::NotifyWindowVisibilityChanged(aura::Window* target,
                                           bool visible) {
  if (!NotifyWindowVisibilityChangedDown(target, visible)) {
    return; // |this| has been deleted.
  }
  NotifyWindowVisibilityChangedUp(target, visible);
}

bool Window::NotifyWindowVisibilityChangedAtReceiver(aura::Window* target,
                                                     bool visible) {
  // |this| may be deleted during a call to OnWindowVisibilityChanged() on one
  // of the observers. We create an local observer for that. In that case we
  // exit without further access to any members.
  WindowTracker tracker;
  tracker.Add(this);
  FOR_EACH_OBSERVER(WindowObserver, observers_,
                    OnWindowVisibilityChanged(target, visible));
  return tracker.Contains(this);
}

bool Window::NotifyWindowVisibilityChangedDown(aura::Window* target,
                                               bool visible) {
  if (!NotifyWindowVisibilityChangedAtReceiver(target, visible))
    return false; // |this| was deleted.
  std::set<const Window*> child_already_processed;
  bool child_destroyed = false;
  do {
    child_destroyed = false;
    for (Window::Windows::const_iterator it = children_.begin();
         it != children_.end(); ++it) {
      if (!child_already_processed.insert(*it).second)
        continue;
      if (!(*it)->NotifyWindowVisibilityChangedDown(target, visible)) {
        // |*it| was deleted, |it| is invalid and |children_| has changed.
        // We exit the current for-loop and enter a new one.
        child_destroyed = true;
        break;
      }
    }
  } while (child_destroyed);
  return true;
}

void Window::NotifyWindowVisibilityChangedUp(aura::Window* target,
                                             bool visible) {
  for (Window* window = this; window; window = window->parent()) {
    bool ret = window->NotifyWindowVisibilityChangedAtReceiver(target, visible);
    DCHECK(ret);
  }
}

void Window::OnWindowBoundsChanged(const gfx::Rect& old_bounds) {
  if (layer()) {
    bounds_ = layer()->bounds();
    if (parent_ && !parent_->layer()) {
      gfx::Vector2d offset;
      aura::Window* ancestor_with_layer =
          parent_->GetAncestorWithLayer(&offset);
      if (ancestor_with_layer)
        bounds_.Offset(-offset);
    }
  }

  if (layout_manager_)
    layout_manager_->OnWindowResized();
  if (delegate_)
    delegate_->OnBoundsChanged(old_bounds, bounds());
  FOR_EACH_OBSERVER(WindowObserver,
                    observers_,
                    OnWindowBoundsChanged(this, old_bounds, bounds()));
}

bool Window::CleanupGestureState() {
  bool state_modified = false;
  state_modified |= ui::GestureRecognizer::Get()->CancelActiveTouches(this);
  state_modified |=
      ui::GestureRecognizer::Get()->CleanupStateForConsumer(this);
  for (Window::Windows::iterator iter = children_.begin();
       iter != children_.end();
       ++iter) {
    state_modified |= (*iter)->CleanupGestureState();
  }
  return state_modified;
}

void Window::OnPaintLayer(gfx::Canvas* canvas) {
  Paint(canvas);
}

base::Closure Window::PrepareForLayerBoundsChange() {
  return base::Bind(&Window::OnWindowBoundsChanged, base::Unretained(this),
                    bounds());
}

bool Window::CanAcceptEvent(const ui::Event& event) {
  // The client may forbid certain windows from receiving events at a given
  // point in time.
  client::EventClient* client = client::GetEventClient(GetRootWindow());
  if (client && !client->CanProcessEventsWithinSubtree(this))
    return false;

  // We need to make sure that a touch cancel event and any gesture events it
  // creates can always reach the window. This ensures that we receive a valid
  // touch / gesture stream.
  if (event.IsEndingEvent())
    return true;

  if (!IsVisible())
    return false;

  // The top-most window can always process an event.
  if (!parent_)
    return true;

  // For located events (i.e. mouse, touch etc.), an assumption is made that
  // windows that don't have a default event-handler cannot process the event
  // (see more in GetWindowForPoint()). This assumption is not made for key
  // events.
  return event.IsKeyEvent() || target_handler();
}

ui::EventTarget* Window::GetParentTarget() {
  if (IsRootWindow()) {
    return client::GetEventClient(this) ?
        client::GetEventClient(this)->GetToplevelEventTarget() :
            Env::GetInstance();
  }
  return parent_;
}

scoped_ptr<ui::EventTargetIterator> Window::GetChildIterator() const {
  return scoped_ptr<ui::EventTargetIterator>(
      new ui::EventTargetIteratorImpl<Window>(children()));
}

ui::EventTargeter* Window::GetEventTargeter() {
  return targeter_.get();
}

void Window::ConvertEventToTarget(ui::EventTarget* target,
                                  ui::LocatedEvent* event) {
  event->ConvertLocationToTarget(this,
                                 static_cast<Window*>(target));
}

void Window::UpdateLayerName() {
#if !defined(NDEBUG)
  DCHECK(layer());

  std::string layer_name(name_);
  if (layer_name.empty())
    layer_name = "Unnamed Window";

  if (id_ != -1)
    layer_name += " " + base::IntToString(id_);

  layer()->set_name(layer_name);
#endif
}

bool Window::ContainsMouse() {
  bool contains_mouse = false;
  if (IsVisible()) {
    WindowTreeHost* host = GetHost();
    contains_mouse = host &&
        ContainsPointInRoot(host->dispatcher()->GetLastMouseLocationInRoot());
  }
  return contains_mouse;
}

const Window* Window::GetAncestorWithLayer(gfx::Vector2d* offset) const {
  for (const aura::Window* window = this; window; window = window->parent()) {
    if (window->layer())
      return window;
    if (offset)
      *offset += window->bounds().OffsetFromOrigin();
  }
  if (offset)
    *offset = gfx::Vector2d();
  return NULL;
}

}  // namespace aura
