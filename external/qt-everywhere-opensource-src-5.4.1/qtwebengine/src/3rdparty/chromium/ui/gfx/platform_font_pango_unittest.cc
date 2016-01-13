// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/platform_font_pango.h"

#include <cairo/cairo.h>
#include <fontconfig/fontconfig.h>
#include <glib-object.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>

#include <string>

#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/pango_util.h"

namespace gfx {

// Test that PlatformFontPango is able to cope with PangoFontDescriptions
// containing multiple font families.  The first family should be preferred.
TEST(PlatformFontPangoTest, FamilyList) {
  // Needed for GLib versions prior to 2.36, but deprecated starting 2.35.
#if !GLIB_CHECK_VERSION(2, 35, 0)
  g_type_init();
#endif

  ScopedPangoFontDescription desc(
      pango_font_description_from_string("Arial,Times New Roman, 13px"));
  scoped_refptr<gfx::PlatformFontPango> font(
      new gfx::PlatformFontPango(desc.get()));
  EXPECT_EQ("Arial", font->GetFontName());
  EXPECT_EQ(13, font->GetFontSize());

  ScopedPangoFontDescription desc2(
      pango_font_description_from_string("Times New Roman,Arial, 15px"));
  scoped_refptr<gfx::PlatformFontPango> font2(
      new gfx::PlatformFontPango(desc2.get()));
  EXPECT_EQ("Times New Roman", font2->GetFontName());
  EXPECT_EQ(15, font2->GetFontSize());

  // Free memory allocated by FontConfig (http://crbug.com/114750).
  pango_fc_font_map_cache_clear(
      PANGO_FC_FONT_MAP(pango_cairo_font_map_get_default()));
  cairo_debug_reset_static_data();
  FcFini();
}

}  // namespace gfx
