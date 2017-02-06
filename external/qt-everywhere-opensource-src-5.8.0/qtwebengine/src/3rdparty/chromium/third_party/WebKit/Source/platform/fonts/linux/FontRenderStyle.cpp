// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/fonts/linux/FontRenderStyle.h"

#include "platform/LayoutTestSupport.h"
#include "platform/fonts/FontDescription.h"
#include "public/platform/Platform.h"
#include "public/platform/linux/WebFontRenderStyle.h"
#include "public/platform/linux/WebSandboxSupport.h"

#include "ui/gfx/font_render_params.h"
#include "ui/gfx/font.h"

namespace blink {

namespace {

// These functions are also implemented in sandbox_ipc_linux.cc
// Converts gfx::FontRenderParams::Hinting to WebFontRenderStyle::hintStyle.
// Returns an int for serialization, but the underlying Blink type is a char.
int ConvertHinting(gfx::FontRenderParams::Hinting hinting) {
  switch (hinting) {
    case gfx::FontRenderParams::HINTING_NONE:   return 0;
    case gfx::FontRenderParams::HINTING_SLIGHT: return 1;
    case gfx::FontRenderParams::HINTING_MEDIUM: return 2;
    case gfx::FontRenderParams::HINTING_FULL:   return 3;
  }
  NOTREACHED() << "Unexpected hinting value " << hinting;
  return 0;
}

// Converts gfx::FontRenderParams::SubpixelRendering to
// WebFontRenderStyle::useSubpixelRendering. Returns an int for serialization,
// but the underlying Blink type is a char.
int ConvertSubpixelRendering(
    gfx::FontRenderParams::SubpixelRendering rendering) {
  switch (rendering) {
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE: return 0;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_RGB:  return 1;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_BGR:  return 1;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VRGB: return 1;
    case gfx::FontRenderParams::SUBPIXEL_RENDERING_VBGR: return 1;
  }
  NOTREACHED() << "Unexpected subpixel rendering value " << rendering;
  return 0;
}

SkPaint::Hinting skiaHinting = SkPaint::kNormal_Hinting;
bool useSkiaAutoHint = true;
bool useSkiaBitmaps = true;
bool useSkiaAntiAlias = true;
bool useSkiaSubpixelRendering = false;

} // namespace

// static
void FontRenderStyle::setHinting(SkPaint::Hinting hinting)
{
    skiaHinting = hinting;
}

// static
void FontRenderStyle::setAutoHint(bool useAutoHint)
{
    useSkiaAutoHint = useAutoHint;
}

// static
void FontRenderStyle::setUseBitmaps(bool useBitmaps)
{
    useSkiaBitmaps = useBitmaps;
}

// static
void FontRenderStyle::setAntiAlias(bool useAntiAlias)
{
    useSkiaAntiAlias = useAntiAlias;
}

// static
void FontRenderStyle::setSubpixelRendering(bool useSubpixelRendering)
{
    useSkiaSubpixelRendering = useSubpixelRendering;
}

// static
FontRenderStyle FontRenderStyle::querySystem(const CString& family, float textSize, SkTypeface::Style typefaceStyle)
{
    WebFontRenderStyle style;
#if OS(ANDROID)
    style.setDefaults();
#else
    // If the the sandbox is disabled, we can query font parameters directly.
    if (!Platform::current()->sandboxSupport()) {
        gfx::FontRenderParamsQuery query;
        if (family.length())
            query.families.push_back(family.data());
        query.pixel_size = textSize;
        query.style = (typefaceStyle & 2) ? gfx::Font::ITALIC : gfx::Font::NORMAL;
        query.weight = (typefaceStyle & 1) ? gfx::Font::Weight::BOLD : gfx::Font::Weight::NORMAL;
        const gfx::FontRenderParams params = gfx::GetFontRenderParams(query, NULL);
        style.useBitmaps = params.use_bitmaps;
        style.useAutoHint = params.autohinter;
        style.useHinting = params.hinting != gfx::FontRenderParams::HINTING_NONE;
        style.hintStyle = ConvertHinting(params.hinting);
        style.useAntiAlias = params.antialiasing;
        style.useSubpixelRendering = ConvertSubpixelRendering(params.subpixel_rendering);
        style.useSubpixelPositioning = params.subpixel_positioning;
    } else {
        const int sizeAndStyle = (((int)textSize) << 2) | (typefaceStyle & 3);
        Platform::current()->sandboxSupport()->getWebFontRenderStyleForStrike(family.data(), sizeAndStyle, &style);
    }
#endif

    FontRenderStyle result;
    style.toFontRenderStyle(&result);

    // Fix FontRenderStyle::NoPreference to actual styles.
    if (result.useAntiAlias == FontRenderStyle::NoPreference)
        result.useAntiAlias = useSkiaAntiAlias;

    if (!result.useHinting)
        result.hintStyle = SkPaint::kNo_Hinting;
    else if (result.useHinting == FontRenderStyle::NoPreference)
        result.hintStyle = skiaHinting;

    if (result.useBitmaps == FontRenderStyle::NoPreference)
        result.useBitmaps = useSkiaBitmaps;
    if (result.useAutoHint == FontRenderStyle::NoPreference)
        result.useAutoHint = useSkiaAutoHint;
    if (result.useAntiAlias == FontRenderStyle::NoPreference)
        result.useAntiAlias = useSkiaAntiAlias;
    if (result.useSubpixelRendering == FontRenderStyle::NoPreference)
        result.useSubpixelRendering = useSkiaSubpixelRendering;

    // TestRunner specifically toggles the subpixel positioning flag.
    if (result.useSubpixelPositioning == FontRenderStyle::NoPreference
        || LayoutTestSupport::isRunningLayoutTest())
        result.useSubpixelPositioning = FontDescription::subpixelPositioning();

    return result;
}

void FontRenderStyle::applyToPaint(SkPaint& paint, float deviceScaleFactor) const
{
    paint.setAntiAlias(useAntiAlias);
    paint.setHinting(static_cast<SkPaint::Hinting>(hintStyle));
    paint.setEmbeddedBitmapText(useBitmaps);
    paint.setAutohinted(useAutoHint);
    if (useAntiAlias)
        paint.setLCDRenderText(useSubpixelRendering);

    // Do not enable subpixel text on low-dpi if normal or full hinting is requested.
    bool useSubpixelText = (paint.getHinting() < SkPaint::kNormal_Hinting || deviceScaleFactor > 1.0f);

    // TestRunner specifically toggles the subpixel positioning flag.
    if (useSubpixelText && !LayoutTestSupport::isRunningLayoutTest())
        paint.setSubpixelText(true);
    else
        paint.setSubpixelText(useSubpixelPositioning);
}

} // namespace blink
