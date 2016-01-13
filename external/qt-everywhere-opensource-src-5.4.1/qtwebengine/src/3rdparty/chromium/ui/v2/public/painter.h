// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_V2_PUBLIC_PAINTER_H_
#define UI_V2_PUBLIC_PAINTER_H_

#include "ui/v2/public/v2_export.h"

namespace gfx {
class Canvas;
}

namespace v2 {

class V2_EXPORT Painter {
 public:
  virtual ~Painter() {}

  virtual void Paint(gfx::Canvas* canvas) = 0;
};

}  // namespace v2

#endif  // UI_V2_PUBLIC_PAINTER_H_
