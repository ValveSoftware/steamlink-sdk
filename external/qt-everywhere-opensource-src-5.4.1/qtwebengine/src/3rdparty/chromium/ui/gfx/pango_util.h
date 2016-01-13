// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PANGO_UTIL_H_
#define UI_GFX_PANGO_UTIL_H_

#include <cairo/cairo.h>
#include <pango/pango.h>
#include <string>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

typedef struct _PangoContext PangoContext;

namespace gfx {

class Font;
class PlatformFontPango;
class Rect;

// Creates and returns a PangoContext. The caller owns the context.
PangoContext* GetPangoContext();

// Returns the resolution (DPI) used by pango. A negative values means the
// resolution hasn't been set.
double GetPangoResolution();

// Utility class to ensure that PangoFontDescription is freed.
class ScopedPangoFontDescription {
 public:
  explicit ScopedPangoFontDescription(PangoFontDescription* description)
      : description_(description) {
    DCHECK(description);
  }

  ~ScopedPangoFontDescription() {
    pango_font_description_free(description_);
  }

  PangoFontDescription* get() { return description_; }

 private:
  PangoFontDescription* description_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPangoFontDescription);
};

// ----------------------------------------------------------------------------
// All other methods in this file are only to be used within the ui/ directory.
// They are shared with internal skia interfaces.
// ----------------------------------------------------------------------------

// Setup pango |layout|; set the |text|, the font description based on
// |font_description|, the |width| in PANGO_SCALE for RTL locale, the base
// |text_direction|, alignment, ellipsis, word wrapping, resolution, etc.
void SetupPangoLayoutWithFontDescription(
    PangoLayout* layout,
    const base::string16& text,
    const std::string& font_description,
    int width,
    base::i18n::TextDirection text_direction,
    int flags);

// Returns the size in pixels for the specified |pango_font|.
size_t GetPangoFontSizeInPixels(PangoFontDescription* pango_font);

// Retrieves the Pango metrics for a Pango font description. Caches the metrics
// and never frees them. The metrics objects are relatively small and very
// expensive to look up.
PangoFontMetrics* GetPangoFontMetrics(PangoFontDescription* desc);

}  // namespace gfx

#endif  // UI_GFX_PANGO_UTIL_H_
