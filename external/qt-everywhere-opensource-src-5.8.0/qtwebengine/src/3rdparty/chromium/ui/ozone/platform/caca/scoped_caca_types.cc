// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/caca/scoped_caca_types.h"

#include <caca.h>

namespace ui {

void CacaCanvasDeleter::operator()(caca_canvas_t* canvas) const {
  caca_free_canvas(canvas);
}

void CacaDisplayDeleter::operator()(caca_display_t* display) const {
  caca_free_display(display);
}

void CacaDitherDeleter::operator()(caca_dither_t* dither) const {
  caca_free_dither(dither);
}

}  // namespace ui
