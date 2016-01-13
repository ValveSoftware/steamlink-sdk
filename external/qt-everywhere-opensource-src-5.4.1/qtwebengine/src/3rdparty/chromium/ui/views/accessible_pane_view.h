// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBLE_PANE_VIEW_H_
#define UI_VIEWS_ACCESSIBLE_PANE_VIEW_H_

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class FocusSearch;

// This class provides keyboard access to any view that extends it, typically
// a toolbar.  The user sets focus to a control in this view by pressing
// F6 to traverse all panes, or by pressing a shortcut that jumps directly
// to this pane.
class VIEWS_EXPORT AccessiblePaneView : public View,
                                        public FocusChangeListener,
                                        public FocusTraversable {
 public:
  AccessiblePaneView();
  virtual ~AccessiblePaneView();

  // Set focus to the pane with complete keyboard access.
  // Focus will be restored to the last focused view if the user escapes.
  // If |initial_focus| is not NULL, that control will get
  // the initial focus, if it's enabled and focusable. Returns true if
  // the pane was able to receive focus.
  virtual bool SetPaneFocus(View* initial_focus);

  // Set focus to the pane with complete keyboard access, with the
  // focus initially set to the default child. Focus will be restored
  // to the last focused view if the user escapes.
  // Returns true if the pane was able to receive focus.
  virtual bool SetPaneFocusAndFocusDefault();

  // Overridden from View:
  virtual FocusTraversable* GetPaneFocusTraversable() OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator)
      OVERRIDE;
  virtual void SetVisible(bool flag) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void RequestFocus() OVERRIDE;

  // Overridden from FocusChangeListener:
  virtual void OnWillChangeFocus(View* focused_before,
                                 View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before,
                                View* focused_now) OVERRIDE;

  // Overridden from FocusTraversable:
  virtual FocusSearch* GetFocusSearch() OVERRIDE;
  virtual FocusTraversable* GetFocusTraversableParent() OVERRIDE;
  virtual View* GetFocusTraversableParentView() OVERRIDE;

  // For testing only.
  const ui::Accelerator& home_key() const { return home_key_; }
  const ui::Accelerator& end_key() const { return end_key_; }
  const ui::Accelerator& escape_key() const { return escape_key_; }
  const ui::Accelerator& left_key() const { return left_key_; }
  const ui::Accelerator& right_key() const { return right_key_; }

 protected:
  // A subclass can override this to provide a default focusable child
  // other than the first focusable child.
  virtual View* GetDefaultFocusableChild();

  // Returns the parent of |v|. Subclasses can override this if
  // they need custom focus search behavior.
  virtual View* GetParentForFocusSearch(View* v);

  // Returns true if |v| is contained within the hierarchy rooted at |root|
  // for the purpose of focus searching. Subclasses can override this if
  // they need custom focus search behavior.
  virtual bool ContainsForFocusSearch(View* root, const View* v);

  // Remove pane focus.
  virtual void RemovePaneFocus();

  View* GetFirstFocusableChild();
  View* GetLastFocusableChild();

  FocusManager* focus_manager() const { return focus_manager_; }

  // When finishing navigation by pressing ESC, it is allowed to surrender the
  // focus to another window if if |allow| is set and no previous view can be
  // found.
  void set_allow_deactivate_on_esc(bool allow) {
    allow_deactivate_on_esc_ = allow;
  }

 private:
  bool pane_has_focus_;

  // If true, the panel should be de-activated upon escape when no active view
  // is known where to return to.
  bool allow_deactivate_on_esc_;

  base::WeakPtrFactory<AccessiblePaneView> method_factory_;

  // Save the focus manager rather than calling GetFocusManager(),
  // so that we can remove focus listeners in the destructor.
  FocusManager* focus_manager_;

  // Our custom focus search implementation that traps focus in this
  // pane and traverses all views that are focusable for accessibility,
  // not just those that are normally focusable.
  scoped_ptr<FocusSearch> focus_search_;

  // Registered accelerators
  ui::Accelerator home_key_;
  ui::Accelerator end_key_;
  ui::Accelerator escape_key_;
  ui::Accelerator left_key_;
  ui::Accelerator right_key_;

  // View storage id for the last focused view that's not within this pane.
  int last_focused_view_storage_id_;

  friend class AccessiblePaneViewFocusSearch;

  DISALLOW_COPY_AND_ASSIGN(AccessiblePaneView);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBLE_PANE_VIEW_H_
