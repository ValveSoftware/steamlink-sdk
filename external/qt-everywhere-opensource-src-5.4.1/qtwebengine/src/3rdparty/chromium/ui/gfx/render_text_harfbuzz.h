// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_HARFBUZZ_H_
#define UI_GFX_RENDER_TEXT_HARFBUZZ_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "third_party/harfbuzz-ng/src/hb.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/icu/source/common/unicode/uscript.h"
#include "ui/gfx/render_text.h"

namespace gfx {

namespace internal {

struct GFX_EXPORT TextRunHarfBuzz {
  TextRunHarfBuzz();
  ~TextRunHarfBuzz();

  // Returns the index of the first glyph that corresponds to the character at
  // |pos|.
  size_t CharToGlyph(size_t pos) const;

  // Returns the corresponding glyph range of the given character range.
  // |range| is in text-space (0 corresponds to |GetLayoutText()[0]|). Returned
  // value is in run-space (0 corresponds to the first glyph in the run).
  Range CharRangeToGlyphRange(const Range& range) const;

  // Returns whether the given shaped run contains any missing glyphs.
  bool HasMissingGlyphs() const;

  // Returns the X coordinate of the leading or |trailing| edge of the glyph
  // starting at |text_index|, relative to the left of the text (not the view).
  int GetGlyphXBoundary(size_t text_index, bool trailing) const;

  int width;
  int preceding_run_widths;
  Range range;
  bool is_rtl;
  UBiDiLevel level;
  UScriptCode script;

  scoped_ptr<uint16[]> glyphs;
  scoped_ptr<SkPoint[]> positions;
  scoped_ptr<uint32[]> glyph_to_char;
  size_t glyph_count;

  skia::RefPtr<SkTypeface> skia_face;
  int font_size;
  int font_style;
  bool strike;
  bool diagonal_strike;
  bool underline;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextRunHarfBuzz);
};

}  // namespace internal

class GFX_EXPORT RenderTextHarfBuzz : public RenderText {
 public:
  RenderTextHarfBuzz();
  virtual ~RenderTextHarfBuzz();

  // Overridden from RenderText.
  virtual Size GetStringSize() OVERRIDE;
  virtual SelectionModel FindCursorPosition(const Point& point) OVERRIDE;
  virtual std::vector<FontSpan> GetFontSpansForTesting() OVERRIDE;

 protected:
  // Overridden from RenderText.
  virtual int GetLayoutTextBaseline() OVERRIDE;
  virtual SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) OVERRIDE;
  virtual SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) OVERRIDE;
  virtual Range GetGlyphBounds(size_t index) OVERRIDE;
  virtual std::vector<Rect> GetSubstringBounds(const Range& range) OVERRIDE;
  virtual size_t TextIndexToLayoutIndex(size_t index) const OVERRIDE;
  virtual size_t LayoutIndexToTextIndex(size_t index) const OVERRIDE;
  virtual bool IsValidCursorIndex(size_t index) OVERRIDE;
  virtual void ResetLayout() OVERRIDE;
  virtual void EnsureLayout() OVERRIDE;
  virtual void DrawVisualText(Canvas* canvas) OVERRIDE;

 private:
  friend class RenderTextTest;
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_RunDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_BreakRunsByUnicodeBlocks);

  // Return the run index that contains the argument; or the length of the
  // |runs_| vector if argument exceeds the text length or width.
  size_t GetRunContainingCaret(const SelectionModel& caret) const;
  size_t GetRunContainingXCoord(int x, int* offset) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);
  SelectionModel LastSelectionModelInsideRun(
      const internal::TextRunHarfBuzz* run);

  // Break the text into logical runs and populate the visual <-> logical maps.
  void ItemizeText();

  // Shape the glyphs needed for the text |run|.
  void ShapeRun(internal::TextRunHarfBuzz* run);

  // Text runs in logical order.
  ScopedVector<internal::TextRunHarfBuzz> runs_;

  // Maps visual run indices to logical run indices and vice versa.
  std::vector<int32_t> visual_to_logical_;
  std::vector<int32_t> logical_to_visual_;

  bool needs_layout_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextHarfBuzz);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_HARFBUZZ_H_
