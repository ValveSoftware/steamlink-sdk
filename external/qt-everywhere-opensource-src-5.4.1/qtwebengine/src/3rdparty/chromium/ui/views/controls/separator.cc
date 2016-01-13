// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"

namespace views {

// static
const char Separator::kViewClassName[] = "Separator";

// The separator height in pixels.
const int kSeparatorHeight = 1;

// Default color of the separator.
const SkColor kDefaultColor = SkColorSetARGB(255, 233, 233, 233);

Separator::Separator(Orientation orientation) : orientation_(orientation) {
  SetFocusable(false);
}

Separator::~Separator() {
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::GetPreferredSize() const {
  if (orientation_ == HORIZONTAL)
    return gfx::Size(width(), kSeparatorHeight);
  return gfx::Size(kSeparatorHeight, height());
}

void Separator::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_SPLITTER;
}

void Separator::Paint(gfx::Canvas* canvas, const views::CullSet& cull_set) {
  canvas->FillRect(bounds(), kDefaultColor);
}

const char* Separator::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
