// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"
#include "third_party/skia/include/core/SkFontLCDConfig.h"
#include "ui/gfx/font_render_params.h"

using blink::WebFontRendering;

namespace content {

namespace {

const int kMaxDefaultFontSize = 999;

SkPaint::Hinting RendererPreferencesToSkiaHinting(
    const RendererPreferences& prefs) {
  if (!prefs.should_antialias_text) {
    // When anti-aliasing is off, GTK maps all non-zero hinting settings to
    // 'Normal' hinting so we do the same. Otherwise, folks who have 'Slight'
    // hinting selected will see readable text in everything expect Chromium.
    switch (prefs.hinting) {
      case gfx::FontRenderParams::HINTING_NONE:
        return SkPaint::kNo_Hinting;
      case gfx::FontRenderParams::HINTING_SLIGHT:
      case gfx::FontRenderParams::HINTING_MEDIUM:
      case gfx::FontRenderParams::HINTING_FULL:
        return SkPaint::kNormal_Hinting;
      default:
        NOTREACHED();
        return SkPaint::kNormal_Hinting;
    }
  }

  switch (prefs.hinting) {
    case gfx::FontRenderParams::HINTING_NONE:   return SkPaint::kNo_Hinting;
    case gfx::FontRenderParams::HINTING_SLIGHT: return SkPaint::kSlight_Hinting;
    case gfx::FontRenderParams::HINTING_MEDIUM: return SkPaint::kNormal_Hinting;
    case gfx::FontRenderParams::HINTING_FULL:   return SkPaint::kFull_Hinting;
    default:
      NOTREACHED();
      return SkPaint::kNormal_Hinting;
    }
}

}  // namespace

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  const RendererPreferences& prefs = renderer_preferences_;
  WebFontRendering::setHinting(RendererPreferencesToSkiaHinting(prefs));
  WebFontRendering::setAutoHint(prefs.use_autohinter);
  WebFontRendering::setUseBitmaps(prefs.use_bitmaps);
  SkFontLCDConfig::SetSubpixelOrder(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          prefs.subpixel_rendering));
  SkFontLCDConfig::SetSubpixelOrientation(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          prefs.subpixel_rendering));
  WebFontRendering::setAntiAlias(prefs.should_antialias_text);
  WebFontRendering::setSubpixelRendering(
      prefs.subpixel_rendering !=
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
  WebFontRendering::setSubpixelPositioning(prefs.use_subpixel_positioning);
  if (prefs.default_font_size > 0 &&
      prefs.default_font_size <= kMaxDefaultFontSize) {
    WebFontRendering::setDefaultFontSize(prefs.default_font_size);
  }
#if !defined(OS_ANDROID)
  if (!prefs.system_font_family_name.empty()) {
    WebFontRendering::setSystemFontFamily(
        blink::WebString::fromUTF8(prefs.system_font_family_name));
  }
#endif
}

}  // namespace content
