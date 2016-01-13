// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/caca_connection.h"

#include "base/logging.h"

namespace ui {

// TODO(dnicoara) As an optimization, |bitmap_size_| should be scaled based on
// |physical_size_| and font size.
CacaConnection::CacaConnection()
    : canvas_(NULL),
      display_(NULL),
      physical_size_(160, 48),
      bitmap_size_(800, 600) {}

CacaConnection::~CacaConnection() {
  if (display_) {
    caca_free_display(display_);
    caca_free_canvas(canvas_);
  }
}

void CacaConnection::Initialize() {
  if (display_)
    return;

  canvas_ = caca_create_canvas(physical_size_.width(), physical_size_.height());
  display_ = caca_create_display(canvas_);

  physical_size_.SetSize(caca_get_canvas_width(canvas_),
                         caca_get_canvas_height(canvas_));

  caca_set_cursor(display_, 1);
  caca_set_display_title(display_, "Ozone Content Shell");
}

}  // namespace ui
