// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"

namespace views {

FocusSearch::FocusSearch(View* root, bool cycle, bool accessibility_mode)
    : root_(root),
      cycle_(cycle),
      accessibility_mode_(accessibility_mode) {
}

View* FocusSearch::FindNextFocusableView(View* starting_view,
                                         bool reverse,
                                         Direction direction,
                                         bool check_starting_view,
                                         FocusTraversable** focus_traversable,
                                         View** focus_traversable_view) {
  *focus_traversable = NULL;
  *focus_traversable_view = NULL;

  if (!root_->has_children()) {
    NOTREACHED();
    // Nothing to focus on here.
    return NULL;
  }

  View* initial_starting_view = starting_view;
  int starting_view_group = -1;
  if (starting_view)
    starting_view_group = starting_view->GetGroup();

  if (!starting_view) {
    // Default to the first/last child
    starting_view = reverse ? root_->child_at(root_->child_count() - 1) :
        root_->child_at(0);
    // If there was no starting view, then the one we select is a potential
    // focus candidate.
    check_starting_view = true;
  } else {
    // The starting view should be a direct or indirect child of the root.
    DCHECK(Contains(root_, starting_view));
  }

  View* v = NULL;
  if (!reverse) {
    v = FindNextFocusableViewImpl(starting_view, check_starting_view,
                                  true,
                                  (direction == DOWN),
                                  starting_view_group,
                                  focus_traversable,
                                  focus_traversable_view);
  } else {
    // If the starting view is focusable, we don't want to go down, as we are
    // traversing the view hierarchy tree bottom-up.
    bool can_go_down = (direction == DOWN) && !IsFocusable(starting_view);
    v = FindPreviousFocusableViewImpl(starting_view, check_starting_view,
                                      true,
                                      can_go_down,
                                      starting_view_group,
                                      focus_traversable,
                                      focus_traversable_view);
  }

  // Don't set the focus to something outside of this view hierarchy.
  if (v && v != root_ && !Contains(root_, v))
    v = NULL;

  // If |cycle_| is true, prefer to keep cycling rather than returning NULL.
  if (cycle_ && !v && initial_starting_view) {
    v = FindNextFocusableView(NULL, reverse, direction, check_starting_view,
                              focus_traversable, focus_traversable_view);
    DCHECK(IsFocusable(v));
    return v;
  }

  // Doing some sanity checks.
  if (v) {
    DCHECK(IsFocusable(v));
    return v;
  }
  if (*focus_traversable) {
    DCHECK(*focus_traversable_view);
    return NULL;
  }
  // Nothing found.
  return NULL;
}

bool FocusSearch::IsViewFocusableCandidate(View* v, int skip_group_id) {
  return IsFocusable(v) &&
      (v->IsGroupFocusTraversable() || skip_group_id == -1 ||
       v->GetGroup() != skip_group_id);
}

bool FocusSearch::IsFocusable(View* v) {
  if (accessibility_mode_)
    return v && v->IsAccessibilityFocusable();
  return v && v->IsFocusable();
}

View* FocusSearch::FindSelectedViewForGroup(View* view) {
  if (view->IsGroupFocusTraversable() ||
      view->GetGroup() == -1)  // No group for that view.
    return view;

  View* selected_view = view->GetSelectedViewForGroup(view->GetGroup());
  if (selected_view)
    return selected_view;

  // No view selected for that group, default to the specified view.
  return view;
}

View* FocusSearch::GetParent(View* v) {
  return Contains(root_, v) ? v->parent() : NULL;
}

bool FocusSearch::Contains(View* root, const View* v) {
  return root->Contains(v);
}

// Strategy for finding the next focusable view:
// - keep going down the first child, stop when you find a focusable view or
//   a focus traversable view (in that case return it) or when you reach a view
//   with no children.
// - go to the right sibling and start the search from there (by invoking
//   FindNextFocusableViewImpl on that view).
// - if the view has no right sibling, go up the parents until you find a parent
//   with a right sibling and start the search from there.
View* FocusSearch::FindNextFocusableViewImpl(
    View* starting_view,
    bool check_starting_view,
    bool can_go_up,
    bool can_go_down,
    int skip_group_id,
    FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  if (check_starting_view) {
    if (IsViewFocusableCandidate(starting_view, skip_group_id)) {
      View* v = FindSelectedViewForGroup(starting_view);
      // The selected view might not be focusable (if it is disabled for
      // example).
      if (IsFocusable(v))
        return v;
    }

    *focus_traversable = starting_view->GetFocusTraversable();
    if (*focus_traversable) {
      *focus_traversable_view = starting_view;
      return NULL;
    }
  }

  // First let's try the left child.
  if (can_go_down) {
    if (starting_view->has_children()) {
      View* v = FindNextFocusableViewImpl(starting_view->child_at(0),
                                          true, false, true, skip_group_id,
                                          focus_traversable,
                                          focus_traversable_view);
      if (v || *focus_traversable)
        return v;
    }
  }

  // Then try the right sibling.
  View* sibling = starting_view->GetNextFocusableView();
  if (sibling) {
    View* v = FindNextFocusableViewImpl(sibling,
                                        true, false, true, skip_group_id,
                                        focus_traversable,
                                        focus_traversable_view);
    if (v || *focus_traversable)
      return v;
  }

  // Then go up to the parent sibling.
  if (can_go_up) {
    View* parent = GetParent(starting_view);
    while (parent && parent != root_) {
      sibling = parent->GetNextFocusableView();
      if (sibling) {
        return FindNextFocusableViewImpl(sibling,
                                         true, true, true,
                                         skip_group_id,
                                         focus_traversable,
                                         focus_traversable_view);
      }
      parent = GetParent(parent);
    }
  }

  // We found nothing.
  return NULL;
}

// Strategy for finding the previous focusable view:
// - keep going down on the right until you reach a view with no children, if it
//   it is a good candidate return it.
// - start the search on the left sibling.
// - if there are no left sibling, start the search on the parent (without going
//   down).
View* FocusSearch::FindPreviousFocusableViewImpl(
    View* starting_view,
    bool check_starting_view,
    bool can_go_up,
    bool can_go_down,
    int skip_group_id,
    FocusTraversable** focus_traversable,
    View** focus_traversable_view) {
  // Let's go down and right as much as we can.
  if (can_go_down) {
    // Before we go into the direct children, we have to check if this view has
    // a FocusTraversable.
    *focus_traversable = starting_view->GetFocusTraversable();
    if (*focus_traversable) {
      *focus_traversable_view = starting_view;
      return NULL;
    }

    if (starting_view->has_children()) {
      View* view =
          starting_view->child_at(starting_view->child_count() - 1);
      View* v = FindPreviousFocusableViewImpl(view, true, false, true,
                                              skip_group_id,
                                              focus_traversable,
                                              focus_traversable_view);
      if (v || *focus_traversable)
        return v;
    }
  }

  // Then look at this view. Here, we do not need to see if the view has
  // a FocusTraversable, since we do not want to go down any more.
  if (check_starting_view &&
      IsViewFocusableCandidate(starting_view, skip_group_id)) {
    View* v = FindSelectedViewForGroup(starting_view);
    // The selected view might not be focusable (if it is disabled for
    // example).
    if (IsFocusable(v))
      return v;
  }

  // Then try the left sibling.
  View* sibling = starting_view->GetPreviousFocusableView();
  if (sibling) {
    return FindPreviousFocusableViewImpl(sibling,
                                         true, true, true,
                                         skip_group_id,
                                         focus_traversable,
                                         focus_traversable_view);
  }

  // Then go up the parent.
  if (can_go_up) {
    View* parent = GetParent(starting_view);
    if (parent)
      return FindPreviousFocusableViewImpl(parent,
                                           true, true, false,
                                           skip_group_id,
                                           focus_traversable,
                                           focus_traversable_view);
  }

  // We found nothing.
  return NULL;
}

}  // namespace views
