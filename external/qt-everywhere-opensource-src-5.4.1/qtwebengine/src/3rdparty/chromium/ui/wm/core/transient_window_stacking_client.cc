// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/transient_window_stacking_client.h"

#include <algorithm>

#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

using aura::Window;

namespace wm {

namespace {

// Populates |ancestors| with all transient ancestors of |window| that are
// siblings of |window|. Returns true if any ancestors were found, false if not.
bool GetAllTransientAncestors(Window* window, Window::Windows* ancestors) {
  Window* parent = window->parent();
  for (; window; window = GetTransientParent(window)) {
    if (window->parent() == parent)
      ancestors->push_back(window);
  }
  return (!ancestors->empty());
}

// Replaces |window1| and |window2| with their possible transient ancestors that
// are still siblings (have a common transient parent).  |window1| and |window2|
// are not modified if such ancestors cannot be found.
void FindCommonTransientAncestor(Window** window1, Window** window2) {
  DCHECK(window1);
  DCHECK(window2);
  DCHECK(*window1);
  DCHECK(*window2);
  // Assemble chains of ancestors of both windows.
  Window::Windows ancestors1;
  Window::Windows ancestors2;
  if (!GetAllTransientAncestors(*window1, &ancestors1) ||
      !GetAllTransientAncestors(*window2, &ancestors2)) {
    return;
  }
  // Walk the two chains backwards and look for the first difference.
  Window::Windows::reverse_iterator it1 = ancestors1.rbegin();
  Window::Windows::reverse_iterator it2 = ancestors2.rbegin();
  for (; it1  != ancestors1.rend() && it2  != ancestors2.rend(); ++it1, ++it2) {
    if (*it1 != *it2) {
      *window1 = *it1;
      *window2 = *it2;
      break;
    }
  }
}

// Adjusts |target| so that we don't attempt to stack on top of a window with a
// NULL delegate.
void SkipNullDelegates(Window::StackDirection direction, Window** target) {
  const Window::Windows& children((*target)->parent()->children());
  size_t target_i =
      std::find(children.begin(), children.end(), *target) -
      children.begin();

  // By convention we don't stack on top of windows with layers with NULL
  // delegates.  Walk backward to find a valid target window.  See tests
  // TransientWindowManagerTest.StackingMadrigal and StackOverClosingTransient
  // for an explanation of this.
  while (target_i > 0) {
    const size_t index = direction == Window::STACK_ABOVE ?
        target_i : target_i - 1;
    if (!children[index]->layer() ||
        children[index]->layer()->delegate() != NULL)
      break;
    --target_i;
  }
  *target = children[target_i];
}

}  // namespace

// static
TransientWindowStackingClient* TransientWindowStackingClient::instance_ = NULL;

TransientWindowStackingClient::TransientWindowStackingClient() {
  instance_ = this;
}

TransientWindowStackingClient::~TransientWindowStackingClient() {
  if (instance_ == this)
    instance_ = NULL;
}

bool TransientWindowStackingClient::AdjustStacking(
    Window** child,
    Window** target,
    Window::StackDirection* direction) {
  const TransientWindowManager* transient_manager =
      TransientWindowManager::Get(static_cast<const Window*>(*child));
  if (transient_manager && transient_manager->IsStackingTransient(*target))
    return true;

  // For windows that have transient children stack the transient ancestors that
  // are siblings. This prevents one transient group from being inserted in the
  // middle of another.
  FindCommonTransientAncestor(child, target);

  // When stacking above skip to the topmost transient descendant of the target.
  if (*direction == Window::STACK_ABOVE &&
      !HasTransientAncestor(*child, *target)) {
    const Window::Windows& siblings((*child)->parent()->children());
    size_t target_i =
        std::find(siblings.begin(), siblings.end(), *target) - siblings.begin();
    while (target_i + 1 < siblings.size() &&
           HasTransientAncestor(siblings[target_i + 1], *target)) {
      ++target_i;
    }
    *target = siblings[target_i];
  }

  SkipNullDelegates(*direction, target);

  // If we couldn't find a valid target position, don't move anything.
  if (*direction == Window::STACK_ABOVE &&
      ((*target)->layer() && (*target)->layer()->delegate() == NULL)) {
    return false;
  }

  return *child != *target;
}

}  // namespace wm
