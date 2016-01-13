// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_CACA_CONNECTION_H_
#define UI_OZONE_PLATFORM_CACA_CACA_CONNECTION_H_

#include <caca.h>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

// Basic initialization of Libcaca. This needs to be shared between SFO and EFO
// since both need the |display_| to draw and process events respectively.
// Note, |canvas_| needs to be here since it is needed for creating a
// |display_|.
class CacaConnection {
 public:
  CacaConnection();
  ~CacaConnection();

  void Initialize();

  // This is the Caca canvas size.
  gfx::Size physical_size() const { return physical_size_; }
  gfx::Size bitmap_size() const { return bitmap_size_; }
  caca_display_t* display() const { return display_; }

 private:
  caca_canvas_t* canvas_;
  caca_display_t* display_;

  // Size of the console in characters. This can be changed by setting
  // CACA_GEOMETRY environment variable.
  gfx::Size physical_size_;

  // Size of the backing buffer we draw into. Size in pixels.
  gfx::Size bitmap_size_;

  DISALLOW_COPY_AND_ASSIGN(CacaConnection);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_CACA_CONNECTION_H_
