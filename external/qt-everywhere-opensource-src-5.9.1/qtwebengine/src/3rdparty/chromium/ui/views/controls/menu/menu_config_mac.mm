// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_config.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"

namespace views {

namespace {

// The height of the menu separator in pixels.
const int kMenuSeparatorHeight = 2;
const int kMenuSeparatorHeightMavericks = 1;

}  // namespace

void MenuConfig::Init() {
  font_list = gfx::FontList(gfx::Font([NSFont menuFontOfSize:0.0]));
  menu_vertical_border_size = 4;
  item_top_margin = item_no_icon_top_margin = 1;
  item_bottom_margin = item_no_icon_bottom_margin = 1;
  item_left_margin = 2;
  arrow_to_edge_padding = 13;
  icon_to_label_padding = 4;
  check_width = 19;
  check_height = 11;
  separator_height = 13;
  separator_upper_height = 7;
  separator_lower_height = 6;
  separator_thickness = base::mac::IsOS10_9() ? kMenuSeparatorHeightMavericks
                                              : kMenuSeparatorHeight;
  align_arrow_and_shortcut = true;
  icons_in_label = true;
  check_selected_combobox_item = true;
  corner_radius = 5;
  use_outer_border = false;
}

}  // namespace views
