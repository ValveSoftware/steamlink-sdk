// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/window_shape.h"

#include "ui/gfx/path.h"
#include "ui/gfx/size.h"

namespace views {

void GetDefaultWindowMask(const gfx::Size &size, gfx::Path *window_mask) {
  // Redefine the window visible region for the new size.
  window_mask->moveTo(0, 3);
  window_mask->lineTo(1, 2);
  window_mask->lineTo(1, 1);
  window_mask->lineTo(2, 1);
  window_mask->lineTo(3, 0);

  window_mask->lineTo(SkIntToScalar(size.width() - 3), 0);
  window_mask->lineTo(SkIntToScalar(size.width() - 2), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 1);
  window_mask->lineTo(SkIntToScalar(size.width() - 1), 2);
  window_mask->lineTo(SkIntToScalar(size.width()), 3);

  window_mask->lineTo(SkIntToScalar(size.width()),
                      SkIntToScalar(size.height() - 3));
  window_mask->lineTo(SkIntToScalar(size.width() - 1),
                      SkIntToScalar(size.height() - 3));
  window_mask->lineTo(SkIntToScalar(size.width() - 1),
                      SkIntToScalar(size.height() - 1));
  window_mask->lineTo(SkIntToScalar(size.width() - 3),
                      SkIntToScalar(size.height() - 2));
  window_mask->lineTo(SkIntToScalar(size.width() - 3),
                      SkIntToScalar(size.height()));

  window_mask->lineTo(3, SkIntToScalar(size.height()));
  window_mask->lineTo(2, SkIntToScalar(size.height() - 2));
  window_mask->lineTo(1, SkIntToScalar(size.height() - 1));
  window_mask->lineTo(1, SkIntToScalar(size.height() - 3));
  window_mask->lineTo(0, SkIntToScalar(size.height() - 3));

  window_mask->close();
}

} // namespace views
