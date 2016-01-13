// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/v2/public/view.h"

#include <algorithm>

#include "base/bind.h"
#include "ui/compositor/layer_owner.h"
#include "ui/v2/public/view_observer.h"
#include "ui/v2/src/view_private.h"

namespace v2 {

enum StackDirection {
  STACK_ABOVE,
  STACK_BELOW
};

void StackChildRelativeTo(View* parent,
                          std::vector<View*>* children,
                          View* child,
                          View* other,
                          StackDirection direction) {
  DCHECK_NE(child, other);
  DCHECK(child);
  DCHECK(other);
  DCHECK_EQ(parent, child->parent());
  DCHECK_EQ(parent, other->parent());

  // TODO(beng): Notify stacking changing.
  // TODO(beng): consult layout manager
  const size_t child_i =
      std::find(children->begin(), children->end(), child) - children->begin();
  const size_t other_i =
      std::find(children->begin(), children->end(), other) - children->begin();
  const size_t destination_i =
      direction == STACK_ABOVE ?
      (child_i < other_i ? other_i : other_i + 1) :
      (child_i < other_i ? other_i - 1 : other_i);
  children->erase(children->begin() + child_i);
  children->insert(children->begin() + destination_i, child);

  // TODO(beng): update layer.
  // TODO(beng): Notify stacking changed.
}

void NotifyViewTreeChangeAtReceiver(
    View* receiver,
    const ViewObserver::TreeChangeParams& params) {
  ViewObserver::TreeChangeParams local_params = params;
  local_params.receiver = receiver;
  FOR_EACH_OBSERVER(ViewObserver,
                    *ViewPrivate(receiver).observers(),
                    OnViewTreeChange(local_params));
}

void NotifyViewTreeChangeUp(View* start_at,
                            const ViewObserver::TreeChangeParams& params) {
  for (View* current = start_at; current; current = current->parent())
    NotifyViewTreeChangeAtReceiver(current, params);
}

void NotifyViewTreeChangeDown(View* start_at,
                              const ViewObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeAtReceiver(start_at, params);
  View::Children::const_iterator it = start_at->children().begin();
  for (; it != start_at->children().end(); ++it)
    NotifyViewTreeChangeDown(*it, params);
}

void NotifyViewTreeChange(const ViewObserver::TreeChangeParams& params) {
  NotifyViewTreeChangeDown(params.target, params);
  switch (params.phase) {
  case ViewObserver::DISPOSITION_CHANGING:
    if (params.old_parent)
      NotifyViewTreeChangeUp(params.old_parent, params);
    break;
  case ViewObserver::DISPOSITION_CHANGED:
    if (params.new_parent)
      NotifyViewTreeChangeUp(params.new_parent, params);
    break;
  default:
    NOTREACHED();
    break;
  }
}

class ScopedTreeNotifier {
 public:
  ScopedTreeNotifier(View* target, View* old_parent, View* new_parent) {
    params_.target = target;
    params_.old_parent = old_parent;
    params_.new_parent = new_parent;
    NotifyViewTreeChange(params_);
  }
  ~ScopedTreeNotifier() {
    params_.phase = ViewObserver::DISPOSITION_CHANGED;
    NotifyViewTreeChange(params_);
  }

 private:
  ViewObserver::TreeChangeParams params_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTreeNotifier);
};

void RemoveChildImpl(View* child, View::Children* children) {
  std::vector<View*>::iterator it =
      std::find(children->begin(), children->end(), child);
  if (it != children->end()) {
    children->erase(it);
    ViewPrivate(child).ClearParent();
  }
}

class ViewLayerOwner : public ui::LayerOwner,
                       public ui::LayerDelegate {
 public:
  explicit ViewLayerOwner(ui::Layer* layer) {
    layer_ = layer;
  }
  ~ViewLayerOwner() {}

 private:
  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    // TODO(beng): paint processor.
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
    // TODO(beng): ???
  }
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Bind(&ViewLayerOwner::OnLayerBoundsChanged,
                      base::Unretained(this));
  }

  void OnLayerBoundsChanged() {
    // TODO(beng): ???
  }

  DISALLOW_COPY_AND_ASSIGN(ViewLayerOwner);
};

////////////////////////////////////////////////////////////////////////////////
// View, public:

// Creation, configuration -----------------------------------------------------

View::View() : visible_(true), owned_by_parent_(true), parent_(NULL) {
}

View::~View() {
  FOR_EACH_OBSERVER(ViewObserver, observers_,
                    OnViewDestroy(this, ViewObserver::DISPOSITION_CHANGING));

  while (!children_.empty()) {
    View* child = children_.front();
    if (child->owned_by_parent_) {
      delete child;
      // Deleting the child also removes it from our child list.
      DCHECK(std::find(children_.begin(), children_.end(), child) ==
             children_.end());
    } else {
      RemoveChild(child);
    }
  }

  if (parent_)
    parent_->RemoveChild(this);

  FOR_EACH_OBSERVER(ViewObserver, observers_,
                    OnViewDestroy(this, ViewObserver::DISPOSITION_CHANGED));
}

void View::AddObserver(ViewObserver* observer) {
  observers_.AddObserver(observer);
}

void View::RemoveObserver(ViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

void View::SetPainter(Painter* painter) {
  painter_.reset(painter);
}

void View::SetLayout(Layout* layout) {
  layout_.reset(layout);
}

// Disposition -----------------------------------------------------------------

void View::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect old_bounds = bounds_;
  // TODO(beng): consult layout manager
  bounds_ = bounds;
  // TODO(beng): update layer

  // TODO(beng): write tests for this where layoutmanager prevents a change
  //             and no changed notification is sent.
  if (bounds_ != old_bounds) {
    FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewBoundsChanged(this,
        old_bounds, bounds_));
  }
}

void View::SetVisible(bool visible) {
  FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewVisibilityChange(this,
      ViewObserver::DISPOSITION_CHANGING));

  bool old_visible = visible_;
  // TODO(beng): consult layout manager
  visible_ = visible;
  // TODO(beng): update layer

  // TODO(beng): write tests for this where layoutmanager prevents a change
  //             and no changed notification is sent.
  if (old_visible != visible_) {
    FOR_EACH_OBSERVER(ViewObserver, observers_, OnViewVisibilityChange(this,
        ViewObserver::DISPOSITION_CHANGED));
  }
}

// Tree ------------------------------------------------------------------------

void View::AddChild(View* child) {
  ScopedTreeNotifier notifier(child, child->parent(), this);
  if (child->parent())
    RemoveChildImpl(child, &child->parent_->children_);
  children_.push_back(child);
  child->parent_ = this;
}

void View::RemoveChild(View* child) {
  DCHECK_EQ(this, child->parent());
  ScopedTreeNotifier(child, this, NULL);
  RemoveChildImpl(child, &children_);
}

bool View::Contains(View* child) const {
  for (View* p = child->parent(); p; p = p->parent()) {
    if (p == this)
      return true;
  }
  return false;
}

void View::StackChildAtTop(View* child) {
  if (children_.size() <= 1 || child == children_.back())
    return;  // On top already.
  StackChildAbove(child, children_.back());
}

void View::StackChildAtBottom(View* child) {
  if (children_.size() <= 1 || child == children_.front())
    return;  // On bottom already.
  StackChildBelow(child, children_.front());
}

void View::StackChildAbove(View* child, View* other) {
  StackChildRelativeTo(this, &children_, child, other, STACK_ABOVE);
}

void View::StackChildBelow(View* child, View* other) {
  StackChildRelativeTo(this, &children_, child, other, STACK_BELOW);
}

// Layer -----------------------------------------------------------------------

const ui::Layer* View::layer() const {
  return layer_owner_.get() ? layer_owner_->layer() : NULL;
}

ui::Layer* View::layer() {
  return const_cast<ui::Layer*>(const_cast<const View*>(this)->layer());
}

bool View::HasLayer() const {
  return !!layer();
}

void View::CreateLayer(ui::LayerType layer_type) {
  layer_owner_.reset(new ViewLayerOwner(new ui::Layer(layer_type)));
  layer()->SetVisible(visible_);
  layer()->set_delegate(layer_owner_.get());
  // TODO(beng): layer name?
  // TODO(beng): SetFillsBoundsOpaquely?
}

void View::DestroyLayer() {
  DCHECK(layer_owner_.get());
  layer_owner_.reset();
}

ui::Layer* View::AcquireLayer() {
  DCHECK(layer_owner_.get());
  return layer_owner_->AcquireLayer();
}

}  // namespace v2
