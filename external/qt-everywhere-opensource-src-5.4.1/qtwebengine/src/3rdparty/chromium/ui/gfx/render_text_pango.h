// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_PANGO_H_
#define UI_GFX_RENDER_TEXT_PANGO_H_

#include <pango/pango.h>
#include <vector>

#include "ui/gfx/render_text.h"

namespace gfx {

// RenderTextPango is the Linux implementation of RenderText using Pango.
class RenderTextPango : public RenderText {
 public:
  RenderTextPango();
  virtual ~RenderTextPango();

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
  friend class RenderTextTest;
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, PangoAttributes);

  // Returns the run that contains the character attached to the caret in the
  // given selection model. Return NULL if not found.
  GSList* GetRunContainingCaret(const SelectionModel& caret) const;

  // Given a |run|, returns the SelectionModel that contains the logical first
  // or last caret position inside (not at a boundary of) the run.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel FirstSelectionModelInsideRun(const PangoItem* run);
  SelectionModel LastSelectionModelInsideRun(const PangoItem* run);

  // Setup pango attribute: foreground, background, font, strike.
  void SetupPangoAttributes(PangoLayout* layout);

  // Append one pango attribute |pango_attr| into pango attribute list |attrs|.
  void AppendPangoAttribute(size_t start,
                            size_t end,
                            PangoAttribute* pango_attr,
                            PangoAttrList* attrs);

  // Get the text index corresponding to the |run|'s |glyph_index|.
  size_t GetGlyphTextIndex(PangoLayoutRun* run, int glyph_index) const;

  // Pango Layout.
  PangoLayout* layout_;
  // A single line layout resulting from laying out via |layout_|.
  PangoLayoutLine* current_line_;

  // Information about character attributes.
  PangoLogAttr* log_attrs_;
  // Number of attributes in |log_attrs_|.
  int num_log_attrs_;

  // The text in the |layout_|.
  const char* layout_text_;

  DISALLOW_COPY_AND_ASSIGN(RenderTextPango);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_PANGO_H_
