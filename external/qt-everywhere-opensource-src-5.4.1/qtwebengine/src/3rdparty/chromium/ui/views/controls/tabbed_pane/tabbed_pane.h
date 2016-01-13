// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace views {

class Tab;
class TabbedPaneListener;
class TabStrip;

// TabbedPane is a view that shows tabs. When the user clicks on a tab, the
// associated view is displayed.
class VIEWS_EXPORT TabbedPane : public View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  TabbedPane();
  virtual ~TabbedPane();

  TabbedPaneListener* listener() const { return listener_; }
  void set_listener(TabbedPaneListener* listener) { listener_ = listener; }

  int selected_tab_index() const { return selected_tab_index_; }

  // Returns the number of tabs.
  int GetTabCount();

  // Returns the contents of the selected tab or NULL if there is none.
  View* GetSelectedTab();

  // Adds a new tab at the end of this TabbedPane with the specified |title|.
  // |contents| is the view displayed when the tab is selected and is owned by
  // the TabbedPane.
  void AddTab(const base::string16& title, View* contents);

  // Adds a new tab at |index| with |title|. |contents| is the view displayed
  // when the tab is selected and is owned by the TabbedPane. If the tabbed pane
  // is currently empty, the new tab is selected.
  void AddTabAtIndex(int index, const base::string16& title, View* contents);

  // Selects the tab at |index|, which must be valid.
  void SelectTabAt(int index);

  // Selects |tab| (the tabstrip view, not its content) if it is valid.
  void SelectTab(Tab* tab);

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

 private:
   friend class TabStrip;

   // Get the Tab (the tabstrip view, not its content) at the valid |index|.
   Tab* GetTabAt(int index);

  // Overridden from View:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;

  // A listener notified when tab selection changes. Weak, not owned.
  TabbedPaneListener* listener_;

  // The tab strip and contents container. The child indices of these members
  // correspond to match each Tab with its respective content View.
  TabStrip* tab_strip_;
  View* contents_;

  // The selected tab index or -1 if invalid.
  int selected_tab_index_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPane);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_
