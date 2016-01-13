// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_manager.h"

#include <algorithm>
#include <functional>

#include "base/auto_reset.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/wm/core/transient_window_observer.h"
#include "ui/wm/core/transient_window_stacking_client.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

namespace wm {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(TransientWindowManager, kPropertyKey, NULL);

TransientWindowManager::~TransientWindowManager() {
}

// static
TransientWindowManager* TransientWindowManager::Get(Window* window) {
  TransientWindowManager* manager = window->GetProperty(kPropertyKey);
  if (!manager) {
    manager = new TransientWindowManager(window);
    window->SetProperty(kPropertyKey, manager);
  }
  return manager;
}

// static
const TransientWindowManager* TransientWindowManager::Get(
    const Window* window) {
  return window->GetProperty(kPropertyKey);
}

void TransientWindowManager::AddObserver(TransientWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void TransientWindowManager::RemoveObserver(TransientWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TransientWindowManager::AddTransientChild(Window* child) {
  // TransientWindowStackingClient does the stacking of transient windows. If it
  // isn't installed stacking is going to be wrong.
  DCHECK(TransientWindowStackingClient::instance_);

  TransientWindowManager* child_manager = Get(child);
  if (child_manager->transient_parent_)
    Get(child_manager->transient_parent_)->RemoveTransientChild(child);
  DCHECK(std::find(transient_children_.begin(), transient_children_.end(),
                   child) == transient_children_.end());
  transient_children_.push_back(child);
  child_manager->transient_parent_ = window_;

  // Restack |child| properly above its transient parent, if they share the same
  // parent.
  if (child->parent() == window_->parent())
    RestackTransientDescendants();

  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildAdded(window_, child));
}

void TransientWindowManager::RemoveTransientChild(Window* child) {
  Windows::iterator i =
      std::find(transient_children_.begin(), transient_children_.end(), child);
  DCHECK(i != transient_children_.end());
  transient_children_.erase(i);
  TransientWindowManager* child_manager = Get(child);
  DCHECK_EQ(window_, child_manager->transient_parent_);
  child_manager->transient_parent_ = NULL;

  // If |child| and its former transient parent share the same parent, |child|
  // should be restacked properly so it is not among transient children of its
  // former parent, anymore.
  if (window_->parent() == child->parent())
    RestackTransientDescendants();

  FOR_EACH_OBSERVER(TransientWindowObserver, observers_,
                    OnTransientChildRemoved(window_, child));
}

bool TransientWindowManager::IsStackingTransient(
    const aura::Window* target) const {
  return stacking_target_ == target;
}

TransientWindowManager::TransientWindowManager(Window* window)
    : window_(window),
      transient_parent_(NULL),
      stacking_target_(NULL) {
  window_->AddObserver(this);
}

void TransientWindowManager::RestackTransientDescendants() {
  Window* parent = window_->parent();
  if (!parent)
    return;

  // Stack any transient children that share the same parent to be in front of
  // |window_|. The existing stacking order is preserved by iterating backwards
  // and always stacking on top.
  Window::Windows children(parent->children());
  for (Window::Windows::reverse_iterator it = children.rbegin();
       it != children.rend(); ++it) {
    if ((*it) != window_ && HasTransientAncestor(*it, window_)) {
      TransientWindowManager* descendant_manager = Get(*it);
      base::AutoReset<Window*> resetter(
          &descendant_manager->stacking_target_,
          window_);
      parent->StackChildAbove((*it), window_);
    }
  }
}

void TransientWindowManager::OnWindowParentChanged(aura::Window* window,
                                                   aura::Window* parent) {
  DCHECK_EQ(window_, window);
  // Stack |window| properly if it is transient child of a sibling.
  Window* transient_parent = wm::GetTransientParent(window);
  if (transient_parent && transient_parent->parent() == parent) {
    TransientWindowManager* transient_parent_manager =
        Get(transient_parent);
    transient_parent_manager->RestackTransientDescendants();
  }
}

void TransientWindowManager::OnWindowVisibilityChanging(Window* window,
                                                        bool visible) {
  // TODO(sky): move handling of becoming visible here.
  if (!visible) {
    std::for_each(transient_children_.begin(), transient_children_.end(),
                  std::mem_fun(&Window::Hide));
  }
}

void TransientWindowManager::OnWindowStackingChanged(Window* window) {
  DCHECK_EQ(window_, window);

  // Do nothing if we initiated the stacking change.
  const TransientWindowManager* transient_manager =
      Get(static_cast<const Window*>(window));
  if (transient_manager && transient_manager->stacking_target_) {
    Windows::const_iterator window_i = std::find(
        window->parent()->children().begin(),
        window->parent()->children().end(),
        window);
    DCHECK(window_i != window->parent()->children().end());
    if (window_i != window->parent()->children().begin() &&
        (*(window_i - 1) == transient_manager->stacking_target_))
      return;
  }

  RestackTransientDescendants();
}

void TransientWindowManager::OnWindowDestroying(Window* window) {
  // Removes ourselves from our transient parent (if it hasn't been done by the
  // RootWindow).
  if (transient_parent_) {
    TransientWindowManager::Get(transient_parent_)->RemoveTransientChild(
        window_);
  }

  // Destroy transient children, only after we've removed ourselves from our
  // parent, as destroying an active transient child may otherwise attempt to
  // refocus us.
  Windows transient_children(transient_children_);
  STLDeleteElements(&transient_children);
  DCHECK(transient_children_.empty());
}

}  // namespace wm
