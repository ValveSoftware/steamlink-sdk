// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/progress_bar.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"

namespace views {

TEST(ProgressBarTest, Accessibility) {
  ProgressBar bar;
  bar.SetValue(0.62);

  ui::AXNodeData node_data;
  bar.GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ui::AX_ROLE_PROGRESS_INDICATOR, node_data.role);
  EXPECT_EQ(base::string16(), node_data.GetString16Attribute(ui::AX_ATTR_NAME));
  EXPECT_TRUE(node_data.HasStateFlag(ui::AX_STATE_READ_ONLY));
}

}  // namespace views
