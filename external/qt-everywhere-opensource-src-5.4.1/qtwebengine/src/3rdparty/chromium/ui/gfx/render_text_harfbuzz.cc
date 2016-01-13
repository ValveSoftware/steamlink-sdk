// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text_harfbuzz.h"

#include <map>

#include "base/debug/leak_annotations.h"
#include "base/i18n/bidi_line_iterator.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/char_iterator.h"
#include "base/lazy_instance.h"
#include "third_party/harfbuzz-ng/src/hb.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/utf16_indexing.h"

#if defined(OS_WIN)
#include "ui/gfx/font_smoothing_win.h"
#endif

namespace gfx {

namespace {

// The maximum number of scripts a Unicode character can belong to. This value
// is arbitrarily chosen to be a good limit because it is unlikely for a single
// character to belong to more scripts.
const size_t kMaxScripts = 5;

// Maps from code points to glyph indices in a font.
typedef std::map<uint32_t, uint16_t> GlyphCache;

// Font data provider for HarfBuzz using Skia. Copied from Blink.
// TODO(ckocagil): Eliminate the duplication. http://crbug.com/368375
struct FontData {
  FontData(GlyphCache* glyph_cache) : glyph_cache_(glyph_cache) {}

  SkPaint paint_;
  GlyphCache* glyph_cache_;
};

hb_position_t SkiaScalarToHarfBuzzPosition(SkScalar value) {
  return SkScalarToFixed(value);
}

// Deletes the object at the given pointer after casting it to the given type.
template<typename Type>
void DeleteByType(void* data) {
  Type* typed_data = reinterpret_cast<Type*>(data);
  delete typed_data;
}

template<typename Type>
void DeleteArrayByType(void* data) {
  Type* typed_data = reinterpret_cast<Type*>(data);
  delete[] typed_data;
}

// Outputs the |width| and |extents| of the glyph with index |codepoint| in
// |paint|'s font.
void GetGlyphWidthAndExtents(SkPaint* paint,
                             hb_codepoint_t codepoint,
                             hb_position_t* width,
                             hb_glyph_extents_t* extents) {
  DCHECK_LE(codepoint, 0xFFFFU);
  paint->setTextEncoding(SkPaint::kGlyphID_TextEncoding);

  SkScalar sk_width;
  SkRect sk_bounds;
  uint16_t glyph = codepoint;

  paint->getTextWidths(&glyph, sizeof(glyph), &sk_width, &sk_bounds);
  if (width)
    *width = SkiaScalarToHarfBuzzPosition(sk_width);
  if (extents) {
    // Invert y-axis because Skia is y-grows-down but we set up HarfBuzz to be
    // y-grows-up.
    extents->x_bearing = SkiaScalarToHarfBuzzPosition(sk_bounds.fLeft);
    extents->y_bearing = SkiaScalarToHarfBuzzPosition(-sk_bounds.fTop);
    extents->width = SkiaScalarToHarfBuzzPosition(sk_bounds.width());
    extents->height = SkiaScalarToHarfBuzzPosition(-sk_bounds.height());
  }
}

// Writes the |glyph| index for the given |unicode| code point. Returns whether
// the glyph exists, i.e. it is not a missing glyph.
hb_bool_t GetGlyph(hb_font_t* font,
                   void* data,
                   hb_codepoint_t unicode,
                   hb_codepoint_t variation_selector,
                   hb_codepoint_t* glyph,
                   void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  GlyphCache* cache = font_data->glyph_cache_;

  bool exists = cache->count(unicode) != 0;
  if (!exists) {
    SkPaint* paint = &font_data->paint_;
    paint->setTextEncoding(SkPaint::kUTF32_TextEncoding);
    paint->textToGlyphs(&unicode, sizeof(hb_codepoint_t), &(*cache)[unicode]);
  }
  *glyph = (*cache)[unicode];
  return !!*glyph;
}

// Returns the horizontal advance value of the |glyph|.
hb_position_t GetGlyphHorizontalAdvance(hb_font_t* font,
                                        void* data,
                                        hb_codepoint_t glyph,
                                        void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  hb_position_t advance = 0;

  GetGlyphWidthAndExtents(&font_data->paint_, glyph, &advance, 0);
  return advance;
}

hb_bool_t GetGlyphHorizontalOrigin(hb_font_t* font,
                                   void* data,
                                   hb_codepoint_t glyph,
                                   hb_position_t* x,
                                   hb_position_t* y,
                                   void* user_data) {
  // Just return true, like the HarfBuzz-FreeType implementation.
  return true;
}

hb_position_t GetGlyphKerning(FontData* font_data,
                              hb_codepoint_t first_glyph,
                              hb_codepoint_t second_glyph) {
  SkTypeface* typeface = font_data->paint_.getTypeface();
  const uint16_t glyphs[2] = { static_cast<uint16_t>(first_glyph),
                               static_cast<uint16_t>(second_glyph) };
  int32_t kerning_adjustments[1] = { 0 };

  if (!typeface->getKerningPairAdjustments(glyphs, 2, kerning_adjustments))
    return 0;

  SkScalar upm = SkIntToScalar(typeface->getUnitsPerEm());
  SkScalar size = font_data->paint_.getTextSize();
  return SkiaScalarToHarfBuzzPosition(
      SkScalarMulDiv(SkIntToScalar(kerning_adjustments[0]), size, upm));
}

hb_position_t GetGlyphHorizontalKerning(hb_font_t* font,
                                        void* data,
                                        hb_codepoint_t left_glyph,
                                        hb_codepoint_t right_glyph,
                                        void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  if (font_data->paint_.isVerticalText()) {
    // We don't support cross-stream kerning.
    return 0;
  }

  return GetGlyphKerning(font_data, left_glyph, right_glyph);
}

hb_position_t GetGlyphVerticalKerning(hb_font_t* font,
                                      void* data,
                                      hb_codepoint_t top_glyph,
                                      hb_codepoint_t bottom_glyph,
                                      void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);
  if (!font_data->paint_.isVerticalText()) {
    // We don't support cross-stream kerning.
    return 0;
  }

  return GetGlyphKerning(font_data, top_glyph, bottom_glyph);
}

// Writes the |extents| of |glyph|.
hb_bool_t GetGlyphExtents(hb_font_t* font,
                          void* data,
                          hb_codepoint_t glyph,
                          hb_glyph_extents_t* extents,
                          void* user_data) {
  FontData* font_data = reinterpret_cast<FontData*>(data);

  GetGlyphWidthAndExtents(&font_data->paint_, glyph, 0, extents);
  return true;
}

class FontFuncs {
 public:
  FontFuncs() : font_funcs_(hb_font_funcs_create()) {
    hb_font_funcs_set_glyph_func(font_funcs_, GetGlyph, 0, 0);
    hb_font_funcs_set_glyph_h_advance_func(
        font_funcs_, GetGlyphHorizontalAdvance, 0, 0);
    hb_font_funcs_set_glyph_h_kerning_func(
        font_funcs_, GetGlyphHorizontalKerning, 0, 0);
    hb_font_funcs_set_glyph_h_origin_func(
        font_funcs_, GetGlyphHorizontalOrigin, 0, 0);
    hb_font_funcs_set_glyph_v_kerning_func(
        font_funcs_, GetGlyphVerticalKerning, 0, 0);
    hb_font_funcs_set_glyph_extents_func(
        font_funcs_, GetGlyphExtents, 0, 0);
    hb_font_funcs_make_immutable(font_funcs_);
  }

  ~FontFuncs() {
    hb_font_funcs_destroy(font_funcs_);
  }

  hb_font_funcs_t* get() { return font_funcs_; }

 private:
  hb_font_funcs_t* font_funcs_;

  DISALLOW_COPY_AND_ASSIGN(FontFuncs);
};

base::LazyInstance<FontFuncs>::Leaky g_font_funcs = LAZY_INSTANCE_INITIALIZER;

// Returns the raw data of the font table |tag|.
hb_blob_t* GetFontTable(hb_face_t* face, hb_tag_t tag, void* user_data) {
  SkTypeface* typeface = reinterpret_cast<SkTypeface*>(user_data);

  const size_t table_size = typeface->getTableSize(tag);
  if (!table_size)
    return 0;

  scoped_ptr<char[]> buffer(new char[table_size]);
  if (!buffer)
    return 0;
  size_t actual_size = typeface->getTableData(tag, 0, table_size, buffer.get());
  if (table_size != actual_size)
    return 0;

  char* buffer_raw = buffer.release();
  return hb_blob_create(buffer_raw, table_size, HB_MEMORY_MODE_WRITABLE,
                        buffer_raw, DeleteArrayByType<char>);
}

void UnrefSkTypeface(void* data) {
  SkTypeface* skia_face = reinterpret_cast<SkTypeface*>(data);
  SkSafeUnref(skia_face);
}

// Creates a HarfBuzz face from the given Skia face.
hb_face_t* CreateHarfBuzzFace(SkTypeface* skia_face) {
  SkSafeRef(skia_face);
  hb_face_t* face = hb_face_create_for_tables(GetFontTable, skia_face,
                                              UnrefSkTypeface);
  DCHECK(face);
  return face;
}

// Creates a HarfBuzz font from the given Skia face and text size.
hb_font_t* CreateHarfBuzzFont(SkTypeface* skia_face, int text_size) {
  typedef std::pair<hb_face_t*, GlyphCache> FaceCache;

  // TODO(ckocagil): This shouldn't grow indefinitely. Maybe use base::MRUCache?
  static std::map<SkFontID, FaceCache> face_caches;

  FaceCache* face_cache = &face_caches[skia_face->uniqueID()];
  if (face_cache->first == 0) {
    // These HarfBuzz faces live indefinitely and are intentionally leaked.
    ANNOTATE_SCOPED_MEMORY_LEAK;
    hb_face_t* harfbuzz_face = CreateHarfBuzzFace(skia_face);
    *face_cache = FaceCache(harfbuzz_face, GlyphCache());
  }

  hb_font_t* harfbuzz_font = hb_font_create(face_cache->first);
  // TODO(ckocagil): Investigate whether disabling hinting here has any effect
  // on text quality.
  int upem = hb_face_get_upem(face_cache->first);
  hb_font_set_scale(harfbuzz_font, upem, upem);
  FontData* hb_font_data = new FontData(&face_cache->second);
  hb_font_data->paint_.setTypeface(skia_face);
  hb_font_data->paint_.setTextSize(text_size);
  hb_font_set_funcs(harfbuzz_font, g_font_funcs.Get().get(), hb_font_data,
                    DeleteByType<FontData>);
  hb_font_make_immutable(harfbuzz_font);
  return harfbuzz_font;
}

// Returns true if characters of |block_code| may trigger font fallback.
bool IsUnusualBlockCode(UBlockCode block_code) {
  return block_code == UBLOCK_GEOMETRIC_SHAPES ||
         block_code == UBLOCK_MISCELLANEOUS_SYMBOLS;
}

// Returns the index of the first unusual character after a usual character or
// vice versa. Unusual characters are defined by |IsUnusualBlockCode|.
size_t FindUnusualCharacter(const base::string16& text,
                            size_t run_start,
                            size_t run_break) {
  const int32 run_length = static_cast<int32>(run_break - run_start);
  base::i18n::UTF16CharIterator iter(text.c_str() + run_start,
                                     run_length);
  const UBlockCode first_block_code = ublock_getCode(iter.get());
  const bool first_block_unusual = IsUnusualBlockCode(first_block_code);
  while (iter.Advance() && iter.array_pos() < run_length) {
    const UBlockCode current_block_code = ublock_getCode(iter.get());
    if (current_block_code != first_block_code &&
        (first_block_unusual || IsUnusualBlockCode(current_block_code))) {
      return run_start + iter.array_pos();
    }
  }
  return run_break;
}

// If the given scripts match, returns the one that isn't USCRIPT_COMMON or
// USCRIPT_INHERITED, i.e. the more specific one. Otherwise returns
// USCRIPT_INVALID_CODE.
UScriptCode ScriptIntersect(UScriptCode first, UScriptCode second) {
  if (first == second ||
      (second > USCRIPT_INVALID_CODE && second <= USCRIPT_INHERITED)) {
    return first;
  }
  if (first > USCRIPT_INVALID_CODE && first <= USCRIPT_INHERITED)
    return second;
  return USCRIPT_INVALID_CODE;
}

// Writes the script and the script extensions of the character with the
// Unicode |codepoint|. Returns the number of written scripts.
int GetScriptExtensions(UChar32 codepoint, UScriptCode* scripts) {
  UErrorCode icu_error = U_ZERO_ERROR;
  // ICU documentation incorrectly states that the result of
  // |uscript_getScriptExtensions| will contain the regular script property.
  // Write the character's script property to the first element.
  scripts[0] = uscript_getScript(codepoint, &icu_error);
  if (U_FAILURE(icu_error))
    return 0;
  // Fill the rest of |scripts| with the extensions.
  int count = uscript_getScriptExtensions(codepoint, scripts + 1,
                                          kMaxScripts - 1, &icu_error);
  if (U_FAILURE(icu_error))
    count = 0;
  return count + 1;
}

// Intersects the script extensions set of |codepoint| with |result| and writes
// to |result|, reading and updating |result_size|.
void ScriptSetIntersect(UChar32 codepoint,
                        UScriptCode* result,
                        size_t* result_size) {
  UScriptCode scripts[kMaxScripts] = { USCRIPT_INVALID_CODE };
  int count = GetScriptExtensions(codepoint, scripts);

  size_t out_size = 0;

  for (size_t i = 0; i < *result_size; ++i) {
    for (int j = 0; j < count; ++j) {
      UScriptCode intersection = ScriptIntersect(result[i], scripts[j]);
      if (intersection != USCRIPT_INVALID_CODE) {
        result[out_size++] = intersection;
        break;
      }
    }
  }

  *result_size = out_size;
}

// Find the longest sequence of characters from 0 and up to |length| that
// have at least one common UScriptCode value. Writes the common script value to
// |script| and returns the length of the sequence. Takes the characters' script
// extensions into account. http://www.unicode.org/reports/tr24/#ScriptX
//
// Consider 3 characters with the script values {Kana}, {Hira, Kana}, {Kana}.
// Without script extensions only the first script in each set would be taken
// into account, resulting in 3 runs where 1 would be enough.
// TODO(ckocagil): Write a unit test for the case above.
int ScriptInterval(const base::string16& text,
                   size_t start,
                   size_t length,
                   UScriptCode* script) {
  DCHECK_GT(length, 0U);

  UScriptCode scripts[kMaxScripts] = { USCRIPT_INVALID_CODE };

  base::i18n::UTF16CharIterator char_iterator(text.c_str() + start, length);
  size_t scripts_size = GetScriptExtensions(char_iterator.get(), scripts);
  *script = scripts[0];

  while (char_iterator.Advance()) {
    ScriptSetIntersect(char_iterator.get(), scripts, &scripts_size);
    if (scripts_size == 0U)
      return char_iterator.array_pos();
    *script = scripts[0];
  }

  return length;
}

// A port of hb_icu_script_to_script because harfbuzz on CrOS is built without
// hb-icu. See http://crbug.com/356929
inline hb_script_t ICUScriptToHBScript(UScriptCode script) {
  if (script == USCRIPT_INVALID_CODE)
    return HB_SCRIPT_INVALID;
  return hb_script_from_string(uscript_getShortName(script), -1);
}

}  // namespace

namespace internal {

TextRunHarfBuzz::TextRunHarfBuzz()
    : width(0),
      preceding_run_widths(0),
      is_rtl(false),
      level(0),
      script(USCRIPT_INVALID_CODE),
      glyph_count(-1),
      font_size(0),
      font_style(0),
      strike(false),
      diagonal_strike(false),
      underline(false) {}

TextRunHarfBuzz::~TextRunHarfBuzz() {}

size_t TextRunHarfBuzz::CharToGlyph(size_t pos) const {
  DCHECK(range.start() <= pos && pos < range.end());

  if (!is_rtl) {
    size_t cluster_start = 0;
    for (size_t i = 1; i < glyph_count && pos >= glyph_to_char[i]; ++i)
      if (glyph_to_char[i] != glyph_to_char[i - 1])
        cluster_start = i;
    return cluster_start;
  }

  for (size_t i = 0; i < glyph_count; ++i) {
    if (pos >= glyph_to_char[i])
      return i;
  }
  NOTREACHED();
  return 0;
}

Range TextRunHarfBuzz::CharRangeToGlyphRange(const Range& char_range) const {
  DCHECK(range.Contains(char_range));
  DCHECK(!char_range.is_reversed());
  DCHECK(!char_range.is_empty());

  size_t first = 0;
  size_t last = 0;

  if (is_rtl) {
    // For RTL runs, we subtract 1 from |char_range| to get the leading edges.
    last = CharToGlyph(char_range.end() - 1);
    // Loop until we find a non-empty glyph range. For multi-character clusters,
    // the loop is needed to find the cluster end. Do the same for LTR below.
    for (size_t i = char_range.start(); i > range.start(); --i) {
      first = CharToGlyph(i - 1);
      if (first != last)
        return Range(last, first);
    }
    return Range(last, glyph_count);
  }

  first = CharToGlyph(char_range.start());
  for (size_t i = char_range.end(); i < range.end(); ++i) {
    last = CharToGlyph(i);
    if (first != last)
      return Range(first, last);
  }
  return Range(first, glyph_count);
}

// Returns whether the given shaped run contains any missing glyphs.
bool TextRunHarfBuzz::HasMissingGlyphs() const {
  static const int kMissingGlyphId = 0;
  for (size_t i = 0; i < glyph_count; ++i) {
    if (glyphs[i] == kMissingGlyphId)
      return true;
  }
  return false;
}

int TextRunHarfBuzz::GetGlyphXBoundary(size_t text_index, bool trailing) const {
  if (text_index == range.end()) {
    trailing = true;
    --text_index;
  }
  Range glyph_range = CharRangeToGlyphRange(Range(text_index, text_index + 1));
  const size_t glyph_pos = (is_rtl == trailing) ?
      glyph_range.start() : glyph_range.end();
  const int x = glyph_pos < glyph_count ?
      SkScalarRoundToInt(positions[glyph_pos].x()) : width;
  return preceding_run_widths + x;
}

}  // namespace internal

RenderTextHarfBuzz::RenderTextHarfBuzz()
    : RenderText(),
      needs_layout_(false) {}

RenderTextHarfBuzz::~RenderTextHarfBuzz() {}

Size RenderTextHarfBuzz::GetStringSize() {
  EnsureLayout();
  return lines()[0].size;
}

SelectionModel RenderTextHarfBuzz::FindCursorPosition(const Point& point) {
  EnsureLayout();

  int x = ToTextPoint(point).x();
  int offset = 0;
  size_t run_index = GetRunContainingXCoord(x, &offset);
  if (run_index >= runs_.size())
    return EdgeSelectionModel((x < 0) ? CURSOR_LEFT : CURSOR_RIGHT);
  const internal::TextRunHarfBuzz& run = *runs_[run_index];

  for (size_t i = 0; i < run.glyph_count; ++i) {
    const SkScalar end =
        i + 1 == run.glyph_count ? run.width : run.positions[i + 1].x();
    const SkScalar middle = (end + run.positions[i].x()) / 2;

    if (offset < middle) {
      return SelectionModel(LayoutIndexToTextIndex(
          run.glyph_to_char[i] + (run.is_rtl ? 1 : 0)),
          (run.is_rtl ? CURSOR_BACKWARD : CURSOR_FORWARD));
    }
    if (offset < end) {
      return SelectionModel(LayoutIndexToTextIndex(
          run.glyph_to_char[i] + (run.is_rtl ? 0 : 1)),
          (run.is_rtl ? CURSOR_FORWARD : CURSOR_BACKWARD));
    }
  }
  return EdgeSelectionModel(CURSOR_RIGHT);
}

std::vector<RenderText::FontSpan> RenderTextHarfBuzz::GetFontSpansForTesting() {
  NOTIMPLEMENTED();
  return std::vector<RenderText::FontSpan>();
}

int RenderTextHarfBuzz::GetLayoutTextBaseline() {
  EnsureLayout();
  return lines()[0].baseline;
}

SelectionModel RenderTextHarfBuzz::AdjacentCharSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  DCHECK(!needs_layout_);
  internal::TextRunHarfBuzz* run;
  size_t run_index = GetRunContainingCaret(selection);
  if (run_index >= runs_.size()) {
    // The cursor is not in any run: we're at the visual and logical edge.
    SelectionModel edge = EdgeSelectionModel(direction);
    if (edge.caret_pos() == selection.caret_pos())
      return edge;
    int visual_index = (direction == CURSOR_RIGHT) ? 0 : runs_.size() - 1;
    run = runs_[visual_to_logical_[visual_index]];
  } else {
    // If the cursor is moving within the current run, just move it by one
    // grapheme in the appropriate direction.
    run = runs_[run_index];
    size_t caret = selection.caret_pos();
    bool forward_motion = run->is_rtl == (direction == CURSOR_LEFT);
    if (forward_motion) {
      if (caret < LayoutIndexToTextIndex(run->range.end())) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_FORWARD);
        return SelectionModel(caret, CURSOR_BACKWARD);
      }
    } else {
      if (caret > LayoutIndexToTextIndex(run->range.start())) {
        caret = IndexOfAdjacentGrapheme(caret, CURSOR_BACKWARD);
        return SelectionModel(caret, CURSOR_FORWARD);
      }
    }
    // The cursor is at the edge of a run; move to the visually adjacent run.
    int visual_index = logical_to_visual_[run_index];
    visual_index += (direction == CURSOR_LEFT) ? -1 : 1;
    if (visual_index < 0 || visual_index >= static_cast<int>(runs_.size()))
      return EdgeSelectionModel(direction);
    run = runs_[visual_to_logical_[visual_index]];
  }
  bool forward_motion = run->is_rtl == (direction == CURSOR_LEFT);
  return forward_motion ? FirstSelectionModelInsideRun(run) :
                          LastSelectionModelInsideRun(run);
}

SelectionModel RenderTextHarfBuzz::AdjacentWordSelectionModel(
    const SelectionModel& selection,
    VisualCursorDirection direction) {
  // TODO(ckocagil): This implementation currently matches RenderTextWin, but it
  // should match the native behavior on other platforms.
  if (obscured())
    return EdgeSelectionModel(direction);

  base::i18n::BreakIterator iter(text(), base::i18n::BreakIterator::BREAK_WORD);
  bool success = iter.Init();
  DCHECK(success);
  if (!success)
    return selection;

  size_t pos;
  if (direction == CURSOR_RIGHT) {
    pos = std::min(selection.caret_pos() + 1, text().length());
    while (iter.Advance()) {
      pos = iter.pos();
      if (iter.IsWord() && pos > selection.caret_pos())
        break;
    }
  } else {  // direction == CURSOR_LEFT
    // Notes: We always iterate words from the beginning.
    // This is probably fast enough for our usage, but we may
    // want to modify WordIterator so that it can start from the
    // middle of string and advance backwards.
    pos = std::max<int>(selection.caret_pos() - 1, 0);
    while (iter.Advance()) {
      if (iter.IsWord()) {
        size_t begin = iter.pos() - iter.GetString().length();
        if (begin == selection.caret_pos()) {
          // The cursor is at the beginning of a word.
          // Move to previous word.
          break;
        } else if (iter.pos() >= selection.caret_pos()) {
          // The cursor is in the middle or at the end of a word.
          // Move to the top of current word.
          pos = begin;
          break;
        }
        pos = iter.pos() - iter.GetString().length();
      }
    }
  }
  return SelectionModel(pos, CURSOR_FORWARD);
}

Range RenderTextHarfBuzz::GetGlyphBounds(size_t index) {
  const size_t run_index =
      GetRunContainingCaret(SelectionModel(index, CURSOR_FORWARD));
  // Return edge bounds if the index is invalid or beyond the layout text size.
  if (run_index >= runs_.size())
    return Range(GetStringSize().width());
  const size_t layout_index = TextIndexToLayoutIndex(index);
  return Range(runs_[run_index]->GetGlyphXBoundary(layout_index, false),
               runs_[run_index]->GetGlyphXBoundary(layout_index, true));
}

std::vector<Rect> RenderTextHarfBuzz::GetSubstringBounds(const Range& range) {
  DCHECK(!needs_layout_);
  DCHECK(Range(0, text().length()).Contains(range));
  Range layout_range(TextIndexToLayoutIndex(range.start()),
                     TextIndexToLayoutIndex(range.end()));
  DCHECK(Range(0, GetLayoutText().length()).Contains(layout_range));

  std::vector<Rect> rects;
  if (layout_range.is_empty())
    return rects;
  std::vector<Range> bounds;

  // Add a Range for each run/selection intersection.
  // TODO(msw): The bounds should probably not always be leading the range ends.
  for (size_t i = 0; i < runs_.size(); ++i) {
    const internal::TextRunHarfBuzz* run = runs_[visual_to_logical_[i]];
    Range intersection = run->range.Intersect(layout_range);
    if (intersection.IsValid()) {
      DCHECK(!intersection.is_reversed());
      Range range_x(run->GetGlyphXBoundary(intersection.start(), false),
                    run->GetGlyphXBoundary(intersection.end(), false));
      if (range_x.is_empty())
        continue;
      range_x = Range(range_x.GetMin(), range_x.GetMax());
      // Union this with the last range if they're adjacent.
      DCHECK(bounds.empty() || bounds.back().GetMax() <= range_x.GetMin());
      if (!bounds.empty() && bounds.back().GetMax() == range_x.GetMin()) {
        range_x = Range(bounds.back().GetMin(), range_x.GetMax());
        bounds.pop_back();
      }
      bounds.push_back(range_x);
    }
  }
  for (size_t i = 0; i < bounds.size(); ++i) {
    std::vector<Rect> current_rects = TextBoundsToViewBounds(bounds[i]);
    rects.insert(rects.end(), current_rects.begin(), current_rects.end());
  }
  return rects;
}

size_t RenderTextHarfBuzz::TextIndexToLayoutIndex(size_t index) const {
  DCHECK_LE(index, text().length());
  ptrdiff_t i = obscured() ? UTF16IndexToOffset(text(), 0, index) : index;
  CHECK_GE(i, 0);
  // Clamp layout indices to the length of the text actually used for layout.
  return std::min<size_t>(GetLayoutText().length(), i);
}

size_t RenderTextHarfBuzz::LayoutIndexToTextIndex(size_t index) const {
  if (!obscured())
    return index;

  DCHECK_LE(index, GetLayoutText().length());
  const size_t text_index = UTF16OffsetToIndex(text(), 0, index);
  DCHECK_LE(text_index, text().length());
  return text_index;
}

bool RenderTextHarfBuzz::IsValidCursorIndex(size_t index) {
  if (index == 0 || index == text().length())
    return true;
  if (!IsValidLogicalIndex(index))
    return false;
  EnsureLayout();
  // Disallow indices amid multi-character graphemes by checking glyph bounds.
  // These characters are not surrogate-pairs, but may yield a single glyph:
  //   \x0915\x093f - (ki) - one of many Devanagari biconsonantal conjuncts.
  //   \x0e08\x0e33 - (cho chan + sara am) - a Thai consonant and vowel pair.
  return GetGlyphBounds(index) != GetGlyphBounds(index - 1);
}

void RenderTextHarfBuzz::ResetLayout() {
  needs_layout_ = true;
}

void RenderTextHarfBuzz::EnsureLayout() {
  if (needs_layout_) {
    runs_.clear();

    if (!GetLayoutText().empty()) {
      ItemizeText();

      for (size_t i = 0; i < runs_.size(); ++i)
        ShapeRun(runs_[i]);

      // Precalculate run width information.
      size_t preceding_run_widths = 0;
      for (size_t i = 0; i < runs_.size(); ++i) {
        internal::TextRunHarfBuzz* run = runs_[visual_to_logical_[i]];
        run->preceding_run_widths = preceding_run_widths;
        preceding_run_widths += run->width;
      }
    }

    needs_layout_ = false;
    std::vector<internal::Line> empty_lines;
    set_lines(&empty_lines);
  }

  if (lines().empty()) {
    std::vector<internal::Line> lines;
    lines.push_back(internal::Line());
    lines[0].baseline = font_list().GetBaseline();
    lines[0].size.set_height(font_list().GetHeight());

    int current_x = 0;
    SkPaint paint;

    for (size_t i = 0; i < runs_.size(); ++i) {
      const internal::TextRunHarfBuzz& run = *runs_[visual_to_logical_[i]];
      internal::LineSegment segment;
      segment.x_range = Range(current_x, current_x + run.width);
      segment.char_range = run.range;
      segment.run = i;
      lines[0].segments.push_back(segment);

      paint.setTypeface(run.skia_face.get());
      paint.setTextSize(run.font_size);
      SkPaint::FontMetrics metrics;
      paint.getFontMetrics(&metrics);

      lines[0].size.set_width(lines[0].size.width() + run.width);
      lines[0].size.set_height(std::max(lines[0].size.height(),
          SkScalarRoundToInt(metrics.fDescent - metrics.fAscent)));
      lines[0].baseline = std::max(lines[0].baseline,
                                   SkScalarRoundToInt(-metrics.fAscent));
    }

    set_lines(&lines);
  }
}

void RenderTextHarfBuzz::DrawVisualText(Canvas* canvas) {
  DCHECK(!needs_layout_);

  int current_x = 0;

  internal::SkiaTextRenderer renderer(canvas);
  ApplyFadeEffects(&renderer);
  ApplyTextShadows(&renderer);

#if defined(OS_WIN)
  bool smoothing_enabled;
  bool cleartype_enabled;
  GetCachedFontSmoothingSettings(&smoothing_enabled, &cleartype_enabled);
  // Note that |cleartype_enabled| corresponds to Skia's |enable_lcd_text|.
  renderer.SetFontSmoothingSettings(
      smoothing_enabled, cleartype_enabled && !background_is_transparent(),
      smoothing_enabled /* subpixel_positioning */);
#endif

  ApplyCompositionAndSelectionStyles();

  const Vector2d line_offset = GetLineOffset(0);

  for (size_t i = 0; i < runs_.size(); ++i) {
    const internal::TextRunHarfBuzz& run = *runs_[visual_to_logical_[i]];
    renderer.SetTypeface(run.skia_face.get());
    renderer.SetTextSize(run.font_size);

    canvas->Save();
    Vector2d origin = line_offset + Vector2d(current_x, lines()[0].baseline);
    canvas->Translate(origin);

    for (BreakList<SkColor>::const_iterator it =
             colors().GetBreak(run.range.start());
         it != colors().breaks().end() && it->first < run.range.end();
         ++it) {
      const Range intersection = colors().GetRange(it).Intersect(run.range);
      const Range colored_glyphs = run.CharRangeToGlyphRange(intersection);
      // The range may be empty if a portion of a multi-character grapheme is
      // selected, yielding two colors for a single glyph. For now, this just
      // paints the glyph with a single style, but it should paint it twice,
      // clipped according to selection bounds. See http://crbug.com/366786
      if (colored_glyphs.is_empty())
        continue;

      renderer.SetForegroundColor(it->second);
      renderer.DrawPosText(&run.positions[colored_glyphs.start()],
                           &run.glyphs[colored_glyphs.start()],
                           colored_glyphs.length());
      int width = (colored_glyphs.end() == run.glyph_count ? run.width :
              run.positions[colored_glyphs.end()].x()) -
          run.positions[colored_glyphs.start()].x();
      renderer.DrawDecorations(0, 0, width, run.underline, run.strike,
                               run.diagonal_strike);
    }

    canvas->Restore();
    current_x += run.width;
  }

  renderer.EndDiagonalStrike();

  UndoCompositionAndSelectionStyles();
}

size_t RenderTextHarfBuzz::GetRunContainingCaret(
    const SelectionModel& caret) const {
  DCHECK(!needs_layout_);
  size_t layout_position = TextIndexToLayoutIndex(caret.caret_pos());
  LogicalCursorDirection affinity = caret.caret_affinity();
  for (size_t run = 0; run < runs_.size(); ++run) {
    if (RangeContainsCaret(runs_[run]->range, layout_position, affinity))
      return run;
  }
  return runs_.size();
}

size_t RenderTextHarfBuzz::GetRunContainingXCoord(int x, int* offset) const {
  DCHECK(!needs_layout_);
  if (x < 0)
    return runs_.size();
  // Find the text run containing the argument point (assumed already offset).
  int current_x = 0;
  for (size_t i = 0; i < runs_.size(); ++i) {
    size_t run = visual_to_logical_[i];
    current_x += runs_[run]->width;
    if (x < current_x) {
      *offset = x - (current_x - runs_[run]->width);
      return run;
    }
  }
  return runs_.size();
}

SelectionModel RenderTextHarfBuzz::FirstSelectionModelInsideRun(
    const internal::TextRunHarfBuzz* run) {
  size_t position = LayoutIndexToTextIndex(run->range.start());
  position = IndexOfAdjacentGrapheme(position, CURSOR_FORWARD);
  return SelectionModel(position, CURSOR_BACKWARD);
}

SelectionModel RenderTextHarfBuzz::LastSelectionModelInsideRun(
    const internal::TextRunHarfBuzz* run) {
  size_t position = LayoutIndexToTextIndex(run->range.end());
  position = IndexOfAdjacentGrapheme(position, CURSOR_BACKWARD);
  return SelectionModel(position, CURSOR_FORWARD);
}

void RenderTextHarfBuzz::ItemizeText() {
  const base::string16& text = GetLayoutText();
  const bool is_text_rtl = GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  DCHECK_NE(0U, text.length());

  // If ICU fails to itemize the text, we create a run that spans the entire
  // text. This is needed because leaving the runs set empty causes some clients
  // to misbehave since they expect non-zero text metrics from a non-empty text.
  base::i18n::BiDiLineIterator bidi_iterator;
  if (!bidi_iterator.Open(text, is_text_rtl, false)) {
    internal::TextRunHarfBuzz* run = new internal::TextRunHarfBuzz;
    run->range = Range(0, text.length());
    runs_.push_back(run);
    visual_to_logical_ = logical_to_visual_ = std::vector<int32_t>(1, 0);
    return;
  }

  // Temporarily apply composition underlines and selection colors.
  ApplyCompositionAndSelectionStyles();

  // Build the list of runs from the script items and ranged styles. Use an
  // empty color BreakList to avoid breaking runs at color boundaries.
  BreakList<SkColor> empty_colors;
  empty_colors.SetMax(text.length());
  internal::StyleIterator style(empty_colors, styles());

  for (size_t run_break = 0; run_break < text.length();) {
    internal::TextRunHarfBuzz* run = new internal::TextRunHarfBuzz;
    run->range.set_start(run_break);
    run->font_style = (style.style(BOLD) ? Font::BOLD : 0) |
                      (style.style(ITALIC) ? Font::ITALIC : 0);
    run->strike = style.style(STRIKE);
    run->diagonal_strike = style.style(DIAGONAL_STRIKE);
    run->underline = style.style(UNDERLINE);

    int32 script_item_break = 0;
    bidi_iterator.GetLogicalRun(run_break, &script_item_break, &run->level);
    // Odd BiDi embedding levels correspond to RTL runs.
    run->is_rtl = (run->level % 2) == 1;
    // Find the length and script of this script run.
    script_item_break = ScriptInterval(text, run_break,
        script_item_break - run_break, &run->script) + run_break;

    // Find the next break and advance the iterators as needed.
    run_break = std::min(static_cast<size_t>(script_item_break),
                         TextIndexToLayoutIndex(style.GetRange().end()));

    // Break runs adjacent to character substrings in certain code blocks.
    // This avoids using their fallback fonts for more characters than needed,
    // in cases like "\x25B6 Media Title", etc. http://crbug.com/278913
    if (run_break > run->range.start())
      run_break = FindUnusualCharacter(text, run->range.start(), run_break);

    DCHECK(IsValidCodePointIndex(text, run_break));
    style.UpdatePosition(LayoutIndexToTextIndex(run_break));
    run->range.set_end(run_break);

    runs_.push_back(run);
  }

  // Undo the temporarily applied composition underlines and selection colors.
  UndoCompositionAndSelectionStyles();

  const size_t num_runs = runs_.size();
  std::vector<UBiDiLevel> levels(num_runs);
  for (size_t i = 0; i < num_runs; ++i)
    levels[i] = runs_[i]->level;
  visual_to_logical_.resize(num_runs);
  ubidi_reorderVisual(&levels[0], num_runs, &visual_to_logical_[0]);
  logical_to_visual_.resize(num_runs);
  ubidi_reorderLogical(&levels[0], num_runs, &logical_to_visual_[0]);
}

void RenderTextHarfBuzz::ShapeRun(internal::TextRunHarfBuzz* run) {
  const base::string16& text = GetLayoutText();
  // TODO(ckocagil|yukishiino): Implement font fallback.
  const Font& primary_font = font_list().GetPrimaryFont();
  run->skia_face = internal::CreateSkiaTypeface(primary_font.GetFontName(),
                                                run->font_style);
  run->font_size = primary_font.GetFontSize();

  hb_font_t* harfbuzz_font = CreateHarfBuzzFont(run->skia_face.get(),
                                                run->font_size);

  // Create a HarfBuzz buffer and add the string to be shaped. The HarfBuzz
  // buffer holds our text, run information to be used by the shaping engine,
  // and the resulting glyph data.
  hb_buffer_t* buffer = hb_buffer_create();
  hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16*>(text.c_str()),
                      text.length(), run->range.start(), run->range.length());
  hb_buffer_set_script(buffer, ICUScriptToHBScript(run->script));
  hb_buffer_set_direction(buffer,
      run->is_rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
  // TODO(ckocagil): Should we determine the actual language?
  hb_buffer_set_language(buffer, hb_language_get_default());

  // Shape the text.
  hb_shape(harfbuzz_font, buffer, NULL, 0);

  // Populate the run fields with the resulting glyph data in the buffer.
  unsigned int glyph_count = 0;
  hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buffer, &glyph_count);
  hb_glyph_position_t* hb_positions = hb_buffer_get_glyph_positions(buffer,
                                                                    NULL);
  run->glyph_count = glyph_count;
  run->glyphs.reset(new uint16[run->glyph_count]);
  run->glyph_to_char.reset(new uint32[run->glyph_count]);
  run->positions.reset(new SkPoint[run->glyph_count]);
  for (size_t i = 0; i < run->glyph_count; ++i) {
    run->glyphs[i] = infos[i].codepoint;
    run->glyph_to_char[i] = infos[i].cluster;
    const int x_offset =
        SkScalarRoundToInt(SkFixedToScalar(hb_positions[i].x_offset));
    const int y_offset =
        SkScalarRoundToInt(SkFixedToScalar(hb_positions[i].y_offset));
    run->positions[i].set(run->width + x_offset, y_offset);
    run->width +=
        SkScalarRoundToInt(SkFixedToScalar(hb_positions[i].x_advance));
  }

  hb_buffer_destroy(buffer);
  hb_font_destroy(harfbuzz_font);
}

}  // namespace gfx
