// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/renderer_preferences.h"
#include "content/child/webthemeengine_impl_default.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/core/SkFontLCDConfig.h"
#include "ui/gfx/font_render_params.h"

using blink::WebFontRendering;

namespace content {

void RenderViewImpl::UpdateFontRenderingFromRendererPrefs() {
  const RendererPreferences& prefs = renderer_preferences_;

  // Cache the system font metrics in blink.
  blink::WebFontRendering::setMenuFontMetrics(
      prefs.menu_font_family_name.c_str(), prefs.menu_font_height);

  blink::WebFontRendering::setSmallCaptionFontMetrics(
      prefs.small_caption_font_family_name.c_str(),
      prefs.small_caption_font_height);

  blink::WebFontRendering::setStatusFontMetrics(
      prefs.status_font_family_name.c_str(), prefs.status_font_height);

  SkFontLCDConfig::SetSubpixelOrder(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrder(
          prefs.subpixel_rendering));
  SkFontLCDConfig::SetSubpixelOrientation(
      gfx::FontRenderParams::SubpixelRenderingToSkiaLCDOrientation(
          prefs.subpixel_rendering));

  blink::WebFontRendering::setAntialiasedTextEnabled(
      prefs.should_antialias_text);
  blink::WebFontRendering::setLCDTextEnabled(prefs.subpixel_rendering
      != gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
}

}  // namespace content
