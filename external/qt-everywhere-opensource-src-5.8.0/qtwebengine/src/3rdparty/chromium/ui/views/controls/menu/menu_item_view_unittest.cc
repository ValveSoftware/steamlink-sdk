// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_item_view.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/view.h"

namespace {

// A simple View class that will match its height to the available width.
class SquareView : public views::View {
 public:
  SquareView() {}
  ~SquareView() override {}

 private:
  gfx::Size GetPreferredSize() const override { return gfx::Size(1, 1); }
  int GetHeightForWidth(int width) const override { return width; }
};

// A MenuItemView implementation with a public destructor (so we can clean up
// in tests).
class TestMenuItemView : public views::MenuItemView {
 public:
  TestMenuItemView() : views::MenuItemView(NULL) {}
  ~TestMenuItemView() override {}
};

}  // namespace

TEST(MenuItemViewUnitTest, TestMenuItemViewWithFlexibleWidthChild) {
  TestMenuItemView root_menu;
  root_menu.set_owned_by_client();

  // Append a normal MenuItemView.
  views::MenuItemView* label_view =
      root_menu.AppendMenuItemWithLabel(1, base::ASCIIToUTF16("item 1"));

  // Append a second MenuItemView that has a child SquareView.
  views::MenuItemView* flexible_view =
      root_menu.AppendMenuItemWithLabel(2, base::string16());
  flexible_view->AddChildView(new SquareView());
  // Set margins to 0 so that we know width should match height.
  flexible_view->SetMargins(0, 0);

  views::SubmenuView* submenu = root_menu.GetSubmenu();

  // The first item should be the label view.
  ASSERT_EQ(label_view, submenu->GetMenuItemAt(0));
  gfx::Size label_size = label_view->GetPreferredSize();

  // The second item should be the flexible view.
  ASSERT_EQ(flexible_view, submenu->GetMenuItemAt(1));
  gfx::Size flexible_size = flexible_view->GetPreferredSize();

  // The flexible view's "preferred size" should be 1x1...
  EXPECT_EQ(flexible_size, gfx::Size(1, 1));

  // ...but it should use whatever space is available to make a square.
  int flex_height = flexible_view->GetHeightForWidth(label_size.width());
  EXPECT_EQ(label_size.width(), flex_height);

  // The submenu should be tall enough to allow for both menu items at the given
  // width.
  EXPECT_EQ(label_size.height() + flex_height,
            submenu->GetPreferredSize().height());
}
