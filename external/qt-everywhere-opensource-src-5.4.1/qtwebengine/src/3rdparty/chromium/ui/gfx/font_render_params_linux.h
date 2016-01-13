// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_RENDER_PARAMS_LINUX_H_
#define UI_GFX_FONT_RENDER_PARAMS_LINUX_H_

#include "ui/gfx/gfx_export.h"

namespace gfx {

// A collection of parameters describing how text should be rendered on Linux.
struct GFX_EXPORT FontRenderParams {
  // No constructor to avoid static initialization.

  // Level of hinting to be applied.
  enum Hinting {
    HINTING_NONE = 0,
    HINTING_SLIGHT,
    HINTING_MEDIUM,
    HINTING_FULL,
  };

  // Different subpixel orders to be used for subpixel rendering.
  enum SubpixelRendering {
    SUBPIXEL_RENDERING_NONE = 0,
    SUBPIXEL_RENDERING_RGB,
    SUBPIXEL_RENDERING_BGR,
    SUBPIXEL_RENDERING_VRGB,
    SUBPIXEL_RENDERING_VBGR,
  };

  // Antialiasing (grayscale if |subpixel_rendering| is SUBPIXEL_RENDERING_NONE
  // and RGBA otherwise).
  bool antialiasing;

  // Should subpixel positioning (i.e. fractional X positions for glyphs) be
  // used?
  bool subpixel_positioning;

  // Should FreeType's autohinter be used (as opposed to Freetype's bytecode
  // interpreter, which uses fonts' own hinting instructions)?
  bool autohinter;

  // Should embedded bitmaps in fonts should be used?
  bool use_bitmaps;

  // Hinting level.
  Hinting hinting;

  // Whether subpixel rendering should be used or not, and if so, the display's
  // subpixel order.
  SubpixelRendering subpixel_rendering;
};

// Returns the system's default parameters for font rendering.
GFX_EXPORT const FontRenderParams& GetDefaultFontRenderParams();

// Returns the system's default parameters for WebKit font rendering.
GFX_EXPORT const FontRenderParams& GetDefaultWebKitFontRenderParams();

// Returns the system's default parameters for WebKit subpixel positioning.
// Subpixel positioning is special since neither GTK nor FontConfig currently
// track it as a preference.
// See https://bugs.freedesktop.org/show_bug.cgi?id=50736
GFX_EXPORT bool GetDefaultWebkitSubpixelPositioning();

}  // namespace gfx

#endif  // UI_GFX_FONT_RENDER_PARAMS_LINUX_H_
