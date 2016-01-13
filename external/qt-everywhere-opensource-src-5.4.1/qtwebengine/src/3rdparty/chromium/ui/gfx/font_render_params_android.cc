// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_render_params_linux.h"

namespace gfx {

namespace {

// Initializes |params| with the system's default settings.
void LoadDefaults(FontRenderParams* params) {
  params->antialiasing = true;
  params->autohinter = true;
  params->use_bitmaps = true;
  params->subpixel_rendering = FontRenderParams::SUBPIXEL_RENDERING_NONE;

  // Use subpixel text positioning to keep consistent character spacing when
  // the page is scaled by a fractional factor.
  params->subpixel_positioning = true;
  // Slight hinting renders much better than normal hinting on Android.
  params->hinting = FontRenderParams::HINTING_SLIGHT;
}

}  // namespace

const FontRenderParams& GetDefaultFontRenderParams() {
  static bool loaded_defaults = false;
  static FontRenderParams default_params;
  if (!loaded_defaults)
    LoadDefaults(&default_params);
  loaded_defaults = true;
  return default_params;
}

const FontRenderParams& GetDefaultWebKitFontRenderParams() {
  return GetDefaultFontRenderParams();
}

}  // namespace gfx
