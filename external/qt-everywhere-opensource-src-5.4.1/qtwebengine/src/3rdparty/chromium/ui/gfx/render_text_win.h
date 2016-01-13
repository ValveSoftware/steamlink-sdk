// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_WIN_H_
#define UI_GFX_RENDER_TEXT_WIN_H_

#include <usp10.h>

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/render_text.h"

namespace gfx {

namespace internal {

struct TextRun {
  TextRun();
  ~TextRun();

  Range range;
  Font font;
  // A gfx::Font::FontStyle flag to specify bold and italic styles.
  // Supersedes |font.GetFontStyle()|. Stored separately to avoid calling
  // |font.DeriveFont()|, which is expensive on Windows.
  int font_style;

  bool strike;
  bool diagonal_strike;
  bool underline;

  int width;
  // The cumulative widths of preceding runs.
  int preceding_run_widths;

  SCRIPT_ANALYSIS script_analysis;

  scoped_ptr<WORD[]> glyphs;
  scoped_ptr<WORD[]> logical_clusters;
  scoped_ptr<SCRIPT_VISATTR[]> visible_attributes;
  int glyph_count;

  scoped_ptr<int[]> advance_widths;
  scoped_ptr<GOFFSET[]> offsets;
  ABC abc_widths;
  SCRIPT_CACHE script_cache;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextRun);
};

}  // namespace internal

// RenderTextWin is the Windows implementation of RenderText using Uniscribe.
class RenderTextWin : public RenderText {
 public:
  RenderTextWin();
  virtual ~RenderTextWin();

  // Overridden from RenderText:
  virtual Size GetStringSize() OVERRIDE;
  virtual SelectionModel FindCursorPosition(const Point& point) OVERRIDE;
  virtual std::vector<FontSpan> GetFontSpansForTesting() OVERRIDE;

 protected:
  // Overridden from RenderText:
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
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Win_BreakRunsByUnicodeBlocks);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Win_LogicalClusters);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_MinWidth);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_NormalWidth);

  void ItemizeLogicalText();
  void LayoutVisualText();
  void LayoutTextRun(internal::TextRun* run);

  // Helper function that calls |ScriptShape()| on the run, which has logic to
  // handle E_OUTOFMEMORY return codes.
  HRESULT ShapeTextRunWithFont(internal::TextRun* run, const Font& font);

  // Returns the number of characters in |run| that have missing glyphs.
  int CountCharsWithMissingGlyphs(internal::TextRun* run) const;

  // Return the run index that contains the argument; or the length of the
  // |runs_| vector if argument exceeds the text length or width.
  size_t GetRunContainingCaret(const SelectionModel& caret) const;
  size_t GetRunContainingXCoord(int x) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(const internal::TextRun* run);
  SelectionModel LastSelectionModelInsideRun(const internal::TextRun* run);

  // Cached HDC for performing Uniscribe API calls.
  static HDC cached_hdc_;

  // Cached map from font name to the last successful substitute font used.
  // TODO(asvitkine): Move the caching logic to font_fallback_win.cc.
  static std::map<std::string, Font> successful_substitute_fonts_;

  SCRIPT_CONTROL script_control_;
  SCRIPT_STATE script_state_;

  ScopedVector<internal::TextRun> runs_;

  // Single line width of the layout text.
  int string_width_;

  // Wrapped multiline size of the layout text.
  Size multiline_string_size_;

  scoped_ptr<int[]> visual_to_logical_;
  scoped_ptr<int[]> logical_to_visual_;

  bool needs_layout_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextWin);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_WIN_H_
