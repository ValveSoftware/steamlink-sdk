// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_TRANSIENT_WINDOW_UTILS_H_
#define COMPONENTS_MUS_COMMON_TRANSIENT_WINDOW_UTILS_H_

#include <stddef.h>

#include <vector>

#include "components/mus/public/interfaces/mus_constants.mojom.h"

namespace mus {

// Returns true if |window| has |ancestor| as a transient ancestor. A transient
// ancestor is found by following the transient parent chain of the window.
template <class T>
bool HasTransientAncestor(const T* window, const T* ancestor) {
  const T* transient_parent = window->transient_parent();
  if (transient_parent == ancestor)
    return true;
  return transient_parent ? HasTransientAncestor(transient_parent, ancestor)
                          : false;
}

// Populates |ancestors| with all transient ancestors of |window| that are
// siblings of |window|. Returns true if any ancestors were found, false if not.
template <class T>
bool GetAllTransientAncestors(T* window, std::vector<T*>* ancestors) {
  T* parent = window->parent();
  for (; window; window = window->transient_parent()) {
    if (window->parent() == parent)
      ancestors->push_back(window);
  }
  return !ancestors->empty();
}

// Replaces |window1| and |window2| with their possible transient ancestors that
// are still siblings (have a common transient parent).  |window1| and |window2|
// are not modified if such ancestors cannot be found.
template <class T>
void FindCommonTransientAncestor(T** window1, T** window2) {
  DCHECK(window1);
  DCHECK(window2);
  DCHECK(*window1);
  DCHECK(*window2);
  // Assemble chains of ancestors of both windows.
  std::vector<T*> ancestors1;
  std::vector<T*> ancestors2;
  if (!GetAllTransientAncestors(*window1, &ancestors1) ||
      !GetAllTransientAncestors(*window2, &ancestors2)) {
    return;
  }
  // Walk the two chains backwards and look for the first difference.
  auto it1 = ancestors1.rbegin();
  auto it2 = ancestors2.rbegin();
  for (; it1 != ancestors1.rend() && it2 != ancestors2.rend(); ++it1, ++it2) {
    if (*it1 != *it2) {
      *window1 = *it1;
      *window2 = *it2;
      break;
    }
  }
}

template <class T>
bool AdjustStackingForTransientWindows(T** child,
                                       T** target,
                                       mojom::OrderDirection* direction,
                                       T* stacking_target) {
  if (stacking_target == *target)
    return true;

  // For windows that have transient children stack the transient ancestors that
  // are siblings. This prevents one transient group from being inserted in the
  // middle of another.
  FindCommonTransientAncestor(child, target);

  // When stacking above skip to the topmost transient descendant of the target.
  if (*direction == mojom::OrderDirection::ABOVE &&
      !HasTransientAncestor(*child, *target)) {
    const std::vector<T*>& siblings((*child)->parent()->children());
    size_t target_i =
        std::find(siblings.begin(), siblings.end(), *target) - siblings.begin();
    while (target_i + 1 < siblings.size() &&
           HasTransientAncestor(siblings[target_i + 1], *target)) {
      ++target_i;
    }
    *target = siblings[target_i];
  }

  return *child != *target;
}

// Stacks transient descendants of |window| that are its siblings just above it.
// |GetStackingTarget| is a function that returns a marker associated with a
// Window that indicates the current Window being stacked.
// |Reorder| is a function that takes in two windows and orders the first
// relative to the second based on the provided OrderDirection.
template <class T>
void RestackTransientDescendants(T* window,
                                 T** (*GetStackingTarget)(T*),
                                 void (*Reorder)(T*,
                                                 T*,
                                                 mojom::OrderDirection)) {
  T* parent = window->parent();
  if (!parent)
    return;

  // stack any transient children that share the same parent to be in front of
  // |window_|. the existing stacking order is preserved by iterating backwards
  // and always stacking on top.
  std::vector<T*> children(parent->children());
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    if ((*it) != window && HasTransientAncestor(*it, window)) {
      T* old_stacking_target = *GetStackingTarget(*it);
      *GetStackingTarget(*it) = window;
      Reorder(*it, window, mojom::OrderDirection::ABOVE);
      *GetStackingTarget(*it) = old_stacking_target;
    }
  }
}
}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_TRANSIENT_WINDOW_UTILS_H_
