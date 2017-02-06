// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CACA_SCOPED_CACA_TYPES_H_
#define UI_OZONE_PLATFORM_CACA_SCOPED_CACA_TYPES_H_

#include <memory>

typedef struct caca_canvas caca_canvas_t;
typedef struct caca_dither caca_dither_t;
typedef struct caca_display caca_display_t;

namespace ui {

struct CacaCanvasDeleter {
  void operator()(caca_canvas_t* canvas) const;
};

struct CacaDisplayDeleter {
  void operator()(caca_display_t* display) const;
};

struct CacaDitherDeleter {
  void operator()(caca_dither_t* dither) const;
};

typedef std::unique_ptr<caca_canvas_t, CacaCanvasDeleter> ScopedCacaCanvas;
typedef std::unique_ptr<caca_display_t, CacaDisplayDeleter> ScopedCacaDisplay;
typedef std::unique_ptr<caca_dither_t, CacaDitherDeleter> ScopedCacaDither;

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CACA_SCOPED_CACA_TYPES_H_
