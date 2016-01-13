// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_view_impl.h"

#include "content/public/common/renderer_preferences.h"
#include "third_party/WebKit/public/web/linux/WebFontRendering.h"

using blink::WebFontRendering;

namespace content {

static SkPaint::Hinting RendererPreferencesToSkiaHinting(
    const RendererPreferences& prefs) {
  if (!prefs.should_antialias_text) {
    // When anti-aliasing is off, GTK maps all non-zero hinting settings to
    // 'Normal' hinting so we do the same. Otherwise, folks who have 'Slight'
    // hinting selected will see readable text in everything expect Chromium.
    switch (prefs.hinting) {
      case RENDERER_PREFERENCES_HINTING_NONE:
        return SkPaint::kNo_Hinting;
      case RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT:
      case RENDERER_PREFERENCES_HINTING_SLIGHT:
      case RENDERER_PREFERENCES_HINTING_MEDIUM:
      case RENDERER_PREFERENCES_HINTING_FULL:
        return SkPaint::kNormal_Hinting;
      default:
        NOTREACHED();
        return SkPaint::kNormal_Hinting;
    }
  }

  switch (prefs.hinting) {
  case RENDERER_PREFERENCES_HINTING_SYSTEM_DEFAULT:
    return SkPaint::kNormal_Hinting;
  case RENDERER_PREFERENCES_HINTING_NONE:
    return SkPaint::kNo_Hinting;
  case RENDERER_PREFERENCES_HINTING_SLIGHT:
    return SkPaint::kSlight_Hinting;
  case RENDERER_PREFERENCES_HINTING_MEDIUM:
    return SkPaint::kNormal_Hinting;
  case RENDERER_PREFERENCES_HINTING_FULL:
    return SkPaint::kFull_Hinting;
  default:
    NOTREACHED();
    return SkPaint::kNormal_Hinting;
  }
}

static SkFontHost::LCDOrder RendererPreferencesToSkiaLCDOrder(
    RendererPreferencesSubpixelRenderingEnum subpixel) {
  switch (subpixel) {
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB:
    return SkFontHost::kRGB_LCDOrder;
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR:
    return SkFontHost::kBGR_LCDOrder;
  default:
    NOTREACHED();
    return SkFontHost::kRGB_LCDOrder;
  }
}

static SkFontHost::LCDOrientation
    RendererPreferencesToSkiaLCDOrientation(
        RendererPreferencesSubpixelRenderingEnum subpixel) {
  switch (subpixel) {
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_RGB:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_BGR:
    return SkFontHost::kHorizontal_LCDOrientation;
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VRGB:
  case RENDERER_PREFERENCES_SUBPIXEL_RENDERING_VBGR:
    return SkFontHost::kVertical_LCDOrientation;
  default:
    NOTREACHED();
    return SkFontHost::kHorizontal_LCDOrientation;
  }
}

static bool RendererPreferencesToAntiAliasFlag(
    const RendererPreferences& prefs) {
  return prefs.should_antialias_text;
}

static bool RendererPreferencesToSubpixelRenderingFlag(
    const RendererPreferences& prefs) {
  if (prefs.subpixel_rendering !=
        RENDERER_PREFERENCES_SUBPIXEL_RENDERING_SYSTEM_DEFAULT &&
      prefs.subpixel_rendering !=
        RENDERER_PREFERENCES_SUBPIXEL_RENDERING_NONE) {
    return true;
  }
  return false;
}

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  const RendererPreferences& prefs = renderer_preferences_;
  WebFontRendering::setHinting(RendererPreferencesToSkiaHinting(prefs));
  WebFontRendering::setAutoHint(prefs.use_autohinter);
  WebFontRendering::setUseBitmaps(prefs.use_bitmaps);
  WebFontRendering::setLCDOrder(
      RendererPreferencesToSkiaLCDOrder(prefs.subpixel_rendering));
  WebFontRendering::setLCDOrientation(
      RendererPreferencesToSkiaLCDOrientation(prefs.subpixel_rendering));
  WebFontRendering::setAntiAlias(RendererPreferencesToAntiAliasFlag(prefs));
  WebFontRendering::setSubpixelRendering(
      RendererPreferencesToSubpixelRenderingFlag(prefs));
  WebFontRendering::setSubpixelPositioning(prefs.use_subpixel_positioning);
}

}  // namespace content
