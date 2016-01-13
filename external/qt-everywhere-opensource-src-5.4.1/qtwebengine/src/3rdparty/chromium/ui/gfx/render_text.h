// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_RENDER_TEXT_H_
#define UI_GFX_RENDER_TEXT_H_

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/break_list.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/point.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/size_f.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector2d.h"

class SkCanvas;
class SkDrawLooper;
struct SkPoint;
class SkShader;
class SkTypeface;

namespace gfx {

class Canvas;
class Font;
class RenderTextTest;

namespace internal {

// Internal helper class used by derived classes to draw text through Skia.
class SkiaTextRenderer {
 public:
  explicit SkiaTextRenderer(Canvas* canvas);
  ~SkiaTextRenderer();

  void SetDrawLooper(SkDrawLooper* draw_looper);
  void SetFontSmoothingSettings(bool antialiasing,
                                bool subpixel_rendering,
                                bool subpixel_positioning);
  void SetFontHinting(SkPaint::Hinting hinting);
  void SetTypeface(SkTypeface* typeface);
  void SetTextSize(SkScalar size);
  void SetFontFamilyWithStyle(const std::string& family, int font_style);
  void SetForegroundColor(SkColor foreground);
  void SetShader(SkShader* shader, const Rect& bounds);
  // Sets underline metrics to use if the text will be drawn with an underline.
  // If not set, default values based on the size of the text will be used. The
  // two metrics must be set together.
  void SetUnderlineMetrics(SkScalar thickness, SkScalar position);
  void DrawSelection(const std::vector<Rect>& selection, SkColor color);
  void DrawPosText(const SkPoint* pos,
                   const uint16* glyphs,
                   size_t glyph_count);
  // Draw underline and strike-through text decorations.
  // Based on |SkCanvas::DrawTextDecorations()| and constants from:
  //   third_party/skia/src/core/SkTextFormatParams.h
  void DrawDecorations(int x, int y, int width, bool underline, bool strike,
                       bool diagonal_strike);
  // Finishes any ongoing diagonal strike run.
  void EndDiagonalStrike();
  void DrawUnderline(int x, int y, int width);
  void DrawStrike(int x, int y, int width) const;

 private:
  // Helper class to draw a diagonal line with multiple pieces of different
  // lengths and colors; to support text selection appearances.
  class DiagonalStrike {
   public:
    DiagonalStrike(Canvas* canvas, Point start, const SkPaint& paint);
    ~DiagonalStrike();

    void AddPiece(int length, SkColor color);
    void Draw();

   private:
    typedef std::pair<int, SkColor> Piece;

    Canvas* canvas_;
    SkMatrix matrix_;
    const Point start_;
    SkPaint paint_;
    int total_length_;
    std::vector<Piece> pieces_;

    DISALLOW_COPY_AND_ASSIGN(DiagonalStrike);
  };

  Canvas* canvas_;
  SkCanvas* canvas_skia_;
  bool started_drawing_;
  SkPaint paint_;
  SkRect bounds_;
  skia::RefPtr<SkShader> deferred_fade_shader_;
  SkScalar underline_thickness_;
  SkScalar underline_position_;
  scoped_ptr<DiagonalStrike> diagonal_;

  DISALLOW_COPY_AND_ASSIGN(SkiaTextRenderer);
};

// Internal helper class used by derived classes to iterate colors and styles.
class StyleIterator {
 public:
  StyleIterator(const BreakList<SkColor>& colors,
                const std::vector<BreakList<bool> >& styles);
  ~StyleIterator();

  // Get the colors and styles at the current iterator position.
  SkColor color() const { return color_->second; }
  bool style(TextStyle s) const { return style_[s]->second; }

  // Get the intersecting range of the current iterator set.
  Range GetRange() const;

  // Update the iterator to point to colors and styles applicable at |position|.
  void UpdatePosition(size_t position);

 private:
  BreakList<SkColor> colors_;
  std::vector<BreakList<bool> > styles_;

  BreakList<SkColor>::const_iterator color_;
  std::vector<BreakList<bool>::const_iterator> style_;

  DISALLOW_COPY_AND_ASSIGN(StyleIterator);
};

// Line segments are slices of the layout text to be rendered on a single line.
struct LineSegment {
  LineSegment();
  ~LineSegment();

  // X coordinates of this line segment in text space.
  Range x_range;

  // The character range this segment corresponds to.
  Range char_range;

  // Index of the text run that generated this segment.
  size_t run;
};

// A line of layout text, comprised of a line segment list and some metrics.
struct Line {
  Line();
  ~Line();

  // Segments that make up this line in visual order.
  std::vector<LineSegment> segments;

  // A line size is the sum of segment widths and the maximum of segment
  // heights.
  Size size;

  // Sum of preceding lines' heights.
  int preceding_heights;

  // Maximum baseline of all segments on this line.
  int baseline;
};

// Creates an SkTypeface from a font |family| name and a |gfx::Font::FontStyle|.
skia::RefPtr<SkTypeface> CreateSkiaTypeface(const std::string& family,
                                            int style);

}  // namespace internal

// RenderText represents an abstract model of styled text and its corresponding
// visual layout. Support is built in for a cursor, a selection, simple styling,
// complex scripts, and bi-directional text. Implementations provide mechanisms
// for rendering and translation between logical and visual data.
class GFX_EXPORT RenderText {
 public:
  virtual ~RenderText();

  // Creates a platform-specific or cross-platform RenderText instance.
  static RenderText* CreateInstance();

  const base::string16& text() const { return text_; }
  void SetText(const base::string16& text);

  HorizontalAlignment horizontal_alignment() const {
    return horizontal_alignment_;
  }
  void SetHorizontalAlignment(HorizontalAlignment alignment);

  const FontList& font_list() const { return font_list_; }
  void SetFontList(const FontList& font_list);

  bool cursor_enabled() const { return cursor_enabled_; }
  void SetCursorEnabled(bool cursor_enabled);

  bool cursor_visible() const { return cursor_visible_; }
  void set_cursor_visible(bool visible) { cursor_visible_ = visible; }

  bool insert_mode() const { return insert_mode_; }
  void ToggleInsertMode();

  SkColor cursor_color() const { return cursor_color_; }
  void set_cursor_color(SkColor color) { cursor_color_ = color; }

  SkColor selection_color() const { return selection_color_; }
  void set_selection_color(SkColor color) { selection_color_ = color; }

  SkColor selection_background_focused_color() const {
    return selection_background_focused_color_;
  }
  void set_selection_background_focused_color(SkColor color) {
    selection_background_focused_color_ = color;
  }

  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }

  bool clip_to_display_rect() const { return clip_to_display_rect_; }
  void set_clip_to_display_rect(bool clip) { clip_to_display_rect_ = clip; }

  // In an obscured (password) field, all text is drawn as asterisks or bullets.
  bool obscured() const { return obscured_; }
  void SetObscured(bool obscured);

  // Makes a char in obscured text at |index| to be revealed. |index| should be
  // a UTF16 text index. If there is a previous revealed index, the previous one
  // is cleared and only the last set index will be revealed. If |index| is -1
  // or out of range, no char will be revealed. The revealed index is also
  // cleared when SetText or SetObscured is called.
  void SetObscuredRevealIndex(int index);

  // TODO(ckocagil): Multiline text rendering is currently only supported on
  // Windows. Support other platforms.
  bool multiline() const { return multiline_; }
  void SetMultiline(bool multiline);

  // Set the maximum length of the displayed layout text, not the actual text.
  // A |length| of 0 forgoes a hard limit, but does not guarantee proper
  // functionality of very long strings. Applies to subsequent SetText calls.
  // WARNING: Only use this for system limits, it lacks complex text support.
  void set_truncate_length(size_t length) { truncate_length_ = length; }

  // Elides the text to fit in |display_rect| according to the specified
  // |elide_behavior|. |ELIDE_MIDDLE| is not supported. If a truncate length and
  // an elide mode are specified, the shorter of the two will be applicable.
  void SetElideBehavior(ElideBehavior elide_behavior);

  const Rect& display_rect() const { return display_rect_; }
  void SetDisplayRect(const Rect& r);

  bool background_is_transparent() const { return background_is_transparent_; }
  void set_background_is_transparent(bool transparent) {
    background_is_transparent_ = transparent;
  }

  const SelectionModel& selection_model() const { return selection_model_; }

  const Range& selection() const { return selection_model_.selection(); }

  size_t cursor_position() const { return selection_model_.caret_pos(); }
  void SetCursorPosition(size_t position);

  // Moves the cursor left or right. Cursor movement is visual, meaning that
  // left and right are relative to screen, not the directionality of the text.
  // If |select| is false, the selection start is moved to the same position.
  void MoveCursor(BreakType break_type,
                  VisualCursorDirection direction,
                  bool select);

  // Set the selection_model_ to the value of |selection|.
  // The selection range is clamped to text().length() if out of range.
  // Returns true if the cursor position or selection range changed.
  // If any index in |selection_model| is not a cursorable position (not on a
  // grapheme boundary), it is a no-op and returns false.
  bool MoveCursorTo(const SelectionModel& selection_model);

  // Set the selection_model_ based on |range|.
  // If the |range| start or end is greater than text length, it is modified
  // to be the text length.
  // If the |range| start or end is not a cursorable position (not on grapheme
  // boundary), it is a NO-OP and returns false. Otherwise, returns true.
  bool SelectRange(const Range& range);

  // Returns true if the local point is over selected text.
  bool IsPointInSelection(const Point& point);

  // Selects no text, keeping the current cursor position and caret affinity.
  void ClearSelection();

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  void SelectAll(bool reversed);

  // Selects the word at the current cursor position. If there is a non-empty
  // selection, the selection bounds are extended to their nearest word
  // boundaries.
  void SelectWord();

  const Range& GetCompositionRange() const;
  void SetCompositionRange(const Range& composition_range);

  // Set the text color over the entire text or a logical character range.
  // The |range| should be valid, non-reversed, and within [0, text().length()].
  void SetColor(SkColor value);
  void ApplyColor(SkColor value, const Range& range);

  // Set various text styles over the entire text or a logical character range.
  // The respective |style| is applied if |value| is true, or removed if false.
  // The |range| should be valid, non-reversed, and within [0, text().length()].
  void SetStyle(TextStyle style, bool value);
  void ApplyStyle(TextStyle style, bool value, const Range& range);

  // Returns whether this style is enabled consistently across the entire
  // RenderText.
  bool GetStyle(TextStyle style) const;

  // Set or get the text directionality mode and get the text direction yielded.
  void SetDirectionalityMode(DirectionalityMode mode);
  DirectionalityMode directionality_mode() const {
      return directionality_mode_;
  }
  base::i18n::TextDirection GetTextDirection();

  // Returns the visual movement direction corresponding to the logical end
  // of the text, considering only the dominant direction returned by
  // |GetTextDirection()|, not the direction of a particular run.
  VisualCursorDirection GetVisualDirectionOfLogicalEnd();

  // Returns the size required to display the current string (which is the
  // wrapped size in multiline mode). The returned size does not include space
  // reserved for the cursor or the offset text shadows.
  virtual Size GetStringSize() = 0;

  // This is same as GetStringSize except that fractional size is returned.
  // The default implementation is same as GetStringSize. Certain platforms that
  // compute the text size as floating-point values, like Mac, will override
  // this method.
  // See comment in Canvas::GetStringWidthF for its usage.
  virtual SizeF GetStringSizeF();

  // Returns the width of the content (which is the wrapped width in multiline
  // mode). Reserves room for the cursor if |cursor_enabled_| is true.
  int GetContentWidth();

  // Returns the common baseline of the text. The return value is the vertical
  // offset from the top of |display_rect_| to the text baseline, in pixels.
  // The baseline is determined from the font list and display rect, and does
  // not depend on the text.
  int GetBaseline();

  void Draw(Canvas* canvas);

  // Draws a cursor at |position|.
  void DrawCursor(Canvas* canvas, const SelectionModel& position);

  // Gets the SelectionModel from a visual point in local coordinates.
  virtual SelectionModel FindCursorPosition(const Point& point) = 0;

  // Returns true if the position is a valid logical index into text(), and is
  // also a valid grapheme boundary, which may be used as a cursor position.
  virtual bool IsValidCursorIndex(size_t index) = 0;

  // Returns true if the position is a valid logical index into text(). Indices
  // amid multi-character graphemes are allowed here, unlike IsValidCursorIndex.
  virtual bool IsValidLogicalIndex(size_t index);

  // Get the visual bounds of a cursor at |caret|. These bounds typically
  // represent a vertical line if |insert_mode| is true. Pass false for
  // |insert_mode| to retrieve the bounds of the associated glyph. These bounds
  // are in local coordinates, but may be outside the visible region if the text
  // is longer than the textfield. Subsequent text, cursor, or bounds changes
  // may invalidate returned values. Note that |caret| must be placed at
  // grapheme boundary, i.e. caret.caret_pos() must be a cursorable position.
  Rect GetCursorBounds(const SelectionModel& caret, bool insert_mode);

  // Compute the current cursor bounds, panning the text to show the cursor in
  // the display rect if necessary. These bounds are in local coordinates.
  // Subsequent text, cursor, or bounds changes may invalidate returned values.
  const Rect& GetUpdatedCursorBounds();

  // Given an |index| in text(), return the next or previous grapheme boundary
  // in logical order (i.e. the nearest cursorable index). The return value is
  // in the range 0 to text().length() inclusive (the input is clamped if it is
  // out of that range). Always moves by at least one character index unless the
  // supplied index is already at the boundary of the string.
  size_t IndexOfAdjacentGrapheme(size_t index,
                                 LogicalCursorDirection direction);

  // Return a SelectionModel with the cursor at the current selection's start.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetSelectionModelForSelectionStart();

  // Sets shadows to drawn with text.
  void set_shadows(const ShadowValues& shadows) { shadows_ = shadows; }

  typedef std::pair<Font, Range> FontSpan;
  // For testing purposes, returns which fonts were chosen for which parts of
  // the text by returning a vector of Font and Range pairs, where each range
  // specifies the character range for which the corresponding font has been
  // chosen.
  virtual std::vector<FontSpan> GetFontSpansForTesting() = 0;

  // Gets the horizontal bounds (relative to the left of the text, not the view)
  // of the glyph starting at |index|. If the glyph is RTL then the returned
  // Range will have is_reversed() true.  (This does not return a Rect because a
  // Rect can't have a negative width.)
  virtual Range GetGlyphBounds(size_t index) = 0;

 protected:
  RenderText();

  const BreakList<SkColor>& colors() const { return colors_; }
  const std::vector<BreakList<bool> >& styles() const { return styles_; }

  const std::vector<internal::Line>& lines() const { return lines_; }
  void set_lines(std::vector<internal::Line>* lines) { lines_.swap(*lines); }

  // Returns the baseline of the current text.  The return value depends on
  // the text and its layout while the return value of GetBaseline() doesn't.
  // GetAlignmentOffset() takes into account the difference between them.
  //
  // We'd like a RenderText to show the text always on the same baseline
  // regardless of the text, so the text does not jump up or down depending
  // on the text.  However, underlying layout engines return different baselines
  // depending on the text.  In general, layout engines determine the minimum
  // bounding box for the text and return the baseline from the top of the
  // bounding box.  So the baseline changes depending on font metrics used to
  // layout the text.
  //
  // For example, suppose there are FontA and FontB and the baseline of FontA
  // is smaller than the one of FontB.  If the text is laid out only with FontA,
  // then the baseline of FontA may be returned.  If the text includes some
  // characters which are laid out with FontB, then the baseline of FontB may
  // be returned.
  //
  // GetBaseline() returns the fixed baseline regardless of the text.
  // GetLayoutTextBaseline() returns the baseline determined by the underlying
  // layout engine, and it changes depending on the text.  GetAlignmentOffset()
  // returns the difference between them.
  virtual int GetLayoutTextBaseline() = 0;

  const Vector2d& GetUpdatedDisplayOffset();

  void set_cached_bounds_and_offset_valid(bool valid) {
    cached_bounds_and_offset_valid_ = valid;
  }

  // Get the selection model that visually neighbors |position| by |break_type|.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel GetAdjacentSelectionModel(const SelectionModel& current,
                                           BreakType break_type,
                                           VisualCursorDirection direction);

  // Get the selection model visually left/right of |selection| by one grapheme.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentCharSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the selection model visually left/right of |selection| by one word.
  // The returned value represents a cursor/caret position without a selection.
  virtual SelectionModel AdjacentWordSelectionModel(
      const SelectionModel& selection,
      VisualCursorDirection direction) = 0;

  // Get the SelectionModels corresponding to visual text ends.
  // The returned value represents a cursor/caret position without a selection.
  SelectionModel EdgeSelectionModel(VisualCursorDirection direction);

  // Sets the selection model, the argument is assumed to be valid.
  virtual void SetSelectionModel(const SelectionModel& model);

  // Get the visual bounds containing the logical substring within the |range|.
  // If |range| is empty, the result is empty. These bounds could be visually
  // discontinuous if the substring is split by a LTR/RTL level change.
  // These bounds are in local coordinates, but may be outside the visible
  // region if the text is longer than the textfield. Subsequent text, cursor,
  // or bounds changes may invalidate returned values.
  virtual std::vector<Rect> GetSubstringBounds(const Range& range) = 0;

  // Convert between indices into |text_| and indices into |obscured_text_|,
  // which differ when the text is obscured. Regardless of whether or not the
  // text is obscured, the character (code point) offsets always match.
  virtual size_t TextIndexToLayoutIndex(size_t index) const = 0;
  virtual size_t LayoutIndexToTextIndex(size_t index) const = 0;

  // Reset the layout to be invalid.
  virtual void ResetLayout() = 0;

  // Ensure the text is laid out, lines are computed, and |lines_| is valid.
  virtual void EnsureLayout() = 0;

  // Draw the text.
  virtual void DrawVisualText(Canvas* canvas) = 0;

  // Returns the text used for layout, which may be obscured or truncated.
  const base::string16& GetLayoutText() const;

  // Returns layout text positions that are suitable for breaking lines.
  const BreakList<size_t>& GetLineBreaks();

  // Apply (and undo) temporary composition underlines and selection colors.
  void ApplyCompositionAndSelectionStyles();
  void UndoCompositionAndSelectionStyles();

  // Returns the line offset from the origin after applying the text alignment
  // and the display offset.
  Vector2d GetLineOffset(size_t line_number);

  // Convert points from the text space to the view space and back. Handles the
  // display area, display offset, application LTR/RTL mode and multiline.
  Point ToTextPoint(const Point& point);
  Point ToViewPoint(const Point& point);

  // Convert a text space x-coordinate range to rects in view space.
  std::vector<Rect> TextBoundsToViewBounds(const Range& x);

  // Returns the line offset from the origin, accounts for text alignment only.
  Vector2d GetAlignmentOffset(size_t line_number);

  // Applies fade effects to |renderer|.
  void ApplyFadeEffects(internal::SkiaTextRenderer* renderer);

  // Applies text shadows to |renderer|.
  void ApplyTextShadows(internal::SkiaTextRenderer* renderer);

  // A convenience function to check whether the glyph attached to the caret
  // is within the given range.
  static bool RangeContainsCaret(const Range& range,
                                 size_t caret_pos,
                                 LogicalCursorDirection caret_affinity);

 private:
  friend class RenderTextTest;
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, DefaultStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, SetColorAndStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ApplyColorAndStyle);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ObscuredText);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, RevealObscuredText);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ElidedText);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, ElidedObscuredText);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, TruncatedText);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, TruncatedObscuredText);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, GraphemePositions);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, EdgeSelectionModels);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, GetTextOffset);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, GetTextOffsetHorizontalDefaultInRTL);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_MinWidth);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_NormalWidth);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_SufficientWidth);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, Multiline_Newline);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, GlyphBounds);
  FRIEND_TEST_ALL_PREFIXES(RenderTextTest, HarfBuzz_GlyphBounds);

  // Creates a platform-specific RenderText instance.
  static RenderText* CreateNativeInstance();

  // Set the cursor to |position|, with the caret trailing the previous
  // grapheme, or if there is no previous grapheme, leading the cursor position.
  // If |select| is false, the selection start is moved to the same position.
  // If the |position| is not a cursorable position (not on grapheme boundary),
  // it is a NO-OP.
  void MoveCursorTo(size_t position, bool select);

  // Updates |layout_text_| if the text is obscured or truncated.
  void UpdateLayoutText();

  // Elides |text| to fit in the |display_rect_| with given |elide_behavior_|.
  // See ElideText in ui/gfx/text_elider.cc for reference.
  base::string16 ElideText(const base::string16& text);

  // Update the cached bounds and display offset to ensure that the current
  // cursor is within the visible display area.
  void UpdateCachedBoundsAndOffset();

  // Draw the selection.
  void DrawSelection(Canvas* canvas);

  // Logical UTF-16 string data to be drawn.
  base::string16 text_;

  // Horizontal alignment of the text with respect to |display_rect_|.  The
  // default is to align left if the application UI is LTR and right if RTL.
  HorizontalAlignment horizontal_alignment_;

  // The text directionality mode, defaults to DIRECTIONALITY_FROM_TEXT.
  DirectionalityMode directionality_mode_;

  // The cached text direction, potentially computed from the text or UI locale.
  // Use GetTextDirection(), do not use this potentially invalid value directly!
  base::i18n::TextDirection text_direction_;

  // A list of fonts used to render |text_|.
  FontList font_list_;

  // Logical selection range and visual cursor position.
  SelectionModel selection_model_;

  // The cached cursor bounds; get these bounds with GetUpdatedCursorBounds.
  Rect cursor_bounds_;

  // Specifies whether the cursor is enabled. If disabled, no space is reserved
  // for the cursor when positioning text.
  bool cursor_enabled_;

  // The cursor visibility and insert mode.
  bool cursor_visible_;
  bool insert_mode_;

  // The color used for the cursor.
  SkColor cursor_color_;

  // The color used for drawing selected text.
  SkColor selection_color_;

  // The background color used for drawing the selection when focused.
  SkColor selection_background_focused_color_;

  // The focus state of the text.
  bool focused_;

  // Composition text range.
  Range composition_range_;

  // Color and style breaks, used to color and stylize ranges of text.
  // BreakList positions are stored with text indices, not layout indices.
  // TODO(msw): Expand to support cursor, selection, background, etc. colors.
  BreakList<SkColor> colors_;
  std::vector<BreakList<bool> > styles_;

  // Breaks saved without temporary composition and selection styling.
  BreakList<SkColor> saved_colors_;
  BreakList<bool> saved_underlines_;
  bool composition_and_selection_styles_applied_;

  // A flag to obscure actual text with asterisks for password fields.
  bool obscured_;
  // The index at which the char should be revealed in the obscured text.
  int obscured_reveal_index_;

  // The maximum length of text to display, 0 forgoes a hard limit.
  size_t truncate_length_;

  // The behavior for eliding, fading, or truncating.
  ElideBehavior elide_behavior_;

  // The obscured and/or truncated text that will be displayed.
  base::string16 layout_text_;

  // Whether the text should be broken into multiple lines. Uses the width of
  // |display_rect_| as the width cap.
  bool multiline_;

  // Is the background transparent (either partially or fully)?
  bool background_is_transparent_;

  // The local display area for rendering the text.
  Rect display_rect_;

  // Flag to work around a Skia bug with the PDF path (http://crbug.com/133548)
  // that results in incorrect clipping when drawing to the document margins.
  // This field allows disabling clipping to work around the issue.
  // TODO(asvitkine): Remove this when the underlying Skia bug is fixed.
  bool clip_to_display_rect_;

  // The offset for the text to be drawn, relative to the display area.
  // Get this point with GetUpdatedDisplayOffset (or risk using a stale value).
  Vector2d display_offset_;

  // The baseline of the text.  This is determined from the height of the
  // display area and the cap height of the font list so the text is vertically
  // centered.
  int baseline_;

  // The cached bounds and offset are invalidated by changes to the cursor,
  // selection, font, and other operations that adjust the visible text bounds.
  bool cached_bounds_and_offset_valid_;

  // Text shadows to be drawn.
  ShadowValues shadows_;

  // A list of valid layout text line break positions.
  BreakList<size_t> line_breaks_;

  // Lines computed by EnsureLayout. These should be invalidated with
  // ResetLayout and on |display_rect_| changes.
  std::vector<internal::Line> lines_;

  DISALLOW_COPY_AND_ASSIGN(RenderText);
};

}  // namespace gfx

#endif  // UI_GFX_RENDER_TEXT_H_
