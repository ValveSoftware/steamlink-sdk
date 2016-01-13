// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/menu_item_builder.h"

#include "content/public/common/menu_item.h"

namespace content {

MenuItem MenuItemBuilder::Build(const blink::WebMenuItemInfo& item) {
  MenuItem result;

  result.label = item.label;
  result.tool_tip = item.toolTip;
  result.type = static_cast<MenuItem::Type>(item.type);
  result.action = item.action;
  result.rtl = (item.textDirection == blink::WebTextDirectionRightToLeft);
  result.has_directional_override = item.hasTextDirectionOverride;
  result.enabled = item.enabled;
  result.checked = item.checked;
  for (size_t i = 0; i < item.subMenuItems.size(); ++i)
    result.submenu.push_back(MenuItemBuilder::Build(item.subMenuItems[i]));

  return result;
}

}  // namespace content
