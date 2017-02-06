// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/separator.h"

#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"

namespace views {

// static
const char Separator::kViewClassName[] = "Separator";

// The separator size in pixels.
const int kSeparatorSize = 1;

// Default color of the separator.
const SkColor kDefaultColor = SkColorSetARGB(255, 233, 233, 233);

Separator::Separator(Orientation orientation)
    : orientation_(orientation),
      color_(kDefaultColor),
      size_(kSeparatorSize) {
}

Separator::~Separator() {
}

void Separator::SetColor(SkColor color) {
  color_ = color;
  SchedulePaint();
}

void Separator::SetPreferredSize(int size) {
  if (size != size_) {
    size_ = size;
    PreferredSizeChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Separator, View overrides:

gfx::Size Separator::GetPreferredSize() const {
  gfx::Size size =
      orientation_ == HORIZONTAL ? gfx::Size(1, size_) : gfx::Size(size_, 1);
  gfx::Insets insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  return size;
}

void Separator::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_SPLITTER;
}

void Separator::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRect(GetContentsBounds(), color_);
}

const char* Separator::GetClassName() const {
  return kViewClassName;
}

}  // namespace views
