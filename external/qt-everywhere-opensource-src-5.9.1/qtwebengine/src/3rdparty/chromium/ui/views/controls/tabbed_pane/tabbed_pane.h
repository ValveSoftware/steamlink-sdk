// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_
#define UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace views {

class Label;
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
  ~TabbedPane() override;

  TabbedPaneListener* listener() const { return listener_; }
  void set_listener(TabbedPaneListener* listener) { listener_ = listener; }

  // Returns the index of the currently selected tab, or -1 if no tab is
  // selected.
  int GetSelectedTabIndex() const;

  // Returns the number of tabs.
  int GetTabCount();

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
  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override;

 private:
  friend class FocusTraversalTest;
  friend class Tab;
  friend class TabStrip;
  FRIEND_TEST_ALL_PREFIXES(TabbedPaneTest, AddAndSelect);
  FRIEND_TEST_ALL_PREFIXES(TabbedPaneTest, ArrowKeyBindings);

  // Get the Tab (the tabstrip view, not its content) at the valid |index|.
  Tab* GetTabAt(int index);

  // Get the Tab (the tabstrip view, not its content) at the selected index.
  Tab* GetSelectedTab();

  // Returns the content View of the currently selected Tab.
  View* GetSelectedTabContentView();

  // Moves the selection by |delta| tabs, where negative delta means leftwards
  // and positive delta means rightwards. Returns whether the selection could be
  // moved by that amount; the only way this can fail is if there is only one
  // tab.
  bool MoveSelectionBy(int delta);

  // Overridden from View:
  void Layout() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

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

// The tab view shown in the tab strip.
class Tab : public View {
 public:
  // Internal class name.
  static const char kViewClassName[];

  Tab(TabbedPane* tabbed_pane, const base::string16& title, View* contents);
  ~Tab() override;

  View* contents() const { return contents_; }

  bool selected() const { return contents_->visible(); }
  void SetSelected(bool selected);

  // Overridden from View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 protected:
  Label* title() { return title_; }

  // Called whenever |tab_state_| changes.
  virtual void OnStateChanged();

 private:
  enum TabState {
    TAB_INACTIVE,
    TAB_ACTIVE,
    TAB_HOVERED,
  };

  void SetState(TabState tab_state);

  TabbedPane* tabbed_pane_;
  Label* title_;
  gfx::Size preferred_title_size_;
  TabState tab_state_;
  // The content view associated with this tab.
  View* contents_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TABBED_PANE_TABBED_PANE_H_
