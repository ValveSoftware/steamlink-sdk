// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/pango_util.h"

#include <cairo/cairo.h>
#include <fontconfig/fontconfig.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>
#include <string>

#include <algorithm>
#include <map>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/platform_font_pango.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/text_utils.h"

namespace gfx {

namespace {

// Marker for accelerators in the text.
const gunichar kAcceleratorChar = '&';

// Return |cairo_font_options|. If needed, allocate and update it.
// TODO(derat): Return font-specific options: http://crbug.com/125235
cairo_font_options_t* GetCairoFontOptions() {
  // Font settings that we initialize once and then use when drawing text.
  static cairo_font_options_t* cairo_font_options = NULL;
  if (cairo_font_options)
    return cairo_font_options;

  cairo_font_options = cairo_font_options_create();

  const FontRenderParams& params = GetDefaultFontRenderParams();
  FontRenderParams::SubpixelRendering subpixel = params.subpixel_rendering;
  if (!params.antialiasing) {
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_NONE);
  } else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_NONE) {
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_GRAY);
  } else {
    cairo_font_options_set_antialias(cairo_font_options,
                                     CAIRO_ANTIALIAS_SUBPIXEL);
    cairo_subpixel_order_t cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_RGB)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
    else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_BGR)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
    else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_VRGB)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
    else if (subpixel == FontRenderParams::SUBPIXEL_RENDERING_VBGR)
      cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
    else
      NOTREACHED() << "Unhandled subpixel rendering type " << subpixel;
    cairo_font_options_set_subpixel_order(cairo_font_options,
                                          cairo_subpixel_order);
  }

  if (params.hinting == FontRenderParams::HINTING_NONE ||
      params.subpixel_positioning) {
    cairo_font_options_set_hint_style(cairo_font_options,
                                      CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(cairo_font_options,
                                        CAIRO_HINT_METRICS_OFF);
  } else {
    cairo_hint_style_t cairo_hint_style = CAIRO_HINT_STYLE_DEFAULT;
    if (params.hinting == FontRenderParams::HINTING_SLIGHT)
      cairo_hint_style = CAIRO_HINT_STYLE_SLIGHT;
    else if (params.hinting == FontRenderParams::HINTING_MEDIUM)
      cairo_hint_style = CAIRO_HINT_STYLE_MEDIUM;
    else if (params.hinting == FontRenderParams::HINTING_FULL)
      cairo_hint_style = CAIRO_HINT_STYLE_FULL;
    else
      NOTREACHED() << "Unhandled hinting style " << params.hinting;
    cairo_font_options_set_hint_style(cairo_font_options, cairo_hint_style);
    cairo_font_options_set_hint_metrics(cairo_font_options,
                                        CAIRO_HINT_METRICS_ON);
  }

  return cairo_font_options;
}

// Returns the number of pixels in a point.
// - multiply a point size by this to get pixels ("device units")
// - divide a pixel size by this to get points
float GetPixelsInPoint() {
  static float pixels_in_point = 1.0;
  static bool determined_value = false;

  if (!determined_value) {
    // http://goo.gl/UIh5m: "This is a scale factor between points specified in
    // a PangoFontDescription and Cairo units.  The default value is 96, meaning
    // that a 10 point font will be 13 units high. (10 * 96. / 72. = 13.3)."
    double pango_dpi = GetPangoResolution();
    if (pango_dpi <= 0)
      pango_dpi = 96.0;
    pixels_in_point = pango_dpi / 72.0;  // 72 points in an inch
    determined_value = true;
  }

  return pixels_in_point;
}

}  // namespace

PangoContext* GetPangoContext() {
  PangoFontMap* font_map = pango_cairo_font_map_get_default();
  return pango_font_map_create_context(font_map);
}

double GetPangoResolution() {
  static double resolution;
  static bool determined_resolution = false;
  if (!determined_resolution) {
    determined_resolution = true;
    PangoContext* default_context = GetPangoContext();
    resolution = pango_cairo_context_get_resolution(default_context);
    g_object_unref(default_context);
  }
  return resolution;
}

// Pass a width greater than 0 to force wrapping and eliding.
static void SetupPangoLayoutWithoutFont(
    PangoLayout* layout,
    const base::string16& text,
    int width,
    base::i18n::TextDirection text_direction,
    int flags) {
  cairo_font_options_t* cairo_font_options = GetCairoFontOptions();

  // If we got an explicit request to turn off subpixel rendering, disable it on
  // a copy of the static font options object.
  bool copied_cairo_font_options = false;
  if ((flags & Canvas::NO_SUBPIXEL_RENDERING) &&
      (cairo_font_options_get_antialias(cairo_font_options) ==
       CAIRO_ANTIALIAS_SUBPIXEL)) {
    cairo_font_options = cairo_font_options_copy(cairo_font_options);
    copied_cairo_font_options = true;
    cairo_font_options_set_antialias(cairo_font_options, CAIRO_ANTIALIAS_GRAY);
  }

  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);

  if (copied_cairo_font_options) {
    cairo_font_options_destroy(cairo_font_options);
    cairo_font_options = NULL;
  }

  // Set Pango's base text direction explicitly from |text_direction|.
  pango_layout_set_auto_dir(layout, FALSE);
  pango_context_set_base_dir(pango_layout_get_context(layout),
      (text_direction == base::i18n::RIGHT_TO_LEFT ?
       PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR));

  if (width > 0)
    pango_layout_set_width(layout, width * PANGO_SCALE);

  if (flags & Canvas::TEXT_ALIGN_CENTER) {
    // We don't support center aligned w/ eliding.
    DCHECK(gfx::Canvas::NO_ELLIPSIS);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & Canvas::TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & Canvas::NO_ELLIPSIS) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    if (flags & Canvas::MULTI_LINE) {
      pango_layout_set_wrap(layout,
          (flags & Canvas::CHARACTER_BREAK) ?
              PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
    }
  } else if (text_direction == base::i18n::RIGHT_TO_LEFT) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  } else {
    // Fading the text will be handled in the draw operation.
    // Ensure that the text is only on one line.
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
    pango_layout_set_width(layout, -1);
  }

  // Set the resolution to match that used by Gtk. If we don't set the
  // resolution and the resolution differs from the default, Gtk and Chrome end
  // up drawing at different sizes.
  double resolution = GetPangoResolution();
  if (resolution > 0) {
    pango_cairo_context_set_resolution(pango_layout_get_context(layout),
                                       resolution);
  }

  // Set text and accelerator character if needed.
  if (flags & Canvas::SHOW_PREFIX) {
    // Escape the text string to be used as markup.
    std::string utf8 = base::UTF16ToUTF8(text);
    gchar* escaped_text = g_markup_escape_text(utf8.c_str(), utf8.size());
    pango_layout_set_markup_with_accel(layout,
                                       escaped_text,
                                       strlen(escaped_text),
                                       kAcceleratorChar, NULL);
    g_free(escaped_text);
  } else {
    std::string utf8;

    // Remove the ampersand character.  A double ampersand is output as
    // a single ampersand.
    if (flags & Canvas::HIDE_PREFIX) {
      DCHECK_EQ(1, g_unichar_to_utf8(kAcceleratorChar, NULL));
      base::string16 accelerator_removed =
          RemoveAcceleratorChar(text,
                                static_cast<base::char16>(kAcceleratorChar),
                                NULL, NULL);
      utf8 = base::UTF16ToUTF8(accelerator_removed);
    } else {
      utf8 = base::UTF16ToUTF8(text);
    }

    pango_layout_set_text(layout, utf8.data(), utf8.size());
  }
}

void SetupPangoLayout(PangoLayout* layout,
                      const base::string16& text,
                      const Font& font,
                      int width,
                      base::i18n::TextDirection text_direction,
                      int flags) {
  SetupPangoLayoutWithoutFont(layout, text, width, text_direction, flags);

  ScopedPangoFontDescription desc(font.GetNativeFont());
  pango_layout_set_font_description(layout, desc.get());
}

void SetupPangoLayoutWithFontDescription(
    PangoLayout* layout,
    const base::string16& text,
    const std::string& font_description,
    int width,
    base::i18n::TextDirection text_direction,
    int flags) {
  SetupPangoLayoutWithoutFont(layout, text, width, text_direction, flags);

  ScopedPangoFontDescription desc(
      pango_font_description_from_string(font_description.c_str()));
  pango_layout_set_font_description(layout, desc.get());
}

size_t GetPangoFontSizeInPixels(PangoFontDescription* pango_font) {
  size_t size_in_pixels = pango_font_description_get_size(pango_font);
  if (pango_font_description_get_size_is_absolute(pango_font)) {
    // If the size is absolute, then it's in Pango units rather than points.
    // There are PANGO_SCALE Pango units in a device unit (pixel).
    size_in_pixels /= PANGO_SCALE;
  } else {
    // Otherwise, we need to convert from points.
    size_in_pixels = size_in_pixels * GetPixelsInPoint() / PANGO_SCALE;
  }
  return size_in_pixels;
}

PangoFontMetrics* GetPangoFontMetrics(PangoFontDescription* desc) {
  static std::map<int, PangoFontMetrics*>* desc_to_metrics = NULL;
  static PangoContext* context = NULL;

  if (!context) {
    context = GetPangoContext();
    pango_context_set_language(context, pango_language_get_default());
  }

  if (!desc_to_metrics)
    desc_to_metrics = new std::map<int, PangoFontMetrics*>();

  const int desc_hash = pango_font_description_hash(desc);
  std::map<int, PangoFontMetrics*>::iterator i =
      desc_to_metrics->find(desc_hash);

  if (i == desc_to_metrics->end()) {
    PangoFontMetrics* metrics = pango_context_get_metrics(context, desc, NULL);
    desc_to_metrics->insert(std::make_pair(desc_hash, metrics));
    return metrics;
  }
  return i->second;
}

}  // namespace gfx
