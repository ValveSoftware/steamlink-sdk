// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/node.h"

#include "mojo/services/view_manager/node_delegate.h"
#include "mojo/services/view_manager/view.h"
#include "ui/aura/window_property.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/native_widget_types.h"

DECLARE_WINDOW_PROPERTY_TYPE(mojo::view_manager::service::Node*);

namespace mojo {
namespace view_manager {
namespace service {

DEFINE_WINDOW_PROPERTY_KEY(Node*, kNodeKey, NULL);

Node::Node(NodeDelegate* delegate, const NodeId& id)
    : delegate_(delegate),
      id_(id),
      view_(NULL),
      window_(this) {
  DCHECK(delegate);  // Must provide a delegate.
  window_.set_owned_by_parent(false);
  window_.AddObserver(this);
  window_.SetProperty(kNodeKey, this);
  window_.Init(aura::WINDOW_LAYER_TEXTURED);

  // TODO(sky): this likely needs to be false and add a visibility API.
  window_.Show();
}

Node::~Node() {
  SetView(NULL);
  // This is implicitly done during deletion of the window, but we do it here so
  // that we're in a known state.
  if (window_.parent())
    window_.parent()->RemoveChild(&window_);
}

const Node* Node::GetParent() const {
  if (!window_.parent())
    return NULL;
  return window_.parent()->GetProperty(kNodeKey);
}

void Node::Add(Node* child) {
  window_.AddChild(&child->window_);
}

void Node::Remove(Node* child) {
  window_.RemoveChild(&child->window_);
}

void Node::Reorder(Node* child, Node* relative, OrderDirection direction) {
  if (direction == ORDER_ABOVE)
    window_.StackChildAbove(child->window(), relative->window());
  else if (direction == ORDER_BELOW)
    window_.StackChildBelow(child->window(), relative->window());
}

const Node* Node::GetRoot() const {
  const aura::Window* window = &window_;
  while (window && window->parent())
    window = window->parent();
  return window->GetProperty(kNodeKey);
}

std::vector<const Node*> Node::GetChildren() const {
  std::vector<const Node*> children;
  children.reserve(window_.children().size());
  for (size_t i = 0; i < window_.children().size(); ++i)
    children.push_back(window_.children()[i]->GetProperty(kNodeKey));
  return children;
}

std::vector<Node*> Node::GetChildren() {
  std::vector<Node*> children;
  children.reserve(window_.children().size());
  for (size_t i = 0; i < window_.children().size(); ++i)
    children.push_back(window_.children()[i]->GetProperty(kNodeKey));
  return children;
}

bool Node::Contains(const Node* node) const {
  return node && window_.Contains(&(node->window_));
}

void Node::SetView(View* view) {
  if (view == view_)
    return;

  // Detach view from existing node. This way notifications are sent out.
  if (view && view->node())
    view->node()->SetView(NULL);

  View* old_view = view_;
  if (view_)
    view_->set_node(NULL);
  view_ = view;
  if (view)
    view->set_node(this);
  delegate_->OnNodeViewReplaced(this, view, old_view);
}

void Node::OnWindowHierarchyChanged(
    const aura::WindowObserver::HierarchyChangeParams& params) {
  if (params.target != &window_ || params.receiver != &window_)
    return;
  const Node* new_parent = params.new_parent ?
      params.new_parent->GetProperty(kNodeKey) : NULL;
  const Node* old_parent = params.old_parent ?
      params.old_parent->GetProperty(kNodeKey) : NULL;
  delegate_->OnNodeHierarchyChanged(this, new_parent, old_parent);
}

gfx::Size Node::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size Node::GetMaximumSize() const {
  return gfx::Size();
}

void Node::OnBoundsChanged(const gfx::Rect& old_bounds,
                           const gfx::Rect& new_bounds) {
}

gfx::NativeCursor Node::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int Node::GetNonClientComponent(const gfx::Point& point) const {
  return HTCAPTION;
}

bool Node::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool Node::CanFocus() {
  return true;
}

void Node::OnCaptureLost() {
}

void Node::OnPaint(gfx::Canvas* canvas) {
  if (view_) {
    canvas->DrawImageInt(
        gfx::ImageSkia::CreateFrom1xBitmap(view_->bitmap()), 0, 0);
  }
}

void Node::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

void Node::OnWindowDestroying(aura::Window* window) {
}

void Node::OnWindowDestroyed(aura::Window* window) {
}

void Node::OnWindowTargetVisibilityChanged(bool visible) {
}

bool Node::HasHitTestMask() const {
  return false;
}

void Node::GetHitTestMask(gfx::Path* mask) const {
}

void Node::OnEvent(ui::Event* event) {
  if (view_)
    delegate_->OnViewInputEvent(view_, event);
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
