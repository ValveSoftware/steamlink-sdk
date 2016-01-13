// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/views/views_export.h"

namespace ui {
class NativeTheme;
}

namespace views {

// Layout type information for menu items. Use the instance() method to obtain
// the MenuConfig for the current platform.
struct VIEWS_EXPORT MenuConfig {
  explicit MenuConfig(const ui::NativeTheme* theme);
  ~MenuConfig();

  static const MenuConfig& instance(const ui::NativeTheme* theme);

  // Font list used by menus.
  gfx::FontList font_list;

  // Color for the arrow to scroll bookmarks.
  SkColor arrow_color;

  // Menu border sizes.
  int menu_vertical_border_size;
  int menu_horizontal_border_size;

  // Submenu horizontal inset with parent menu. This is the horizontal overlap
  // between the submenu and its parent menu, not including the borders of
  // submenu and parent menu.
  int submenu_horizontal_inset;

  // Margins between the top of the item and the label.
  int item_top_margin;

  // Margins between the bottom of the item and the label.
  int item_bottom_margin;

  // Margins used if the menu doesn't have icons.
  int item_no_icon_top_margin;
  int item_no_icon_bottom_margin;

  // Margins between the left of the item and the icon.
  int item_left_margin;

  // Padding between the label and submenu arrow.
  int label_to_arrow_padding;

  // Padding between the arrow and the edge.
  int arrow_to_edge_padding;

  // Padding between the icon and label.
  int icon_to_label_padding;

  // Padding between the gutter and label.
  int gutter_to_label;

  // Size of the check.
  int check_width;
  int check_height;

  // Width of the radio bullet.
  int radio_width;

  // Width of the submenu arrow.
  int arrow_width;

  // Width of the gutter. Only used if render_gutter is true.
  int gutter_width;

  // Height of a normal separator (ui::NORMAL_SEPARATOR).
  int separator_height;

  // Height of a ui::UPPER_SEPARATOR.
  int separator_upper_height;

  // Height of a ui::LOWER_SEPARATOR.
  int separator_lower_height;

  // Height of a ui::SPACING_SEPARATOR.
  int separator_spacing_height;

  // Whether or not the gutter should be rendered. The gutter is specific to
  // Vista.
  bool render_gutter;

  // Are mnemonics shown?
  bool show_mnemonics;

  // Height of the scroll arrow.
  int scroll_arrow_height;

  // Padding between the label and minor text. Only used if there is an
  // accelerator or sublabel.
  int label_to_minor_text_padding;

  // Minimum height of menu item.
  int item_min_height;

  // Whether the keyboard accelerators are visible.
  bool show_accelerators;

  // True if icon to label padding is always added with or without icon.
  bool always_use_icon_to_label_padding;

  // True if submenu arrow and shortcut right edge should be aligned.
  bool align_arrow_and_shortcut;

  // True if the context menu's should be offset from the cursor position.
  bool offset_context_menus;

  const ui::NativeTheme* native_theme;

  // Delay, in ms, between when menus are selected or moused over and the menu
  // appears.
  int show_delay;

  // Radius of the rounded corners of the menu border. Must be >= 0.
  int corner_radius;

 private:
  // Configures a MenuConfig as appropriate for the current platform.
  void Init(const ui::NativeTheme* theme);

  // TODO: temporary until we standardize.
  void InitAura(const ui::NativeTheme* theme);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_CONFIG_H_
