// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_pango.h"

#include <pango/pangocairo.h>
#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_render_params_linux.h"
#include "ui/gfx/pango_util.h"
#include "ui/gfx/utf16_indexing.h"

namespace gfx {

namespace {

// Returns the preceding element in a GSList (O(n)).
GSList* GSListPrevious(GSList* head, GSList* item) {
  GSList* prev = NULL;
  for (GSList* cur = head; cur != item; cur = cur->next) {
    DCHECK(cur);
    prev = cur;
  }
  return prev;
}

// Returns true if the given visual cursor |direction| is logically forward
// motion in the given Pango |item|.
bool IsForwardMotion(VisualCursorDirection direction, const PangoItem* item) {
  bool rtl = item->analysis.level & 1;
  return rtl == (direction == CURSOR_LEFT);
}

// Checks whether |range| contains |index|. This is not the same as calling
// range.Contains(Range(index)), which returns true if |index| == |range.end()|.
bool IndexInRange(const Range& range, size_t index) {
  return index >= range.start() && index < range.end();
}

// Sets underline metrics on |renderer| according to Pango font |desc|.
void SetPangoUnderlineMetrics(PangoFontDescription *desc,
                              internal::SkiaTextRenderer* renderer) {
  PangoFontMetrics* metrics = GetPangoFontMetrics(desc);
  int thickness = pango_font_metrics_get_underline_thickness(metrics);
  // Pango returns the position "above the baseline". Change its sign to convert
  // it to a vertical offset from the baseline.
  int position = -pango_font_metrics_get_underline_position(metrics);
  pango_quantize_line_geometry(&thickness, &position);
  // Note: pango_quantize_line_geometry() guarantees pixel boundaries, so
  //       PANGO_PIXELS() is safe to use.
  renderer->SetUnderlineMetrics(PANGO_PIXELS(thickness),
                                PANGO_PIXELS(position));
}

}  // namespace

// TODO(xji): index saved in upper layer is utf16 index. Pango uses utf8 index.
// Since caret_pos is used internally, we could save utf8 index for caret_pos
// to avoid conversion.

RenderTextPango::RenderTextPango()
    : layout_(NULL),
      current_line_(NULL),
      log_attrs_(NULL),
      num_log_attrs_(0),
      layout_text_(NULL) {
}

RenderTextPango::~RenderTextPango() {
  ResetLayout();
}

Size RenderTextPango::GetStringSize() {
  EnsureLayout();
  int width = 0, height = 0;
  pango_layout_get_pixel_size(layout_, &width, &height);
  // Keep a consistent height between this particular string's PangoLayout and
  // potentially larger text supported by the FontList.
  // For example, if a text field contains a Japanese character, which is
  // smaller than Latin ones, and then later a Latin one is inserted, this
  // ensures that the text baseline does not shift.
  return Size(width, std::max(height, font_list().GetHeight()));
}

SelectionModel RenderTextPango::FindCursorPosition(const Point& point) {
  EnsureLayout();

  if (text().empty())
    return SelectionModel(0, CURSOR_FORWARD);

  Point p(ToTextPoint(point));

  // When the point is outside of text, return HOME/END position.
  if (p.x() < 0)
    return EdgeSelectionModel(CURSOR_LEFT);
  if (p.x() > GetStringSize().width())
    return EdgeSelectionModel(CURSOR_RIGHT);

  int caret_pos = 0, trailing = 0;
  pango_layout_xy_to_index(layout_, p.x() * PANGO_SCALE, p.y() * PANGO_SCALE,
                           &caret_pos, &trailing);

  DCHECK_GE(trailing, 0);
  if (trailing > 0) {
    caret_pos = g_utf8_offset_to_pointer(layout_text_ + caret_pos,
                                         trailing) - layout_text_;
    DCHECK_LE(static_cast<size_t>(caret_pos), strlen(layout_text_));
  }

  return SelectionModel(LayoutIndexToTextIndex(caret_pos),
                        (trailing > 0) ? CURSOR_BACKWARD : CURSOR_FORWARD);
}

std::vector<RenderText::FontSpan> RenderTextPango::GetFontSpansForTesting() {
  EnsureLayout();

  std::vector<RenderText::FontSpan> spans;
  for (GSList* it = current_line_->runs; it; it = it->next) {
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(it->data)->item;
    const int start = LayoutIndexToTextIndex(item->offset);
    const int end = LayoutIndexToTextIndex(item->offset + item->length);
    const Range range(start, end);

    ScopedPangoFontDescription desc(pango_font_describe(item->analysis.font));
    spans.push_back(RenderText::FontSpan(Font(desc.get()), range));
  }

  return spans;
}

int RenderTextPango::GetLayoutTextBaseline() {
  EnsureLayout();
  return PANGO_PIXELS(pango_layout_get_baseline(layout_));
}

SelectionModel RenderTextPango::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  GSList* run = GetRunContainingCaret(selection);
  if (!run) {
    // The cursor is not in any run: we're at the visual and logical edge.
    SelectionModel edge = EdgeSelectionModel(direction);
    if (edge.caret_pos() == selection.caret_pos())
      return edge;
    else
      run = (direction == CURSOR_RIGHT) ?
          current_line_->runs : g_slist_last(current_line_->runs);
  } else {
    // If the cursor is moving within the current run, just move it by one
    // grapheme in the appropriate direction.
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t caret = selection.caret_pos();
    if (IsForwardMotion(direction, item)) {
      if (caret < LayoutIndexToTextIndex(item->offset + item->length)) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
        return SelectionModel(caret, CURSOR_BACKWARD);
      }
    } else {
      if (caret > LayoutIndexToTextIndex(item->offset)) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_BACKWARD);
        return SelectionModel(caret, CURSOR_FORWARD);
      }
    }
    // The cursor is at the edge of a run; move to the visually adjacent run.
    // TODO(xji): Keep a vector of runs to avoid using a singly-linked list.
    run = (direction == CURSOR_RIGHT) ?
        run->next : GSListPrevious(current_line_->runs, run);
    if (!run)
      return EdgeSelectionModel(direction);
  }
  PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
  return IsForwardMotion(direction, item) ?
      FirstSelectionModelInsideRun(item) : LastSelectionModelInsideRun(item);
}

SelectionModel RenderTextPango::AdjacentWordSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  if (obscured())
    return EdgeSelectionModel(direction);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  SelectionModel cur(selection);
  for (;;) {
    cur = AdjacentCharSelectionModel(cur, direction);
    GSList* run = GetRunContainingCaret(cur);
    if (!run)
      break;
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    size_t cursor = cur.caret_pos();
    if (IsForwardMotion(direction, item) ?
        iter.IsEndOfWord(cursor) : iter.IsStartOfWord(cursor))
      break;
  }

  return cur;
}

Range RenderTextPango::GetGlyphBounds(size_t index) {
  EnsureLayout();
  PangoRectangle pos;
  pango_layout_index_to_pos(layout_, TextIndexToLayoutIndex(index), &pos);
  // TODO(derat): Support fractional ranges for subpixel positioning?
  return Range(PANGO_PIXELS(pos.x), PANGO_PIXELS(pos.x + pos.width));
}

std::vector<Rect> RenderTextPango::GetSubstringBounds(const Range& range) {
  DCHECK_LE(range.GetMax(), text().length());
  if (range.is_empty())
    return std::vector<Rect>();

  EnsureLayout();
  int* ranges = NULL;
  int n_ranges = 0;
  pango_layout_line_get_x_ranges(current_line_,
                                 TextIndexToLayoutIndex(range.GetMin()),
                                 TextIndexToLayoutIndex(range.GetMax()),
                                 &ranges,
                                 &n_ranges);

  const int height = GetStringSize().height();

  std::vector<Rect> bounds;
  for (int i = 0; i < n_ranges; ++i) {
    // TODO(derat): Support fractional bounds for subpixel positioning?
    int x = PANGO_PIXELS(ranges[2 * i]);
    int width = PANGO_PIXELS(ranges[2 * i + 1]) - x;
    Rect rect(x, 0, width, height);
    rect.set_origin(ToViewPoint(rect.origin()));
    bounds.push_back(rect);
  }
  g_free(ranges);
  return bounds;
}

size_t RenderTextPango::TextIndexToLayoutIndex(size_t index) const {
  DCHECK(layout_);
  ptrdiff_t offset = UTF16IndexToOffset(text(), 0, index);
  // Clamp layout indices to the length of the text actually used for layout.
  offset = std::min<size_t>(offset, g_utf8_strlen(layout_text_, -1));
  const char* layout_pointer = g_utf8_offset_to_pointer(layout_text_, offset);
  return (layout_pointer - layout_text_);
}

size_t RenderTextPango::LayoutIndexToTextIndex(size_t index) const {
  DCHECK(layout_);
  const char* layout_pointer = layout_text_ + index;
  const long offset = g_utf8_pointer_to_offset(layout_text_, layout_pointer);
  return UTF16OffsetToIndex(text(), 0, offset);
}

bool RenderTextPango::IsValidCursorIndex(size_t index) {
  if (index == 0 || index == text().length())
    return true;
  if (!IsValidLogicalIndex(index))
    return false;

  EnsureLayout();
  ptrdiff_t offset = UTF16IndexToOffset(text(), 0, index);
  // Check that the index is marked as a legitimate cursor position by Pango.
  return offset < num_log_attrs_ && log_attrs_[offset].is_cursor_position;
}

void RenderTextPango::ResetLayout() {
  // set_cached_bounds_and_offset_valid(false) is done in RenderText for every
  // operation that triggers ResetLayout().
  if (layout_) {
    // TODO(msw): Keep |layout_| across text changes, etc.; it can be re-used.
    g_object_unref(layout_);
    layout_ = NULL;
  }
  if (current_line_) {
    pango_layout_line_unref(current_line_);
    current_line_ = NULL;
  }
  if (log_attrs_) {
    g_free(log_attrs_);
    log_attrs_ = NULL;
    num_log_attrs_ = 0;
  }
  layout_text_ = NULL;
}

void RenderTextPango::EnsureLayout() {
  if (layout_ == NULL) {
    cairo_surface_t* surface =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
    CHECK_EQ(CAIRO_STATUS_SUCCESS, cairo_surface_status(surface));
    cairo_t* cr = cairo_create(surface);
    CHECK_EQ(CAIRO_STATUS_SUCCESS, cairo_status(cr));

    layout_ = pango_cairo_create_layout(cr);
    CHECK_NE(static_cast<PangoLayout*>(NULL), layout_);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    SetupPangoLayoutWithFontDescription(layout_,
                                        GetLayoutText(),
                                        font_list().GetFontDescriptionString(),
                                        0,
                                        GetTextDirection(),
                                        Canvas::DefaultCanvasTextAlignment());

    // No width set so that the x-axis position is relative to the start of the
    // text. ToViewPoint and ToTextPoint take care of the position conversion
    // between text space and view spaces.
    pango_layout_set_width(layout_, -1);
    // TODO(xji): If RenderText will be used for displaying purpose, such as
    // label, we will need to remove the single-line-mode setting.
    pango_layout_set_single_paragraph_mode(layout_, true);

    layout_text_ = pango_layout_get_text(layout_);
    SetupPangoAttributes(layout_);

    current_line_ = pango_layout_get_line_readonly(layout_, 0);
    CHECK_NE(static_cast<PangoLayoutLine*>(NULL), current_line_);
    pango_layout_line_ref(current_line_);

    pango_layout_get_log_attrs(layout_, &log_attrs_, &num_log_attrs_);
  }
}

void RenderTextPango::SetupPangoAttributes(PangoLayout* layout) {
  PangoAttrList* attrs = pango_attr_list_new();

  // Splitting text runs to accommodate styling can break Arabic glyph shaping.
  // Only split text runs as needed for bold and italic font styles changes.
  BreakList<bool>::const_iterator bold = styles()[BOLD].breaks().begin();
  BreakList<bool>::const_iterator italic = styles()[ITALIC].breaks().begin();
  while (bold != styles()[BOLD].breaks().end() &&
         italic != styles()[ITALIC].breaks().end()) {
    const int style = (bold->second ? Font::BOLD : 0) |
                      (italic->second ? Font::ITALIC : 0);
    const size_t bold_end = styles()[BOLD].GetRange(bold).end();
    const size_t italic_end = styles()[ITALIC].GetRange(italic).end();
    const size_t style_end = std::min(bold_end, italic_end);
    if (style != font_list().GetFontStyle()) {
      FontList derived_font_list = font_list().DeriveWithStyle(style);
      ScopedPangoFontDescription desc(pango_font_description_from_string(
          derived_font_list.GetFontDescriptionString().c_str()));

      PangoAttribute* pango_attr = pango_attr_font_desc_new(desc.get());
      pango_attr->start_index =
          TextIndexToLayoutIndex(std::max(bold->first, italic->first));
      pango_attr->end_index = TextIndexToLayoutIndex(style_end);
      pango_attr_list_insert(attrs, pango_attr);
    }
    bold += bold_end == style_end ? 1 : 0;
    italic += italic_end == style_end ? 1 : 0;
  }
  DCHECK(bold == styles()[BOLD].breaks().end());
  DCHECK(italic == styles()[ITALIC].breaks().end());

  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);
}

void RenderTextPango::DrawVisualText(Canvas* canvas) {
  DCHECK(layout_);

  // Skia will draw glyphs with respect to the baseline.
  Vector2d offset(GetLineOffset(0) + Vector2d(0, GetLayoutTextBaseline()));

  SkScalar x = SkIntToScalar(offset.x());
  SkScalar y = SkIntToScalar(offset.y());

  std::vector<SkPoint> pos;
  std::vector<uint16> glyphs;

  internal::SkiaTextRenderer renderer(canvas);
  ApplyFadeEffects(&renderer);
  ApplyTextShadows(&renderer);

  // TODO(derat): Use font-specific params: http://crbug.com/125235
  const FontRenderParams& render_params = GetDefaultFontRenderParams();
  const bool use_subpixel_rendering =
      render_params.subpixel_rendering !=
          FontRenderParams::SUBPIXEL_RENDERING_NONE;
  renderer.SetFontSmoothingSettings(
      render_params.antialiasing,
      use_subpixel_rendering && !background_is_transparent(),
      render_params.subpixel_positioning);

  SkPaint::Hinting skia_hinting = SkPaint::kNormal_Hinting;
  switch (render_params.hinting) {
    case FontRenderParams::HINTING_NONE:
      skia_hinting = SkPaint::kNo_Hinting;
      break;
    case FontRenderParams::HINTING_SLIGHT:
      skia_hinting = SkPaint::kSlight_Hinting;
      break;
    case FontRenderParams::HINTING_MEDIUM:
      skia_hinting = SkPaint::kNormal_Hinting;
      break;
    case FontRenderParams::HINTING_FULL:
      skia_hinting = SkPaint::kFull_Hinting;
      break;
  }
  renderer.SetFontHinting(skia_hinting);

  // Temporarily apply composition underlines and selection colors.
  ApplyCompositionAndSelectionStyles();

  internal::StyleIterator style(colors(), styles());
  for (GSList* it = current_line_->runs; it; it = it->next) {
    PangoLayoutRun* run = reinterpret_cast<PangoLayoutRun*>(it->data);
    int glyph_count = run->glyphs->num_glyphs;
    // TODO(msw): Skip painting runs outside the display rect area, like Win.
    if (glyph_count == 0)
      continue;

    ScopedPangoFontDescription desc(
        pango_font_describe(run->item->analysis.font));

    const std::string family_name =
        pango_font_description_get_family(desc.get());
    renderer.SetTextSize(GetPangoFontSizeInPixels(desc.get()));

    glyphs.resize(glyph_count);
    pos.resize(glyph_count);

    // Track the current glyph and the glyph at the start of its styled range.
    int glyph_index = 0;
    int style_start_glyph_index = glyph_index;

    // Track the x-coordinates for each styled range (|x| marks the current).
    SkScalar style_start_x = x;

    // Track the current style and its text (not layout) index range.
    style.UpdatePosition(GetGlyphTextIndex(run, style_start_glyph_index));
    Range style_range = style.GetRange();

    do {
      const PangoGlyphInfo& glyph = run->glyphs->glyphs[glyph_index];
      glyphs[glyph_index] = static_cast<uint16>(glyph.glyph);
      // Use pango_units_to_double() rather than PANGO_PIXELS() here, so units
      // are not rounded to the pixel grid if subpixel positioning is enabled.
      pos[glyph_index].set(x + pango_units_to_double(glyph.geometry.x_offset),
                           y + pango_units_to_double(glyph.geometry.y_offset));
      x += pango_units_to_double(glyph.geometry.width);

      ++glyph_index;
      const size_t glyph_text_index = (glyph_index == glyph_count) ?
          style_range.end() : GetGlyphTextIndex(run, glyph_index);
      if (!IndexInRange(style_range, glyph_text_index)) {
        // TODO(asvitkine): For cases like "fi", where "fi" is a single glyph
        //                  but can span multiple styles, Pango splits the
        //                  styles evenly over the glyph. We can do this too by
        //                  clipping and drawing the glyph several times.
        renderer.SetForegroundColor(style.color());
        const int font_style = (style.style(BOLD) ? Font::BOLD : 0) |
                               (style.style(ITALIC) ? Font::ITALIC : 0);
        renderer.SetFontFamilyWithStyle(family_name, font_style);
        renderer.DrawPosText(&pos[style_start_glyph_index],
                             &glyphs[style_start_glyph_index],
                             glyph_index - style_start_glyph_index);
        if (style.style(UNDERLINE))
          SetPangoUnderlineMetrics(desc.get(), &renderer);
        renderer.DrawDecorations(style_start_x, y, x - style_start_x,
                                 style.style(UNDERLINE), style.style(STRIKE),
                                 style.style(DIAGONAL_STRIKE));
        style.UpdatePosition(glyph_text_index);
        style_range = style.GetRange();
        style_start_glyph_index = glyph_index;
        style_start_x = x;
      }
    } while (glyph_index < glyph_count);
  }

  renderer.EndDiagonalStrike();

  // Undo the temporarily applied composition underlines and selection colors.
  UndoCompositionAndSelectionStyles();
}

GSList* RenderTextPango::GetRunContainingCaret(
    const SelectionModel& caret) const {
  size_t position = TextIndexToLayoutIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  GSList* run = current_line_->runs;
  while (run) {
    PangoItem* item = reinterpret_cast<PangoLayoutRun*>(run->data)->item;
    Range item_range(item->offset, item->offset + item->length);
    if (RangeContainsCaret(item_range, position, affinity))
      return run;
    run = run->next;
  }
  return NULL;
}

SelectionModel RenderTextPango::FirstSelectionModelInsideRun(
    const PangoItem* item) {
  size_t caret = IndexOfAdjacentGrapheme(
      LayoutIndexToTextIndex(item->offset), CURSOR_FORWARD);
  return SelectionModel(caret, CURSOR_BACKWARD);
}

SelectionModel RenderTextPango::LastSelectionModelInsideRun(
    const PangoItem* item) {
  size_t caret = IndexOfAdjacentGrapheme(
      LayoutIndexToTextIndex(item->offset + item->length), CURSOR_BACKWARD);
  return SelectionModel(caret, CURSOR_FORWARD);
}

size_t RenderTextPango::GetGlyphTextIndex(PangoLayoutRun* run,
                                          int glyph_index) const {
  return LayoutIndexToTextIndex(run->item->offset +
                                run->glyphs->log_clusters[glyph_index]);
}

RenderText* RenderText::CreateNativeInstance() {
  return new RenderTextPango;
}

}  // namespace gfx
