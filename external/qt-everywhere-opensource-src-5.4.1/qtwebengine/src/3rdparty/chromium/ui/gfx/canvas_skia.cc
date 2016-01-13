// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"

namespace gfx {

namespace {

#if defined(OS_WIN)
// If necessary, wraps |text| with RTL/LTR directionality characters based on
// |flags| and |text| content.
// Returns true if the text will be rendered right-to-left.
// TODO(msw): Nix this, now that RenderTextWin supports directionality directly.
bool AdjustStringDirection(int flags, base::string16* text) {
  // TODO(msw): FORCE_LTR_DIRECTIONALITY does not work for RTL text now.

  // If the string is empty or LTR was forced, simply return false since the
  // default RenderText directionality is already LTR.
  if (text->empty() || (flags & Canvas::FORCE_LTR_DIRECTIONALITY))
    return false;

  // If RTL is forced, apply it to the string.
  if (flags & Canvas::FORCE_RTL_DIRECTIONALITY) {
    base::i18n::WrapStringWithRTLFormatting(text);
    return true;
  }

  // If a direction wasn't forced but the UI language is RTL and there were
  // strong RTL characters, ensure RTL is applied.
  if (base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(*text)) {
    base::i18n::WrapStringWithRTLFormatting(text);
    return true;
  }

  // In the default case, the string should be rendered as LTR. RenderText's
  // default directionality is LTR, so the text doesn't need to be wrapped.
  // Note that individual runs within the string may still be rendered RTL
  // (which will be the case for RTL text under non-RTL locales, since under RTL
  // locales it will be handled by the if statement above).
  return false;
}
#endif  // defined(OS_WIN)

// Checks each pixel immediately adjacent to the given pixel in the bitmap. If
// any of them are not the halo color, returns true. This defines the halo of
// pixels that will appear around the text. Note that we have to check each
// pixel against both the halo color and transparent since
// |DrawStringRectWithHalo| will modify the bitmap as it goes, and cleared
// pixels shouldn't count as changed.
bool PixelShouldGetHalo(const SkBitmap& bitmap,
                        int x, int y,
                        SkColor halo_color) {
  if (x > 0 &&
      *bitmap.getAddr32(x - 1, y) != halo_color &&
      *bitmap.getAddr32(x - 1, y) != 0)
    return true;  // Touched pixel to the left.
  if (x < bitmap.width() - 1 &&
      *bitmap.getAddr32(x + 1, y) != halo_color &&
      *bitmap.getAddr32(x + 1, y) != 0)
    return true;  // Touched pixel to the right.
  if (y > 0 &&
      *bitmap.getAddr32(x, y - 1) != halo_color &&
      *bitmap.getAddr32(x, y - 1) != 0)
    return true;  // Touched pixel above.
  if (y < bitmap.height() - 1 &&
      *bitmap.getAddr32(x, y + 1) != halo_color &&
      *bitmap.getAddr32(x, y + 1) != 0)
    return true;  // Touched pixel below.
  return false;
}

// Strips accelerator character prefixes in |text| if needed, based on |flags|.
// Returns a range in |text| to underline or Range::InvalidRange() if
// underlining is not needed.
Range StripAcceleratorChars(int flags, base::string16* text) {
  if (flags & (Canvas::SHOW_PREFIX | Canvas::HIDE_PREFIX)) {
    int char_pos = -1;
    int char_span = 0;
    *text = RemoveAcceleratorChar(*text, '&', &char_pos, &char_span);
    if ((flags & Canvas::SHOW_PREFIX) && char_pos != -1)
      return Range(char_pos, char_pos + char_span);
  }
  return Range::InvalidRange();
}

// Elides |text| and adjusts |range| appropriately. If eliding causes |range|
// to no longer point to the same character in |text|, |range| is made invalid.
void ElideTextAndAdjustRange(const FontList& font_list,
                             int width,
                             base::string16* text,
                             Range* range) {
  const base::char16 start_char =
      (range->IsValid() ? text->at(range->start()) : 0);
  *text = ElideText(*text, font_list, width, ELIDE_TAIL);
  if (!range->IsValid())
    return;
  if (range->start() >= text->length() ||
      text->at(range->start()) != start_char) {
    *range = Range::InvalidRange();
  }
}

// Updates |render_text| from the specified parameters.
void UpdateRenderText(const Rect& rect,
                      const base::string16& text,
                      const FontList& font_list,
                      int flags,
                      SkColor color,
                      RenderText* render_text) {
  render_text->SetFontList(font_list);
  render_text->SetText(text);
  render_text->SetCursorEnabled(false);

  Rect display_rect = rect;
  display_rect.set_height(font_list.GetHeight());
  render_text->SetDisplayRect(display_rect);

  // Set the text alignment explicitly based on the directionality of the UI,
  // if not specified.
  if (!(flags & (Canvas::TEXT_ALIGN_CENTER |
                 Canvas::TEXT_ALIGN_RIGHT |
                 Canvas::TEXT_ALIGN_LEFT))) {
    flags |= Canvas::DefaultCanvasTextAlignment();
  }

  if (flags & Canvas::TEXT_ALIGN_RIGHT)
    render_text->SetHorizontalAlignment(ALIGN_RIGHT);
  else if (flags & Canvas::TEXT_ALIGN_CENTER)
    render_text->SetHorizontalAlignment(ALIGN_CENTER);
  else
    render_text->SetHorizontalAlignment(ALIGN_LEFT);

  if (flags & Canvas::NO_SUBPIXEL_RENDERING)
    render_text->set_background_is_transparent(true);

  render_text->SetColor(color);
  const int font_style = font_list.GetFontStyle();
  render_text->SetStyle(BOLD, (font_style & Font::BOLD) != 0);
  render_text->SetStyle(ITALIC, (font_style & Font::ITALIC) != 0);
  render_text->SetStyle(UNDERLINE, (font_style & Font::UNDERLINE) != 0);
}

}  // namespace

// static
void Canvas::SizeStringFloat(const base::string16& text,
                             const FontList& font_list,
                             float* width, float* height,
                             int line_height,
                             int flags) {
  DCHECK_GE(*width, 0);
  DCHECK_GE(*height, 0);

  base::string16 adjusted_text = text;
#if defined(OS_WIN)
  AdjustStringDirection(flags, &adjusted_text);
#endif

  if ((flags & MULTI_LINE) && *width != 0) {
    WordWrapBehavior wrap_behavior = TRUNCATE_LONG_WORDS;
    if (flags & CHARACTER_BREAK)
      wrap_behavior = WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ELIDE_LONG_WORDS;

    Rect rect(*width, INT_MAX);
    std::vector<base::string16> strings;
    ElideRectangleText(adjusted_text, font_list, rect.width(), rect.height(),
                       wrap_behavior, &strings);
    scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
    UpdateRenderText(rect, base::string16(), font_list, flags, 0,
                     render_text.get());

    float h = 0;
    float w = 0;
    for (size_t i = 0; i < strings.size(); ++i) {
      StripAcceleratorChars(flags, &strings[i]);
      render_text->SetText(strings[i]);
      const SizeF& string_size = render_text->GetStringSizeF();
      w = std::max(w, string_size.width());
      h += (i > 0 && line_height > 0) ? line_height : string_size.height();
    }
    *width = w;
    *height = h;
  } else {
    // If the string is too long, the call by |RenderTextWin| to |ScriptShape()|
    // will inexplicably fail with result E_INVALIDARG. Guard against this.
    const size_t kMaxRenderTextLength = 5000;
    if (adjusted_text.length() >= kMaxRenderTextLength) {
      *width = font_list.GetExpectedTextWidth(adjusted_text.length());
      *height = font_list.GetHeight();
    } else {
      scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
      Rect rect(*width, *height);
      StripAcceleratorChars(flags, &adjusted_text);
      UpdateRenderText(rect, adjusted_text, font_list, flags, 0,
                       render_text.get());
      const SizeF& string_size = render_text->GetStringSizeF();
      *width = string_size.width();
      *height = string_size.height();
    }
  }
}

void Canvas::DrawStringRectWithShadows(const base::string16& text,
                                       const FontList& font_list,
                                       SkColor color,
                                       const Rect& text_bounds,
                                       int line_height,
                                       int flags,
                                       const ShadowValues& shadows) {
  if (!IntersectsClipRect(text_bounds))
    return;

  Rect clip_rect(text_bounds);
  clip_rect.Inset(ShadowValue::GetMargin(shadows));

  canvas_->save();
  ClipRect(clip_rect);

  Rect rect(text_bounds);
  base::string16 adjusted_text = text;

#if defined(OS_WIN)
  AdjustStringDirection(flags, &adjusted_text);
#endif

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->set_shadows(shadows);

  if (flags & MULTI_LINE) {
    WordWrapBehavior wrap_behavior = IGNORE_LONG_WORDS;
    if (flags & CHARACTER_BREAK)
      wrap_behavior = WRAP_LONG_WORDS;
    else if (!(flags & NO_ELLIPSIS))
      wrap_behavior = ELIDE_LONG_WORDS;

    std::vector<base::string16> strings;
    ElideRectangleText(adjusted_text, font_list, text_bounds.width(),
                       text_bounds.height(), wrap_behavior, &strings);

    for (size_t i = 0; i < strings.size(); i++) {
      Range range = StripAcceleratorChars(flags, &strings[i]);
      UpdateRenderText(rect, strings[i], font_list, flags, color,
                       render_text.get());
      int line_padding = 0;
      if (line_height > 0)
        line_padding = line_height - render_text->GetStringSize().height();
      else
        line_height = render_text->GetStringSize().height();

      // TODO(msw|asvitkine): Center Windows multi-line text: crbug.com/107357
#if !defined(OS_WIN)
      if (i == 0) {
        // TODO(msw|asvitkine): Support multi-line text with varied heights.
        const int text_height = strings.size() * line_height - line_padding;
        rect += Vector2d(0, (text_bounds.height() - text_height) / 2);
      }
#endif

      rect.set_height(line_height - line_padding);

      if (range.IsValid())
        render_text->ApplyStyle(UNDERLINE, true, range);
      render_text->SetDisplayRect(rect);
      render_text->Draw(this);
      rect += Vector2d(0, line_height);
    }
  } else {
    Range range = StripAcceleratorChars(flags, &adjusted_text);
    bool elide_text = ((flags & NO_ELLIPSIS) == 0);

#if defined(OS_LINUX)
    // On Linux, eliding really means fading the end of the string. But only
    // for LTR text. RTL text is still elided (on the left) with "...".
    if (elide_text) {
      render_text->SetText(adjusted_text);
      if (render_text->GetTextDirection() == base::i18n::LEFT_TO_RIGHT) {
        render_text->SetElideBehavior(FADE_TAIL);
        elide_text = false;
      }
    }
#endif

    if (elide_text) {
      ElideTextAndAdjustRange(font_list, text_bounds.width(), &adjusted_text,
                              &range);
    }

    UpdateRenderText(rect, adjusted_text, font_list, flags, color,
                     render_text.get());

    const int text_height = render_text->GetStringSize().height();
    rect += Vector2d(0, (text_bounds.height() - text_height) / 2);
    rect.set_height(text_height);
    render_text->SetDisplayRect(rect);
    if (range.IsValid())
      render_text->ApplyStyle(UNDERLINE, true, range);
    render_text->Draw(this);
  }

  canvas_->restore();
}

void Canvas::DrawStringRectWithHalo(const base::string16& text,
                                    const FontList& font_list,
                                    SkColor text_color,
                                    SkColor halo_color_in,
                                    const Rect& display_rect,
                                    int flags) {
  // Some callers will have semitransparent halo colors, which we don't handle
  // (since the resulting image can have 1-bit transparency only).
  SkColor halo_color = SkColorSetA(halo_color_in, 0xFF);

  // Create a temporary buffer filled with the halo color. It must leave room
  // for the 1-pixel border around the text.
  Size size(display_rect.width() + 2, display_rect.height() + 2);
  Canvas text_canvas(size, image_scale(), false);
  SkPaint bkgnd_paint;
  bkgnd_paint.setColor(halo_color);
  text_canvas.DrawRect(Rect(size), bkgnd_paint);

  // Draw the text into the temporary buffer. This will have correct
  // ClearType since the background color is the same as the halo color.
  text_canvas.DrawStringRectWithFlags(
      text, font_list, text_color,
      Rect(1, 1, display_rect.width(), display_rect.height()), flags);

  uint32_t halo_premul = SkPreMultiplyColor(halo_color);
  SkBitmap& text_bitmap = const_cast<SkBitmap&>(
      skia::GetTopDevice(*text_canvas.sk_canvas())->accessBitmap(true));

  for (int cur_y = 0; cur_y < text_bitmap.height(); cur_y++) {
    uint32_t* text_row = text_bitmap.getAddr32(0, cur_y);
    for (int cur_x = 0; cur_x < text_bitmap.width(); cur_x++) {
      if (text_row[cur_x] == halo_premul) {
        // This pixel was not touched by the text routines. See if it borders
        // a touched pixel in any of the 4 directions (not diagonally).
        if (!PixelShouldGetHalo(text_bitmap, cur_x, cur_y, halo_premul))
          text_row[cur_x] = 0;  // Make transparent.
      } else {
        text_row[cur_x] |= 0xff << SK_A32_SHIFT;  // Make opaque.
      }
    }
  }

  // Draw the halo bitmap with blur.
  ImageSkia text_image = ImageSkia(ImageSkiaRep(text_bitmap,
      text_canvas.image_scale()));
  DrawImageInt(text_image, display_rect.x() - 1, display_rect.y() - 1);
}

void Canvas::DrawFadedString(const base::string16& text,
                             const FontList& font_list,
                             SkColor color,
                             const Rect& display_rect,
                             int flags) {
  // If the whole string fits in the destination then just draw it directly.
  if (GetStringWidth(text, font_list) <= display_rect.width()) {
    DrawStringRectWithFlags(text, font_list, color, display_rect, flags);
    return;
  }

  // Align with forced content directionality, overriding alignment flags.
  if (flags & FORCE_RTL_DIRECTIONALITY) {
    flags &= ~(TEXT_ALIGN_CENTER | TEXT_ALIGN_LEFT);
    flags |= TEXT_ALIGN_RIGHT;
  } else if (flags & FORCE_LTR_DIRECTIONALITY) {
    flags &= ~(TEXT_ALIGN_CENTER | TEXT_ALIGN_RIGHT);
    flags |= TEXT_ALIGN_LEFT;
  } else if (!(flags & TEXT_ALIGN_LEFT) && !(flags & TEXT_ALIGN_RIGHT)) {
    // Also align with content directionality instead of fading both ends.
    flags &= ~TEXT_ALIGN_CENTER;
    const bool is_rtl = base::i18n::GetFirstStrongCharacterDirection(text) ==
                        base::i18n::RIGHT_TO_LEFT;
    flags |= is_rtl ? TEXT_ALIGN_RIGHT : TEXT_ALIGN_LEFT;
  }
  flags |= NO_ELLIPSIS;

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  Rect rect = display_rect;
  UpdateRenderText(rect, text, font_list, flags, color, render_text.get());
  render_text->SetElideBehavior(FADE_TAIL);

  const int line_height = render_text->GetStringSize().height();
  rect += Vector2d(0, (display_rect.height() - line_height) / 2);
  rect.set_height(line_height);
  render_text->SetDisplayRect(rect);

  canvas_->save();
  ClipRect(display_rect);
  render_text->Draw(this);
  canvas_->restore();
}

}  // namespace gfx
