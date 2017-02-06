// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_ROW_BACKGROUND_PAINTER_H_
#define UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_ROW_BACKGROUND_PAINTER_H_

#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Rect;
}

namespace views {

// TableViewRowBackgroundPainter is used to paint the background of a row in the
// table.
class VIEWS_EXPORT TableViewRowBackgroundPainter {
 public:
  virtual ~TableViewRowBackgroundPainter() {}
  virtual void PaintRowBackground(int model_index,
                                  const gfx::Rect& row_bounds,
                                  gfx::Canvas* canvas) = 0;
};

}

#endif  // UI_VIEWS_CONTROLS_TABLE_TABLE_VIEW_ROW_BACKGROUND_PAINTER_H_
