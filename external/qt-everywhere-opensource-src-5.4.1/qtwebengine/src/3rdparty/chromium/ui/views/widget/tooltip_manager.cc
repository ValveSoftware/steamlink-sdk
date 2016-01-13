// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/tooltip_manager.h"

#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/text_elider.h"

namespace views {

const size_t kMaxTooltipLength = 1024;

// static
const char TooltipManager::kGroupingPropertyKey[] = "GroupingPropertyKey";

// static
int TooltipManager::GetMaxWidth(int x, int y, gfx::NativeView context) {
  return GetMaxWidth(gfx::Screen::GetScreenFor(context)->GetDisplayNearestPoint(
                         gfx::Point(x, y)));
}

// static
int TooltipManager::GetMaxWidth(const gfx::Display& display) {
  return (display.bounds().width() + 1) / 2;
}

// static
void TooltipManager::TrimTooltipText(base::string16* text) {
  // Clamp the tooltip length to kMaxTooltipLength so that we don't
  // accidentally DOS the user with a mega tooltip.
  *text = gfx::TruncateString(*text, kMaxTooltipLength);
}

}  // namespace views
