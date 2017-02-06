// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/views_test_base.h"

using base::ASCIIToUTF16;

namespace views {

namespace {

// A view for testing that takes a fixed preferred size upon construction.
class FixedSizeView : public View {
 public:
  explicit FixedSizeView(const gfx::Size& size)
    : size_(size) {}

  // Overridden from View:
  gfx::Size GetPreferredSize() const override { return size_; }

 private:
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizeView);
};

typedef ViewsTestBase TabbedPaneTest;

// Tests TabbedPane::GetPreferredSize() and TabbedPane::Layout().
TEST_F(TabbedPaneTest, SizeAndLayout) {
  std::unique_ptr<TabbedPane> tabbed_pane(new TabbedPane());
  View* child1 = new FixedSizeView(gfx::Size(20, 10));
  tabbed_pane->AddTab(ASCIIToUTF16("tab1"), child1);
  View* child2 = new FixedSizeView(gfx::Size(5, 5));
  tabbed_pane->AddTab(ASCIIToUTF16("tab2"), child2);
  tabbed_pane->SelectTabAt(0);

  // The |tabbed_pane| implementation of Views has no border by default.
  // Therefore it should be as wide as the widest tab. The native Windows
  // tabbed pane has a border that used up extra space. Therefore the preferred
  // width is larger than the largest child.
  gfx::Size pref(tabbed_pane->GetPreferredSize());
  EXPECT_GE(pref.width(), 20);
  EXPECT_GT(pref.height(), 10);

  // The bounds of our children should be smaller than the tabbed pane's bounds.
  tabbed_pane->SetBounds(0, 0, 100, 200);
  RunPendingMessages();
  gfx::Rect bounds(child1->bounds());
  EXPECT_GT(bounds.width(), 0);
  // The |tabbed_pane| has no border. Therefore the children should be as wide
  // as the |tabbed_pane|.
  EXPECT_LE(bounds.width(), 100);
  EXPECT_GT(bounds.height(), 0);
  EXPECT_LT(bounds.height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane->SelectTabAt(1);
  EXPECT_EQ(bounds, child2->bounds());
}

TEST_F(TabbedPaneTest, AddAndSelect) {
  std::unique_ptr<TabbedPane> tabbed_pane(new TabbedPane());
  // Add several tabs; only the first should be a selected automatically.
  for (int i = 0; i < 3; ++i) {
    View* tab = new View();
    tabbed_pane->AddTab(ASCIIToUTF16("tab"), tab);
    EXPECT_EQ(i + 1, tabbed_pane->GetTabCount());
    EXPECT_EQ(0, tabbed_pane->selected_tab_index());
  }

  // Select each tab.
  for (int i = 0; i < tabbed_pane->GetTabCount(); ++i) {
    tabbed_pane->SelectTabAt(i);
    EXPECT_EQ(i, tabbed_pane->selected_tab_index());
  }

  // Add a tab at index 0, it should not be selected automatically.
  View* tab0 = new View();
  tabbed_pane->AddTabAtIndex(0, ASCIIToUTF16("tab0"), tab0);
  EXPECT_NE(tab0, tabbed_pane->GetSelectedTab());
  EXPECT_NE(0, tabbed_pane->selected_tab_index());
}

}  // namespace

}  // namespace views
