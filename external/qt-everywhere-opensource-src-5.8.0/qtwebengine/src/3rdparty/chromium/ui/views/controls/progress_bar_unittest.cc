// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_view_state.h"

namespace views {

TEST(ProgressBarTest, TooltipTextProperty) {
  ProgressBar bar;
  base::string16 tooltip = base::ASCIIToUTF16("Some text");
  EXPECT_FALSE(bar.GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(base::string16(), tooltip);
  base::string16 tooltip_text = base::ASCIIToUTF16("My progress");
  bar.SetTooltipText(tooltip_text);
  EXPECT_TRUE(bar.GetTooltipText(gfx::Point(), &tooltip));
  EXPECT_EQ(tooltip_text, tooltip);
}

TEST(ProgressBarTest, Accessibility) {
  ProgressBar bar;
  bar.SetValue(62);

  ui::AXViewState state;
  bar.GetAccessibleState(&state);
  EXPECT_EQ(ui::AX_ROLE_PROGRESS_INDICATOR, state.role);
  EXPECT_EQ(base::string16(), state.name);
  EXPECT_TRUE(state.HasStateFlag(ui::AX_STATE_READ_ONLY));
}

}  // namespace views
