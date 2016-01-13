// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/render_text.h"

#include <algorithm>

#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/break_list.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/render_text_harfbuzz.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/gfx/render_text_win.h"
#endif

#if defined(OS_LINUX) && !defined(USE_OZONE)
#include "ui/gfx/render_text_pango.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using base::WideToUTF16;

namespace gfx {

namespace {

// Various weak, LTR, RTL, and Bidi string cases with three characters each.
const wchar_t kWeak[] =      L" . ";
const wchar_t kLtr[] =       L"abc";
const wchar_t kRtl[] =       L"\x5d0\x5d1\x5d2";
#if !defined(OS_MACOSX)
const wchar_t kLtrRtl[] =    L"a" L"\x5d0\x5d1";
const wchar_t kLtrRtlLtr[] = L"a" L"\x5d1" L"b";
const wchar_t kRtlLtr[] =    L"\x5d0\x5d1" L"a";
const wchar_t kRtlLtrRtl[] = L"\x5d0" L"a" L"\x5d1";
#endif

// Checks whether |range| contains |index|. This is not the same as calling
// range.Contains(Range(index)), which returns true if |index| == |range.end()|.
bool IndexInRange(const Range& range, size_t index) {
  return index >= range.start() && index < range.end();
}

base::string16 GetSelectedText(RenderText* render_text) {
  return render_text->text().substr(render_text->selection().GetMin(),
                                    render_text->selection().length());
}

// A test utility function to set the application default text direction.
void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  EXPECT_EQ(rtl, base::i18n::IsRTL());
}

#if !defined(OS_MACOSX)
// Ensure cursor movement in the specified |direction| yields |expected| values.
void RunMoveCursorLeftRightTest(RenderText* render_text,
                                const std::vector<SelectionModel>& expected,
                                VisualCursorDirection direction) {
  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(base::StringPrintf("Going %s; expected value index %d.",
        direction == CURSOR_LEFT ? "left" : "right", static_cast<int>(i)));
    EXPECT_EQ(expected[i], render_text->selection_model());
    render_text->MoveCursor(CHARACTER_BREAK, direction, false);
  }
  // Check that cursoring is clamped at the line edge.
  EXPECT_EQ(expected.back(), render_text->selection_model());
  // Check that it is the line edge.
  render_text->MoveCursor(LINE_BREAK, direction, false);
  EXPECT_EQ(expected.back(), render_text->selection_model());
}
#endif  // !defined(OS_MACOSX)

}  // namespace

class RenderTextTest : public testing::Test {
};

TEST_F(RenderTextTest, DefaultStyle) {
  // Check the default styles applied to new instances and adjusted text.
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  EXPECT_TRUE(render_text->text().empty());
  const wchar_t* const cases[] = { kWeak, kLtr, L"Hello", kRtl, L"", L"" };
  for (size_t i = 0; i < arraysize(cases); ++i) {
    EXPECT_TRUE(render_text->colors().EqualsValueForTesting(SK_ColorBLACK));
    for (size_t style = 0; style < NUM_TEXT_STYLES; ++style)
      EXPECT_TRUE(render_text->styles()[style].EqualsValueForTesting(false));
    render_text->SetText(WideToUTF16(cases[i]));
  }
}

TEST_F(RenderTextTest, SetColorAndStyle) {
  // Ensure custom default styles persist across setting and clearing text.
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  const SkColor color = SK_ColorRED;
  render_text->SetColor(color);
  render_text->SetStyle(BOLD, true);
  render_text->SetStyle(UNDERLINE, false);
  const wchar_t* const cases[] = { kWeak, kLtr, L"Hello", kRtl, L"", L"" };
  for (size_t i = 0; i < arraysize(cases); ++i) {
    EXPECT_TRUE(render_text->colors().EqualsValueForTesting(color));
    EXPECT_TRUE(render_text->styles()[BOLD].EqualsValueForTesting(true));
    EXPECT_TRUE(render_text->styles()[UNDERLINE].EqualsValueForTesting(false));
    render_text->SetText(WideToUTF16(cases[i]));

    // Ensure custom default styles can be applied after text has been set.
    if (i == 1)
      render_text->SetStyle(STRIKE, true);
    if (i >= 1)
      EXPECT_TRUE(render_text->styles()[STRIKE].EqualsValueForTesting(true));
  }
}

TEST_F(RenderTextTest, ApplyColorAndStyle) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16("012345678"));

  // Apply a ranged color and style and check the resulting breaks.
  render_text->ApplyColor(SK_ColorRED, Range(1, 4));
  render_text->ApplyStyle(BOLD, true, Range(2, 5));
  std::vector<std::pair<size_t, SkColor> > expected_color;
  expected_color.push_back(std::pair<size_t, SkColor>(0, SK_ColorBLACK));
  expected_color.push_back(std::pair<size_t, SkColor>(1, SK_ColorRED));
  expected_color.push_back(std::pair<size_t, SkColor>(4, SK_ColorBLACK));
  EXPECT_TRUE(render_text->colors().EqualsForTesting(expected_color));
  std::vector<std::pair<size_t, bool> > expected_style;
  expected_style.push_back(std::pair<size_t, bool>(0, false));
  expected_style.push_back(std::pair<size_t, bool>(2, true));
  expected_style.push_back(std::pair<size_t, bool>(5, false));
  EXPECT_TRUE(render_text->styles()[BOLD].EqualsForTesting(expected_style));

  // Ensure setting a color and style overrides the ranged colors and styles.
  render_text->SetColor(SK_ColorBLUE);
  EXPECT_TRUE(render_text->colors().EqualsValueForTesting(SK_ColorBLUE));
  render_text->SetStyle(BOLD, false);
  EXPECT_TRUE(render_text->styles()[BOLD].EqualsValueForTesting(false));

  // Apply a color and style over the text end and check the resulting breaks.
  // (INT_MAX should be used instead of the text length for the range end)
  const size_t text_length = render_text->text().length();
  render_text->ApplyColor(SK_ColorRED, Range(0, text_length));
  render_text->ApplyStyle(BOLD, true, Range(2, text_length));
  std::vector<std::pair<size_t, SkColor> > expected_color_end;
  expected_color_end.push_back(std::pair<size_t, SkColor>(0, SK_ColorRED));
  EXPECT_TRUE(render_text->colors().EqualsForTesting(expected_color_end));
  std::vector<std::pair<size_t, bool> > expected_style_end;
  expected_style_end.push_back(std::pair<size_t, bool>(0, false));
  expected_style_end.push_back(std::pair<size_t, bool>(2, true));
  EXPECT_TRUE(render_text->styles()[BOLD].EqualsForTesting(expected_style_end));

  // Ensure ranged values adjust to accommodate text length changes.
  render_text->ApplyStyle(ITALIC, true, Range(0, 2));
  render_text->ApplyStyle(ITALIC, true, Range(3, 6));
  render_text->ApplyStyle(ITALIC, true, Range(7, text_length));
  std::vector<std::pair<size_t, bool> > expected_italic;
  expected_italic.push_back(std::pair<size_t, bool>(0, true));
  expected_italic.push_back(std::pair<size_t, bool>(2, false));
  expected_italic.push_back(std::pair<size_t, bool>(3, true));
  expected_italic.push_back(std::pair<size_t, bool>(6, false));
  expected_italic.push_back(std::pair<size_t, bool>(7, true));
  EXPECT_TRUE(render_text->styles()[ITALIC].EqualsForTesting(expected_italic));

  // Truncating the text should trim any corresponding breaks.
  render_text->SetText(ASCIIToUTF16("0123456"));
  expected_italic.resize(4);
  EXPECT_TRUE(render_text->styles()[ITALIC].EqualsForTesting(expected_italic));
  render_text->SetText(ASCIIToUTF16("01234"));
  expected_italic.resize(3);
  EXPECT_TRUE(render_text->styles()[ITALIC].EqualsForTesting(expected_italic));

  // Appending text should extend the terminal styles without changing breaks.
  render_text->SetText(ASCIIToUTF16("012345678"));
  EXPECT_TRUE(render_text->styles()[ITALIC].EqualsForTesting(expected_italic));
}

#if defined(OS_LINUX) && !defined(USE_OZONE)
TEST_F(RenderTextTest, PangoAttributes) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16("012345678"));

  // Apply ranged BOLD/ITALIC styles and check the resulting Pango attributes.
  render_text->ApplyStyle(BOLD, true, Range(2, 4));
  render_text->ApplyStyle(ITALIC, true, Range(1, 3));

  struct {
    int start;
    int end;
    bool bold;
    bool italic;
  } cases[] = {
    { 0, 1,       false, false },
    { 1, 2,       false, true  },
    { 2, 3,       true,  true  },
    { 3, 4,       true,  false },
    { 4, INT_MAX, false, false },
  };

  int start = 0, end = 0;
  RenderTextPango* rt_linux = static_cast<RenderTextPango*>(render_text.get());
  rt_linux->EnsureLayout();
  PangoAttrList* attributes = pango_layout_get_attributes(rt_linux->layout_);
  PangoAttrIterator* iter = pango_attr_list_get_iterator(attributes);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    pango_attr_iterator_range(iter, &start, &end);
    EXPECT_EQ(cases[i].start, start);
    EXPECT_EQ(cases[i].end, end);
    PangoFontDescription* font = pango_font_description_new();
    pango_attr_iterator_get_font(iter, font, NULL, NULL);
    char* description_string = pango_font_description_to_string(font);
    const base::string16 desc = ASCIIToUTF16(description_string);
    const bool bold = desc.find(ASCIIToUTF16("Bold")) != std::string::npos;
    EXPECT_EQ(cases[i].bold, bold);
    const bool italic = desc.find(ASCIIToUTF16("Italic")) != std::string::npos;
    EXPECT_EQ(cases[i].italic, italic);
    pango_attr_iterator_next(iter);
    pango_font_description_free(font);
    g_free(description_string);
  }
  EXPECT_FALSE(pango_attr_iterator_next(iter));
  pango_attr_iterator_destroy(iter);
}
#endif

// TODO(asvitkine): Cursor movements tests disabled on Mac because RenderTextMac
//                  does not implement this yet. http://crbug.com/131618
#if !defined(OS_MACOSX)
void TestVisualCursorMotionInObscuredField(RenderText* render_text,
                                           const base::string16& text,
                                           bool select) {
  ASSERT_TRUE(render_text->obscured());
  render_text->SetText(text);
  int len = text.length();
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, select);
  EXPECT_EQ(SelectionModel(Range(select ? 0 : len, len), CURSOR_FORWARD),
            render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, select);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  for (int j = 1; j <= len; ++j) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, select);
    EXPECT_EQ(SelectionModel(Range(select ? 0 : j, j), CURSOR_BACKWARD),
              render_text->selection_model());
  }
  for (int j = len - 1; j >= 0; --j) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, select);
    EXPECT_EQ(SelectionModel(Range(select ? 0 : j, j), CURSOR_FORWARD),
              render_text->selection_model());
  }
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, select);
  EXPECT_EQ(SelectionModel(Range(select ? 0 : len, len), CURSOR_FORWARD),
            render_text->selection_model());
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, select);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
}

TEST_F(RenderTextTest, ObscuredText) {
  const base::string16 seuss = ASCIIToUTF16("hop on pop");
  const base::string16 no_seuss = ASCIIToUTF16("**********");
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  // GetLayoutText() returns asterisks when the obscured bit is set.
  render_text->SetText(seuss);
  render_text->SetObscured(true);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(no_seuss, render_text->GetLayoutText());
  render_text->SetObscured(false);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(seuss, render_text->GetLayoutText());

  render_text->SetObscured(true);

  // Surrogate pairs are counted as one code point.
  const base::char16 invalid_surrogates[] = {0xDC00, 0xD800, 0};
  render_text->SetText(invalid_surrogates);
  EXPECT_EQ(ASCIIToUTF16("**"), render_text->GetLayoutText());
  const base::char16 valid_surrogates[] = {0xD800, 0xDC00, 0};
  render_text->SetText(valid_surrogates);
  EXPECT_EQ(ASCIIToUTF16("*"), render_text->GetLayoutText());
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(2U, render_text->cursor_position());

  // Test index conversion and cursor validity with a valid surrogate pair.
  EXPECT_EQ(0U, render_text->TextIndexToLayoutIndex(0U));
  EXPECT_EQ(1U, render_text->TextIndexToLayoutIndex(1U));
  EXPECT_EQ(1U, render_text->TextIndexToLayoutIndex(2U));
  EXPECT_EQ(0U, render_text->LayoutIndexToTextIndex(0U));
  EXPECT_EQ(2U, render_text->LayoutIndexToTextIndex(1U));
  EXPECT_TRUE(render_text->IsValidCursorIndex(0U));
  EXPECT_FALSE(render_text->IsValidCursorIndex(1U));
  EXPECT_TRUE(render_text->IsValidCursorIndex(2U));

  // FindCursorPosition() should not return positions between a surrogate pair.
  render_text->SetDisplayRect(Rect(0, 0, 20, 20));
  EXPECT_EQ(render_text->FindCursorPosition(Point(0, 0)).caret_pos(), 0U);
  EXPECT_EQ(render_text->FindCursorPosition(Point(20, 0)).caret_pos(), 2U);
  for (int x = -1; x <= 20; ++x) {
    SelectionModel selection = render_text->FindCursorPosition(Point(x, 0));
    EXPECT_TRUE(selection.caret_pos() == 0U || selection.caret_pos() == 2U);
  }

  // GetGlyphBounds() should yield the entire string bounds for text index 0.
  EXPECT_EQ(render_text->GetStringSize().width(),
            static_cast<int>(render_text->GetGlyphBounds(0U).length()));

  // Cursoring is independent of underlying characters when text is obscured.
  const wchar_t* const texts[] = {
    kWeak, kLtr, kLtrRtl, kLtrRtlLtr, kRtl, kRtlLtr, kRtlLtrRtl,
    L"hop on pop",                              // Check LTR word boundaries.
    L"\x05d0\x05d1 \x05d0\x05d2 \x05d1\x05d2",  // Check RTL word boundaries.
  };
  for (size_t i = 0; i < arraysize(texts); ++i) {
    base::string16 text = WideToUTF16(texts[i]);
    TestVisualCursorMotionInObscuredField(render_text.get(), text, false);
    TestVisualCursorMotionInObscuredField(render_text.get(), text, true);
  }
}

TEST_F(RenderTextTest, RevealObscuredText) {
  const base::string16 seuss = ASCIIToUTF16("hop on pop");
  const base::string16 no_seuss = ASCIIToUTF16("**********");
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  render_text->SetText(seuss);
  render_text->SetObscured(true);
  EXPECT_EQ(seuss, render_text->text());
  EXPECT_EQ(no_seuss, render_text->GetLayoutText());

  // Valid reveal index and new revealed index clears previous one.
  render_text->RenderText::SetObscuredRevealIndex(0);
  EXPECT_EQ(ASCIIToUTF16("h*********"), render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  EXPECT_EQ(ASCIIToUTF16("*o********"), render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(ASCIIToUTF16("**p*******"), render_text->GetLayoutText());

  // Invalid reveal index.
  render_text->RenderText::SetObscuredRevealIndex(-1);
  EXPECT_EQ(no_seuss, render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(seuss.length() + 1);
  EXPECT_EQ(no_seuss, render_text->GetLayoutText());

  // SetObscured clears the revealed index.
  render_text->RenderText::SetObscuredRevealIndex(0);
  EXPECT_EQ(ASCIIToUTF16("h*********"), render_text->GetLayoutText());
  render_text->SetObscured(false);
  EXPECT_EQ(seuss, render_text->GetLayoutText());
  render_text->SetObscured(true);
  EXPECT_EQ(no_seuss, render_text->GetLayoutText());

  // SetText clears the revealed index.
  render_text->SetText(ASCIIToUTF16("new"));
  EXPECT_EQ(ASCIIToUTF16("***"), render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(ASCIIToUTF16("**w"), render_text->GetLayoutText());
  render_text->SetText(ASCIIToUTF16("new longer"));
  EXPECT_EQ(ASCIIToUTF16("**********"), render_text->GetLayoutText());

  // Text with invalid surrogates.
  const base::char16 invalid_surrogates[] = {0xDC00, 0xD800, 'h', 'o', 'p', 0};
  render_text->SetText(invalid_surrogates);
  EXPECT_EQ(ASCIIToUTF16("*****"), render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(0);
  const base::char16 invalid_expect_0[] = {0xDC00, '*', '*', '*', '*', 0};
  EXPECT_EQ(invalid_expect_0, render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  const base::char16 invalid_expect_1[] = {'*', 0xD800, '*', '*', '*', 0};
  EXPECT_EQ(invalid_expect_1, render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(ASCIIToUTF16("**h**"), render_text->GetLayoutText());

  // Text with valid surrogates before and after the reveal index.
  const base::char16 valid_surrogates[] =
      {0xD800, 0xDC00, 'h', 'o', 'p', 0xD800, 0xDC00, 0};
  render_text->SetText(valid_surrogates);
  EXPECT_EQ(ASCIIToUTF16("*****"), render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(0);
  const base::char16 valid_expect_0_and_1[] =
      {0xD800, 0xDC00, '*', '*', '*', '*', 0};
  EXPECT_EQ(valid_expect_0_and_1, render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(1);
  EXPECT_EQ(valid_expect_0_and_1, render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(2);
  EXPECT_EQ(ASCIIToUTF16("*h***"), render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(5);
  const base::char16 valid_expect_5_and_6[] =
      {'*', '*', '*', '*', 0xD800, 0xDC00, 0};
  EXPECT_EQ(valid_expect_5_and_6, render_text->GetLayoutText());
  render_text->RenderText::SetObscuredRevealIndex(6);
  EXPECT_EQ(valid_expect_5_and_6, render_text->GetLayoutText());
}

TEST_F(RenderTextTest, ElidedText) {
  // TODO(skanuj) : Add more test cases for following
  // - RenderText styles.
  // - Cross interaction of truncate, elide and obscure.
  // - ElideText tests from text_elider.cc.
  struct {
    const wchar_t* text;
    const wchar_t* layout_text;
    const bool elision_expected;
  } cases[] = {
    // Strings shorter than the elision width should be laid out in full.
    { L"",        L""       , false },
    { L"M",       L""       , false },
    { L" . ",     L" . "    , false },
    { kWeak,      kWeak     , false },
    { kLtr,       kLtr      , false },
    { kLtrRtl,    kLtrRtl   , false },
    { kLtrRtlLtr, kLtrRtlLtr, false },
    { kRtl,       kRtl      , false },
    { kRtlLtr,    kRtlLtr   , false },
    { kRtlLtrRtl, kRtlLtrRtl, false },
    // Strings as long as the elision width should be laid out in full.
    { L"012ab",   L"012ab"  , false },
    // Long strings should be elided with an ellipsis appended at the end.
    { L"012abc",              L"012a\x2026", true },
    { L"012ab" L"\x5d0\x5d1", L"012a\x2026", true },
    { L"012a" L"\x5d1" L"b",  L"012a\x2026", true },
    // No RLM marker added as digits (012) have weak directionality.
    { L"01" L"\x5d0\x5d1\x5d2", L"01\x5d0\x5d1\x2026", true },
    // RLM marker added as "ab" have strong LTR directionality.
    { L"ab" L"\x5d0\x5d1\x5d2", L"ab\x5d0\x5d1\x2026\x200f", true },
    // Complex script is not handled. In this example, the "\x0915\x093f" is a
    // compound glyph, but only half of it is elided.
    { L"0123\x0915\x093f", L"0123\x0915\x2026", true },
    // Surrogate pairs should be elided reasonably enough.
    { L"0\x05e9\x05bc\x05c1\x05b8",   L"0\x05e9\x05bc\x05c1\x05b8", false },
    { L"0\x05e9\x05bc\x05c1\x05b8",   L"0\x05e9\x05bc\x2026"      , true  },
    { L"01\x05e9\x05bc\x05c1\x05b8",  L"01\x05e9\x2026"           , true  },
    { L"012\x05e9\x05bc\x05c1\x05b8", L"012\x2026\x200E"          , true  },
    { L"012\xF0\x9D\x84\x9E",         L"012\xF0\x2026"            , true  },
  };

  scoped_ptr<RenderText> expected_render_text(RenderText::CreateInstance());
  expected_render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  expected_render_text->SetDisplayRect(Rect(0, 0, 9999, 100));

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  render_text->SetElideBehavior(ELIDE_TAIL);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    // Compute expected width
    expected_render_text->SetText(WideToUTF16(cases[i].layout_text));
    int expected_width = expected_render_text->GetContentWidth();

    base::string16 input = WideToUTF16(cases[i].text);
    // Extend the input text to ensure that it is wider than the layout_text,
    // and so it will get elided.
    if (cases[i].elision_expected)
      input.append(WideToUTF16(L" MMMMMMMMMMM"));

    render_text->SetText(input);
    render_text->SetDisplayRect(Rect(0, 0, expected_width, 100));
    EXPECT_EQ(input, render_text->text())
        << "->For case " << i << ": " << cases[i].text << "\n";
    EXPECT_EQ(WideToUTF16(cases[i].layout_text), render_text->GetLayoutText())
        << "->For case " << i << ": " << cases[i].text << "\n";
    expected_render_text->SetText(base::string16());
  }
}

TEST_F(RenderTextTest, ElidedObscuredText) {
  scoped_ptr<RenderText> expected_render_text(RenderText::CreateInstance());
  expected_render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  expected_render_text->SetDisplayRect(Rect(0, 0, 9999, 100));
  expected_render_text->SetText(WideToUTF16(L"**\x2026"));

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetFontList(FontList("serif, Sans serif, 12px"));
  render_text->SetElideBehavior(ELIDE_TAIL);
  render_text->SetDisplayRect(
      Rect(0, 0, expected_render_text->GetContentWidth(), 100));
  render_text->SetObscured(true);
  render_text->SetText(WideToUTF16(L"abcdef"));
  EXPECT_EQ(WideToUTF16(L"abcdef"), render_text->text());
  EXPECT_EQ(WideToUTF16(L"**\x2026"), render_text->GetLayoutText());
}

TEST_F(RenderTextTest, TruncatedText) {
  struct {
    const wchar_t* text;
    const wchar_t* layout_text;
  } cases[] = {
    // Strings shorter than the truncation length should be laid out in full.
    { L"",        L""        },
    { kWeak,      kWeak      },
    { kLtr,       kLtr       },
    { kLtrRtl,    kLtrRtl    },
    { kLtrRtlLtr, kLtrRtlLtr },
    { kRtl,       kRtl       },
    { kRtlLtr,    kRtlLtr    },
    { kRtlLtrRtl, kRtlLtrRtl },
    // Strings as long as the truncation length should be laid out in full.
    { L"01234",   L"01234"   },
    // Long strings should be truncated with an ellipsis appended at the end.
    { L"012345",                  L"0123\x2026"     },
    { L"012" L" . ",              L"012 \x2026"     },
    { L"012" L"abc",              L"012a\x2026"     },
    { L"012" L"a" L"\x5d0\x5d1",  L"012a\x2026"     },
    { L"012" L"a" L"\x5d1" L"b",  L"012a\x2026"     },
    { L"012" L"\x5d0\x5d1\x5d2",  L"012\x5d0\x2026" },
    { L"012" L"\x5d0\x5d1" L"a",  L"012\x5d0\x2026" },
    { L"012" L"\x5d0" L"a" L"\x5d1",    L"012\x5d0\x2026" },
    // Surrogate pairs should be truncated reasonably enough.
    { L"0123\x0915\x093f",              L"0123\x2026"                },
    { L"0\x05e9\x05bc\x05c1\x05b8",     L"0\x05e9\x05bc\x05c1\x05b8" },
    { L"01\x05e9\x05bc\x05c1\x05b8",    L"01\x05e9\x05bc\x2026"      },
    { L"012\x05e9\x05bc\x05c1\x05b8",   L"012\x05e9\x2026"           },
    { L"0123\x05e9\x05bc\x05c1\x05b8",  L"0123\x2026"                },
    { L"01234\x05e9\x05bc\x05c1\x05b8", L"0123\x2026"                },
    { L"012\xF0\x9D\x84\x9E",           L"012\xF0\x2026"             },
  };

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->set_truncate_length(5);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    render_text->SetText(WideToUTF16(cases[i].text));
    EXPECT_EQ(WideToUTF16(cases[i].text), render_text->text());
    EXPECT_EQ(WideToUTF16(cases[i].layout_text), render_text->GetLayoutText())
        << "For case " << i << ": " << cases[i].text;
  }
}

TEST_F(RenderTextTest, TruncatedObscuredText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->set_truncate_length(3);
  render_text->SetObscured(true);
  render_text->SetText(WideToUTF16(L"abcdef"));
  EXPECT_EQ(WideToUTF16(L"abcdef"), render_text->text());
  EXPECT_EQ(WideToUTF16(L"**\x2026"), render_text->GetLayoutText());
}

TEST_F(RenderTextTest, TruncatedCursorMovementLTR) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->set_truncate_length(2);
  render_text->SetText(WideToUTF16(L"abcd"));

  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(SelectionModel(4, CURSOR_FORWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());

  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  // The cursor hops over the ellipsis and elided text to the line end.
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  // The cursor hops over the elided text to preceeding text.
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, TruncatedCursorMovementRTL) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->set_truncate_length(2);
  render_text->SetText(WideToUTF16(L"\x5d0\x5d1\x5d2\x5d3"));

  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(SelectionModel(4, CURSOR_FORWARD), render_text->selection_model());
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(SelectionModel(0, CURSOR_BACKWARD), render_text->selection_model());

  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  // The cursor hops over the ellipsis and elided text to the line end.
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  // The cursor hops over the elided text to preceeding text.
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, GetTextDirection) {
  struct {
    const wchar_t* text;
    const base::i18n::TextDirection text_direction;
  } cases[] = {
    // Blank strings and those with no/weak directionality default to LTR.
    { L"",        base::i18n::LEFT_TO_RIGHT },
    { kWeak,      base::i18n::LEFT_TO_RIGHT },
    // Strings that begin with strong LTR characters.
    { kLtr,       base::i18n::LEFT_TO_RIGHT },
    { kLtrRtl,    base::i18n::LEFT_TO_RIGHT },
    { kLtrRtlLtr, base::i18n::LEFT_TO_RIGHT },
    // Strings that begin with strong RTL characters.
    { kRtl,       base::i18n::RIGHT_TO_LEFT },
    { kRtlLtr,    base::i18n::RIGHT_TO_LEFT },
    { kRtlLtrRtl, base::i18n::RIGHT_TO_LEFT },
  };

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  const bool was_rtl = base::i18n::IsRTL();

  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTL(!base::i18n::IsRTL());
    const base::i18n::TextDirection ui_direction = base::i18n::IsRTL() ?
        base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT;

    // Ensure that directionality modes yield the correct text directions.
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(cases); j++) {
      render_text->SetText(WideToUTF16(cases[j].text));
      render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_TEXT);
      EXPECT_EQ(render_text->GetTextDirection(), cases[j].text_direction);
      render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_UI);
      EXPECT_EQ(render_text->GetTextDirection(), ui_direction);
      render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_LTR);
      EXPECT_EQ(render_text->GetTextDirection(), base::i18n::LEFT_TO_RIGHT);
      render_text->SetDirectionalityMode(DIRECTIONALITY_FORCE_RTL);
      EXPECT_EQ(render_text->GetTextDirection(), base::i18n::RIGHT_TO_LEFT);
    }
  }

  EXPECT_EQ(was_rtl, base::i18n::IsRTL());

  // Ensure that text changes update the direction for DIRECTIONALITY_FROM_TEXT.
  render_text->SetDirectionalityMode(DIRECTIONALITY_FROM_TEXT);
  render_text->SetText(WideToUTF16(kLtr));
  EXPECT_EQ(render_text->GetTextDirection(), base::i18n::LEFT_TO_RIGHT);
  render_text->SetText(WideToUTF16(kRtl));
  EXPECT_EQ(render_text->GetTextDirection(), base::i18n::RIGHT_TO_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  // Pure LTR.
  render_text->SetText(ASCIIToUTF16("abc"));
  // |expected| saves the expected SelectionModel when moving cursor from left
  // to right.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  // LTR-RTL
  render_text->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // The last one is the expected END position.
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInLtrRtlLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  // LTR-RTL-LTR.
  render_text->SetText(WideToUTF16(L"a" L"\x05d1" L"b"));
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  // Pure RTL.
  render_text->SetText(WideToUTF16(L"\x05d0\x05d1\x05d2"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  std::vector<SelectionModel> expected;

  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();

  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtr) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  // RTL-LTR
  render_text->SetText(WideToUTF16(L"\x05d0\x05d1\x05d2" L"abc"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();
  expected.push_back(SelectionModel(6, CURSOR_FORWARD));
  expected.push_back(SelectionModel(4, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(5, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(6, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

TEST_F(RenderTextTest, MoveCursorLeftRightInRtlLtrRtl) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  // RTL-LTR-RTL.
  render_text->SetText(WideToUTF16(L"\x05d0" L"a" L"\x05d1"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  std::vector<SelectionModel> expected;
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(1, CURSOR_FORWARD));
  expected.push_back(SelectionModel(3, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_LEFT);

  expected.clear();
  expected.push_back(SelectionModel(3, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_FORWARD));
  expected.push_back(SelectionModel(2, CURSOR_BACKWARD));
  expected.push_back(SelectionModel(0, CURSOR_FORWARD));
  expected.push_back(SelectionModel(0, CURSOR_BACKWARD));
  RunMoveCursorLeftRightTest(render_text.get(), expected, CURSOR_RIGHT);
}

// TODO(xji): temporarily disable in platform Win since the complex script
// characters turned into empty square due to font regression. So, not able
// to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
TEST_F(RenderTextTest, MoveCursorLeftRight_ComplexScript) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  render_text->SetText(WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"));
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(4U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(5U, render_text->cursor_position());

  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(4U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
}
#endif

TEST_F(RenderTextTest, MoveCursorLeftRight_MeiryoUILigatures) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  // Meiryo UI uses single-glyph ligatures for 'ff' and 'ffi', but each letter
  // (code point) has unique bounds, so mid-glyph cursoring should be possible.
  render_text->SetFontList(FontList("Meiryo UI, 12px"));
  render_text->SetText(WideToUTF16(L"ff ffi"));
  EXPECT_EQ(0U, render_text->cursor_position());
  for (size_t i = 0; i < render_text->text().length(); ++i) {
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
    EXPECT_EQ(i + 1, render_text->cursor_position());
  }
  EXPECT_EQ(6U, render_text->cursor_position());
}

TEST_F(RenderTextTest, GraphemePositions) {
  // LTR 2-character grapheme, LTR abc, LTR 2-character grapheme.
  const base::string16 kText1 =
      WideToUTF16(L"\x0915\x093f" L"abc" L"\x0915\x093f");

  // LTR ab, LTR 2-character grapheme, LTR cd.
  const base::string16 kText2 = WideToUTF16(L"ab" L"\x0915\x093f" L"cd");

  // The below is 'MUSICAL SYMBOL G CLEF', which is represented in UTF-16 as
  // two characters forming the surrogate pair 0x0001D11E.
  const std::string kSurrogate = "\xF0\x9D\x84\x9E";

  // LTR ab, UTF16 surrogate pair, LTR cd.
  const base::string16 kText3 = UTF8ToUTF16("ab" + kSurrogate + "cd");

  struct {
    base::string16 text;
    size_t index;
    size_t expected_previous;
    size_t expected_next;
  } cases[] = {
    { base::string16(), 0, 0, 0 },
    { base::string16(), 1, 0, 0 },
    { base::string16(), 50, 0, 0 },
    { kText1, 0, 0, 2 },
    { kText1, 1, 0, 2 },
    { kText1, 2, 0, 3 },
    { kText1, 3, 2, 4 },
    { kText1, 4, 3, 5 },
    { kText1, 5, 4, 7 },
    { kText1, 6, 5, 7 },
    { kText1, 7, 5, 7 },
    { kText1, 8, 7, 7 },
    { kText1, 50, 7, 7 },
    { kText2, 0, 0, 1 },
    { kText2, 1, 0, 2 },
    { kText2, 2, 1, 4 },
    { kText2, 3, 2, 4 },
    { kText2, 4, 2, 5 },
    { kText2, 5, 4, 6 },
    { kText2, 6, 5, 6 },
    { kText2, 7, 6, 6 },
    { kText2, 50, 6, 6 },
    { kText3, 0, 0, 1 },
    { kText3, 1, 0, 2 },
    { kText3, 2, 1, 4 },
    { kText3, 3, 2, 4 },
    { kText3, 4, 2, 5 },
    { kText3, 5, 4, 6 },
    { kText3, 6, 5, 6 },
    { kText3, 7, 6, 6 },
    { kText3, 50, 6, 6 },
  };

#if defined(OS_WIN)
  // TODO(msw): XP fails due to lack of font support: http://crbug.com/106450
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    render_text->SetText(cases[i].text);

    size_t next = render_text->IndexOfAdjacentGrapheme(cases[i].index,
                                                       CURSOR_FORWARD);
    EXPECT_EQ(cases[i].expected_next, next);
    EXPECT_TRUE(render_text->IsValidCursorIndex(next));

    size_t previous = render_text->IndexOfAdjacentGrapheme(cases[i].index,
                                                           CURSOR_BACKWARD);
    EXPECT_EQ(cases[i].expected_previous, previous);
    EXPECT_TRUE(render_text->IsValidCursorIndex(previous));
  }
}

TEST_F(RenderTextTest, MidGraphemeSelectionBounds) {
#if defined(OS_WIN)
  // TODO(msw): XP fails due to lack of font support: http://crbug.com/106450
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  // Test that selection bounds may be set amid multi-character graphemes.
  const base::string16 kHindi = WideToUTF16(L"\x0915\x093f");
  const base::string16 kThai = WideToUTF16(L"\x0e08\x0e33");
  const base::string16 cases[] = { kHindi, kThai };

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  for (size_t i = 0; i < arraysize(cases); i++) {
    SCOPED_TRACE(base::StringPrintf("Testing cases[%" PRIuS "]", i));
    render_text->SetText(cases[i]);
    EXPECT_TRUE(render_text->IsValidLogicalIndex(1));
#if !defined(OS_MACOSX)
    EXPECT_FALSE(render_text->IsValidCursorIndex(1));
#endif
    EXPECT_TRUE(render_text->SelectRange(Range(2, 1)));
    EXPECT_EQ(Range(2, 1), render_text->selection());
    EXPECT_EQ(1U, render_text->cursor_position());
    // Although selection bounds may be set within a multi-character grapheme,
    // cursor movement (e.g. via arrow key) should avoid those indices.
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
    EXPECT_EQ(0U, render_text->cursor_position());
    render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
    EXPECT_EQ(2U, render_text->cursor_position());
  }
}

TEST_F(RenderTextTest, FindCursorPosition) {
  const wchar_t* kTestStrings[] = { kLtrRtl, kLtrRtlLtr, kRtlLtr, kRtlLtrRtl };
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetDisplayRect(Rect(0, 0, 100, 20));
  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case[%" PRIuS "]", i));
    render_text->SetText(WideToUTF16(kTestStrings[i]));
    for(size_t j = 0; j < render_text->text().length(); ++j) {
      const Range range(render_text->GetGlyphBounds(j));
      // Test a point just inside the leading edge of the glyph bounds.
      int x = range.is_reversed() ? range.GetMax() - 1 : range.GetMin() + 1;
      EXPECT_EQ(j, render_text->FindCursorPosition(Point(x, 0)).caret_pos());
    }
  }
}

TEST_F(RenderTextTest, EdgeSelectionModels) {
  // Simple Latin text.
  const base::string16 kLatin = WideToUTF16(L"abc");
  // LTR 2-character grapheme.
  const base::string16 kLTRGrapheme = WideToUTF16(L"\x0915\x093f");
  // LTR 2-character grapheme, LTR a, LTR 2-character grapheme.
  const base::string16 kHindiLatin =
      WideToUTF16(L"\x0915\x093f" L"a" L"\x0915\x093f");
  // RTL 2-character grapheme.
  const base::string16 kRTLGrapheme = WideToUTF16(L"\x05e0\x05b8");
  // RTL 2-character grapheme, LTR a, RTL 2-character grapheme.
  const base::string16 kHebrewLatin =
      WideToUTF16(L"\x05e0\x05b8" L"a" L"\x05e0\x05b8");

  struct {
    base::string16 text;
    base::i18n::TextDirection expected_text_direction;
  } cases[] = {
    { base::string16(), base::i18n::LEFT_TO_RIGHT },
    { kLatin,       base::i18n::LEFT_TO_RIGHT },
    { kLTRGrapheme, base::i18n::LEFT_TO_RIGHT },
    { kHindiLatin,  base::i18n::LEFT_TO_RIGHT },
    { kRTLGrapheme, base::i18n::RIGHT_TO_LEFT },
    { kHebrewLatin, base::i18n::RIGHT_TO_LEFT },
  };

#if defined(OS_WIN)
  // TODO(msw): XP fails due to lack of font support: http://crbug.com/106450
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return;
#endif

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    render_text->SetText(cases[i].text);
    bool ltr = (cases[i].expected_text_direction == base::i18n::LEFT_TO_RIGHT);

    SelectionModel start_edge =
        render_text->EdgeSelectionModel(ltr ? CURSOR_LEFT : CURSOR_RIGHT);
    EXPECT_EQ(start_edge, SelectionModel(0, CURSOR_BACKWARD));

    SelectionModel end_edge =
        render_text->EdgeSelectionModel(ltr ? CURSOR_RIGHT : CURSOR_LEFT);
    EXPECT_EQ(end_edge, SelectionModel(cases[i].text.length(), CURSOR_FORWARD));
  }
}

TEST_F(RenderTextTest, SelectAll) {
  const wchar_t* const cases[] =
      { kWeak, kLtr, kLtrRtl, kLtrRtlLtr, kRtl, kRtlLtr, kRtlLtrRtl };

  // Ensure that SelectAll respects the |reversed| argument regardless of
  // application locale and text content directionality.
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  const SelectionModel expected_reversed(Range(3, 0), CURSOR_FORWARD);
  const SelectionModel expected_forwards(Range(0, 3), CURSOR_BACKWARD);
  const bool was_rtl = base::i18n::IsRTL();

  for (size_t i = 0; i < 2; ++i) {
    SetRTL(!base::i18n::IsRTL());
    // Test that an empty string produces an empty selection model.
    render_text->SetText(base::string16());
    EXPECT_EQ(render_text->selection_model(), SelectionModel());

    // Test the weak, LTR, RTL, and Bidi string cases.
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(cases); j++) {
      render_text->SetText(WideToUTF16(cases[j]));
      render_text->SelectAll(false);
      EXPECT_EQ(render_text->selection_model(), expected_forwards);
      render_text->SelectAll(true);
      EXPECT_EQ(render_text->selection_model(), expected_reversed);
    }
  }

  EXPECT_EQ(was_rtl, base::i18n::IsRTL());
}

  TEST_F(RenderTextTest, MoveCursorLeftRightWithSelection) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(WideToUTF16(L"abc\x05d0\x05d1\x05d2"));
  // Left arrow on select ranging (6, 4).
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(Range(4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(Range(5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(Range(6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, true);
  EXPECT_EQ(Range(6, 5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, true);
  EXPECT_EQ(Range(6, 4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(Range(6), render_text->selection());

  // Right arrow on select ranging (4, 6).
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(Range(0), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(1), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(2), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(3), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(4), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, true);
  EXPECT_EQ(Range(4, 5), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, true);
  EXPECT_EQ(Range(4, 6), render_text->selection());
  render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(Range(4), render_text->selection());
}
#endif  // !defined(OS_MACOSX)

// TODO(xji): Make these work on Windows.
#if defined(OS_LINUX)
void MoveLeftRightByWordVerifier(RenderText* render_text,
                                 const wchar_t* str) {
  render_text->SetText(WideToUTF16(str));

  // Test moving by word from left ro right.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  bool first_word = true;
  while (true) {
    // First, test moving by word from a word break position, such as from
    // "|abc def" to "abc| def".
    SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
    SelectionModel end = render_text->selection_model();
    if (end == start)  // reach the end.
      break;

    // For testing simplicity, each word is a 3-character word.
    int num_of_character_moves = first_word ? 3 : 4;
    first_word = false;
    render_text->MoveCursorTo(start);
    for (int j = 0; j < num_of_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
    EXPECT_EQ(end, render_text->selection_model());

    // Then, test moving by word from positions inside the word, such as from
    // "a|bc def" to "abc| def", and from "ab|c def" to "abc| def".
    for (int j = 1; j < num_of_character_moves; ++j) {
      render_text->MoveCursorTo(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_RIGHT, false);
      render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }

  // Test moving by word from right to left.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  first_word = true;
  while (true) {
    SelectionModel start = render_text->selection_model();
    render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
    SelectionModel end = render_text->selection_model();
    if (end == start)  // reach the end.
      break;

    int num_of_character_moves = first_word ? 3 : 4;
    first_word = false;
    render_text->MoveCursorTo(start);
    for (int j = 0; j < num_of_character_moves; ++j)
      render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
    EXPECT_EQ(end, render_text->selection_model());

    for (int j = 1; j < num_of_character_moves; ++j) {
      render_text->MoveCursorTo(start);
      for (int k = 0; k < j; ++k)
        render_text->MoveCursor(CHARACTER_BREAK, CURSOR_LEFT, false);
      render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
      EXPECT_EQ(end, render_text->selection_model());
    }
  }
}

TEST_F(RenderTextTest, MoveLeftRightByWordInBidiText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  // For testing simplicity, each word is a 3-character word.
  std::vector<const wchar_t*> test;
  test.push_back(L"abc");
  test.push_back(L"abc def");
  test.push_back(L"\x05E1\x05E2\x05E3");
  test.push_back(L"\x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6");
  test.push_back(L"abc \x05E1\x05E2\x05E3");
  test.push_back(L"abc def \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6");
  test.push_back(L"abc def hij \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6"
                 L" \x05E7\x05E8\x05E9");

  test.push_back(L"abc \x05E1\x05E2\x05E3 hij");
  test.push_back(L"abc def \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6 hij opq");
  test.push_back(L"abc def hij \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6"
                 L" \x05E7\x05E8\x05E9" L" opq rst uvw");

  test.push_back(L"\x05E1\x05E2\x05E3 abc");
  test.push_back(L"\x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6 abc def");
  test.push_back(L"\x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6 \x05E7\x05E8\x05E9"
                 L" abc def hij");

  test.push_back(L"\x05D1\x05D2\x05D3 abc \x05E1\x05E2\x05E3");
  test.push_back(L"\x05D1\x05D2\x05D3 \x05D4\x05D5\x05D6 abc def"
                 L" \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6");
  test.push_back(L"\x05D1\x05D2\x05D3 \x05D4\x05D5\x05D6 \x05D7\x05D8\x05D9"
                 L" abc def hij \x05E1\x05E2\x05E3 \x05E4\x05E5\x05E6"
                 L" \x05E7\x05E8\x05E9");

  for (size_t i = 0; i < test.size(); ++i)
    MoveLeftRightByWordVerifier(render_text.get(), test[i]);
}

TEST_F(RenderTextTest, MoveLeftRightByWordInBidiText_TestEndOfText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  render_text->SetText(WideToUTF16(L"ab\x05E1"));
  // Moving the cursor by word from "abC|" to the left should return "|abC".
  // But since end of text is always treated as a word break, it returns
  // position "ab|C".
  // TODO(xji): Need to make it work as expected.
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
  // EXPECT_EQ(SelectionModel(), render_text->selection_model());

  // Moving the cursor by word from "|abC" to the right returns "abC|".
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(SelectionModel(3, CURSOR_FORWARD), render_text->selection_model());

  render_text->SetText(WideToUTF16(L"\x05E1\x05E2" L"a"));
  // For logical text "BCa", moving the cursor by word from "aCB|" to the left
  // returns "|aCB".
  render_text->MoveCursor(LINE_BREAK, CURSOR_RIGHT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(SelectionModel(3, CURSOR_FORWARD), render_text->selection_model());

  // Moving the cursor by word from "|aCB" to the right should return "aCB|".
  // But since end of text is always treated as a word break, it returns
  // position "a|CB".
  // TODO(xji): Need to make it work as expected.
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  // EXPECT_EQ(SelectionModel(), render_text->selection_model());
}

TEST_F(RenderTextTest, MoveLeftRightByWordInTextWithMultiSpaces) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(WideToUTF16(L"abc     def"));
  render_text->MoveCursorTo(SelectionModel(5, CURSOR_FORWARD));
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(11U, render_text->cursor_position());

  render_text->MoveCursorTo(SelectionModel(5, CURSOR_FORWARD));
  render_text->MoveCursor(WORD_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
}

TEST_F(RenderTextTest, MoveLeftRightByWordInChineseText) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(WideToUTF16(L"\x6211\x4EEC\x53BB\x516C\x56ED\x73A9"));
  render_text->MoveCursor(LINE_BREAK, CURSOR_LEFT, false);
  EXPECT_EQ(0U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(2U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(3U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(5U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(6U, render_text->cursor_position());
  render_text->MoveCursor(WORD_BREAK, CURSOR_RIGHT, false);
  EXPECT_EQ(6U, render_text->cursor_position());
}
#endif

#if defined(OS_WIN)
TEST_F(RenderTextTest, Win_LogicalClusters) {
  scoped_ptr<RenderTextWin> render_text(
      static_cast<RenderTextWin*>(RenderText::CreateInstance()));

  const base::string16 test_string =
      WideToUTF16(L"\x0930\x0930\x0930\x0930\x0930");
  render_text->SetText(test_string);
  render_text->EnsureLayout();
  ASSERT_EQ(1U, render_text->runs_.size());
  WORD* logical_clusters = render_text->runs_[0]->logical_clusters.get();
  for (size_t i = 0; i < test_string.length(); ++i)
    EXPECT_EQ(i, logical_clusters[i]);
}
#endif  // defined(OS_WIN)

TEST_F(RenderTextTest, StringSizeSanity) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(UTF8ToUTF16("Hello World"));
  const Size string_size = render_text->GetStringSize();
  EXPECT_GT(string_size.width(), 0);
  EXPECT_GT(string_size.height(), 0);
}

// TODO(asvitkine): This test fails because PlatformFontMac uses point font
//                  sizes instead of pixel sizes like other implementations.
#if !defined(OS_MACOSX)
TEST_F(RenderTextTest, StringSizeEmptyString) {
  // Ascent and descent of Arial and Symbol are different on most platforms.
  const FontList font_list("Arial,Symbol, 16px");
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetFontList(font_list);
  render_text->SetDisplayRect(Rect(0, 0, 0, font_list.GetHeight()));

  // The empty string respects FontList metrics for non-zero height
  // and baseline.
  render_text->SetText(base::string16());
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(0, render_text->GetStringSize().width());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());

  render_text->SetText(UTF8ToUTF16(" "));
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());
}
#endif  // !defined(OS_MACOSX)

TEST_F(RenderTextTest, StringSizeRespectsFontListMetrics) {
  // Check that Arial and Symbol have different font metrics.
  Font arial_font("Arial", 16);
  ASSERT_EQ("arial",
            StringToLowerASCII(arial_font.GetActualFontNameForTesting()));
  Font symbol_font("Symbol", 16);
  ASSERT_EQ("symbol",
            StringToLowerASCII(symbol_font.GetActualFontNameForTesting()));
  EXPECT_NE(arial_font.GetHeight(), symbol_font.GetHeight());
  EXPECT_NE(arial_font.GetBaseline(), symbol_font.GetBaseline());
  // "a" should be rendered with Arial, not with Symbol.
  const char* arial_font_text = "a";
  // "®" (registered trademark symbol) should be rendered with Symbol,
  // not with Arial.
  const char* symbol_font_text = "\xC2\xAE";

  Font smaller_font = arial_font;
  Font larger_font = symbol_font;
  const char* smaller_font_text = arial_font_text;
  const char* larger_font_text = symbol_font_text;
  if (symbol_font.GetHeight() < arial_font.GetHeight() &&
      symbol_font.GetBaseline() < arial_font.GetBaseline()) {
    std::swap(smaller_font, larger_font);
    std::swap(smaller_font_text, larger_font_text);
  }
  ASSERT_LT(smaller_font.GetHeight(), larger_font.GetHeight());
  ASSERT_LT(smaller_font.GetBaseline(), larger_font.GetBaseline());

  // Check |smaller_font_text| is rendered with the smaller font.
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(UTF8ToUTF16(smaller_font_text));
  render_text->SetFontList(FontList(smaller_font));
  render_text->SetDisplayRect(Rect(0, 0, 0,
                                   render_text->font_list().GetHeight()));
  EXPECT_EQ(smaller_font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(smaller_font.GetBaseline(), render_text->GetBaseline());

  // Layout the same text with mixed fonts.  The text should be rendered with
  // the smaller font, but the height and baseline are determined with the
  // metrics of the font list, which is equal to the larger font.
  std::vector<Font> fonts;
  fonts.push_back(smaller_font);  // The primary font is the smaller font.
  fonts.push_back(larger_font);
  const FontList font_list(fonts);
  render_text->SetFontList(font_list);
  render_text->SetDisplayRect(Rect(0, 0, 0,
                                   render_text->font_list().GetHeight()));
  EXPECT_LT(smaller_font.GetHeight(), render_text->GetStringSize().height());
  EXPECT_LT(smaller_font.GetBaseline(), render_text->GetBaseline());
  EXPECT_EQ(font_list.GetHeight(), render_text->GetStringSize().height());
  EXPECT_EQ(font_list.GetBaseline(), render_text->GetBaseline());
}

TEST_F(RenderTextTest, SetFontList) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetFontList(FontList("Arial,Symbol, 13px"));
  const std::vector<Font>& fonts = render_text->font_list().GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("Arial", fonts[0].GetFontName());
  EXPECT_EQ("Symbol", fonts[1].GetFontName());
  EXPECT_EQ(13, render_text->font_list().GetFontSize());
}

TEST_F(RenderTextTest, StringSizeBoldWidth) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(UTF8ToUTF16("Hello World"));

  const int plain_width = render_text->GetStringSize().width();
  EXPECT_GT(plain_width, 0);

  // Apply a bold style and check that the new width is greater.
  render_text->SetStyle(BOLD, true);
  const int bold_width = render_text->GetStringSize().width();
  EXPECT_GT(bold_width, plain_width);

  // Now, apply a plain style over the first word only.
  render_text->ApplyStyle(BOLD, false, Range(0, 5));
  const int plain_bold_width = render_text->GetStringSize().width();
  EXPECT_GT(plain_bold_width, plain_width);
  EXPECT_LT(plain_bold_width, bold_width);
}

TEST_F(RenderTextTest, StringSizeHeight) {
  base::string16 cases[] = {
    WideToUTF16(L"Hello World!"),  // English
    WideToUTF16(L"\x6328\x62f6"),  // Japanese
    WideToUTF16(L"\x0915\x093f"),  // Hindi
    WideToUTF16(L"\x05e0\x05b8"),  // Hebrew
  };

  const FontList default_font_list;
  const FontList& larger_font_list = default_font_list.DeriveWithSizeDelta(24);
  EXPECT_GT(larger_font_list.GetHeight(), default_font_list.GetHeight());

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
    render_text->SetFontList(default_font_list);
    render_text->SetText(cases[i]);

    const int height1 = render_text->GetStringSize().height();
    EXPECT_GT(height1, 0);

    // Check that setting the larger font increases the height.
    render_text->SetFontList(larger_font_list);
    const int height2 = render_text->GetStringSize().height();
    EXPECT_GT(height2, height1);
  }
}

TEST_F(RenderTextTest, GetBaselineSanity) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(UTF8ToUTF16("Hello World"));
  const int baseline = render_text->GetBaseline();
  EXPECT_GT(baseline, 0);
}

TEST_F(RenderTextTest, CursorBoundsInReplacementMode) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetDisplayRect(Rect(100, 17));
  SelectionModel sel_b(1, CURSOR_FORWARD);
  SelectionModel sel_c(2, CURSOR_FORWARD);
  Rect cursor_around_b = render_text->GetCursorBounds(sel_b, false);
  Rect cursor_before_b = render_text->GetCursorBounds(sel_b, true);
  Rect cursor_before_c = render_text->GetCursorBounds(sel_c, true);
  EXPECT_EQ(cursor_around_b.x(), cursor_before_b.x());
  EXPECT_EQ(cursor_around_b.right(), cursor_before_c.x());
}

TEST_F(RenderTextTest, GetTextOffset) {
  // The default horizontal text offset differs for LTR and RTL, and is only set
  // when the RenderText object is created.  This test will check the default in
  // LTR mode, and the next test will check the RTL default.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(false);
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));

  // Set display area's size equal to the font size.
  const Size font_size(render_text->GetContentWidth(),
                       render_text->font_list().GetHeight());
  Rect display_rect(font_size);
  render_text->SetDisplayRect(display_rect);

  Vector2d offset = render_text->GetLineOffset(0);
  EXPECT_TRUE(offset.IsZero());

  const int kEnlargementX = 2;
  display_rect.Inset(0, 0, -kEnlargementX, 0);
  render_text->SetDisplayRect(display_rect);

  // Check the default horizontal alignment.
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(0, offset.x());

  // Check explicitly setting the horizontal alignment.
  render_text->SetHorizontalAlignment(ALIGN_LEFT);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(0, offset.x());
  render_text->SetHorizontalAlignment(ALIGN_CENTER);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementX / 2, offset.x());
  render_text->SetHorizontalAlignment(ALIGN_RIGHT);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargementX, offset.x());

  // Check that text is vertically centered within taller display rects.
  const int kEnlargementY = display_rect.height();
  display_rect.Inset(0, 0, 0, -kEnlargementY);
  render_text->SetDisplayRect(display_rect);
  const Vector2d prev_offset = render_text->GetLineOffset(0);
  display_rect.Inset(0, 0, 0, -2 * kEnlargementY);
  render_text->SetDisplayRect(display_rect);
  offset = render_text->GetLineOffset(0);
  EXPECT_EQ(prev_offset.y() + kEnlargementY, offset.y());

  SetRTL(was_rtl);
}

TEST_F(RenderTextTest, GetTextOffsetHorizontalDefaultInRTL) {
  // This only checks the default horizontal alignment in RTL mode; all other
  // GetLineOffset(0) attributes are checked by the test above.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(true);
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  render_text->SetFontList(FontList("Arial, 13px"));
  const int kEnlargement = 2;
  const Size font_size(render_text->GetContentWidth() + kEnlargement,
                       render_text->GetStringSize().height());
  Rect display_rect(font_size);
  render_text->SetDisplayRect(display_rect);
  Vector2d offset = render_text->GetLineOffset(0);
  EXPECT_EQ(kEnlargement, offset.x());
  SetRTL(was_rtl);
}

TEST_F(RenderTextTest, SameFontForParentheses) {
  struct {
    const base::char16 left_char;
    const base::char16 right_char;
  } punctuation_pairs[] = {
    { '(', ')' },
    { '{', '}' },
    { '<', '>' },
  };
  struct {
    base::string16 text;
  } cases[] = {
    // English(English)
    { WideToUTF16(L"Hello World(a)") },
    // English(English)English
    { WideToUTF16(L"Hello World(a)Hello World") },

    // Japanese(English)
    { WideToUTF16(L"\x6328\x62f6(a)") },
    // Japanese(English)Japanese
    { WideToUTF16(L"\x6328\x62f6(a)\x6328\x62f6") },
    // English(Japanese)English
    { WideToUTF16(L"Hello World(\x6328\x62f6)Hello World") },

    // Hindi(English)
    { WideToUTF16(L"\x0915\x093f(a)") },
    // Hindi(English)Hindi
    { WideToUTF16(L"\x0915\x093f(a)\x0915\x093f") },
    // English(Hindi)English
    { WideToUTF16(L"Hello World(\x0915\x093f)Hello World") },

    // Hebrew(English)
    { WideToUTF16(L"\x05e0\x05b8(a)") },
    // Hebrew(English)Hebrew
    { WideToUTF16(L"\x05e0\x05b8(a)\x05e0\x05b8") },
    // English(Hebrew)English
    { WideToUTF16(L"Hello World(\x05e0\x05b8)Hello World") },
  };

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    base::string16 text = cases[i].text;
    const size_t start_paren_char_index = text.find('(');
    ASSERT_NE(base::string16::npos, start_paren_char_index);
    const size_t end_paren_char_index = text.find(')');
    ASSERT_NE(base::string16::npos, end_paren_char_index);

    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(punctuation_pairs); ++j) {
      text[start_paren_char_index] = punctuation_pairs[j].left_char;
      text[end_paren_char_index] = punctuation_pairs[j].right_char;
      render_text->SetText(text);

      const std::vector<RenderText::FontSpan> spans =
          render_text->GetFontSpansForTesting();

      int start_paren_span_index = -1;
      int end_paren_span_index = -1;
      for (size_t k = 0; k < spans.size(); ++k) {
        if (IndexInRange(spans[k].second, start_paren_char_index))
          start_paren_span_index = k;
        if (IndexInRange(spans[k].second, end_paren_char_index))
          end_paren_span_index = k;
      }
      ASSERT_NE(-1, start_paren_span_index);
      ASSERT_NE(-1, end_paren_span_index);

      const Font& start_font = spans[start_paren_span_index].first;
      const Font& end_font = spans[end_paren_span_index].first;
      EXPECT_EQ(start_font.GetFontName(), end_font.GetFontName());
      EXPECT_EQ(start_font.GetFontSize(), end_font.GetFontSize());
      EXPECT_EQ(start_font.GetStyle(), end_font.GetStyle());
    }
  }
}

// Make sure the caret width is always >=1 so that the correct
// caret is drawn at high DPI. crbug.com/164100.
TEST_F(RenderTextTest, CaretWidth) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16("abcdefg"));
  EXPECT_GE(render_text->GetUpdatedCursorBounds().width(), 1);
}

TEST_F(RenderTextTest, SelectWord) {
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(ASCIIToUTF16(" foo  a.bc.d bar"));

  struct {
    size_t cursor;
    size_t selection_start;
    size_t selection_end;
  } cases[] = {
    { 0,   0,  1 },
    { 1,   1,  4 },
    { 2,   1,  4 },
    { 3,   1,  4 },
    { 4,   4,  6 },
    { 5,   4,  6 },
    { 6,   6,  7 },
    { 7,   7,  8 },
    { 8,   8, 10 },
    { 9,   8, 10 },
    { 10, 10, 11 },
    { 11, 11, 12 },
    { 12, 12, 13 },
    { 13, 13, 16 },
    { 14, 13, 16 },
    { 15, 13, 16 },
    { 16, 13, 16 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    render_text->SetCursorPosition(cases[i].cursor);
    render_text->SelectWord();
    EXPECT_EQ(Range(cases[i].selection_start, cases[i].selection_end),
              render_text->selection());
  }
}

// Make sure the last word is selected when the cursor is at text.length().
TEST_F(RenderTextTest, LastWordSelected) {
  const std::string kTestURL1 = "http://www.google.com";
  const std::string kTestURL2 = "http://www.google.com/something/";

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  render_text->SetText(ASCIIToUTF16(kTestURL1));
  render_text->SetCursorPosition(kTestURL1.length());
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("com"), GetSelectedText(render_text.get()));
  EXPECT_FALSE(render_text->selection().is_reversed());

  render_text->SetText(ASCIIToUTF16(kTestURL2));
  render_text->SetCursorPosition(kTestURL2.length());
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("/"), GetSelectedText(render_text.get()));
  EXPECT_FALSE(render_text->selection().is_reversed());
}

// When given a non-empty selection, SelectWord should expand the selection to
// nearest word boundaries.
TEST_F(RenderTextTest, SelectMultipleWords) {
  const std::string kTestURL = "http://www.google.com";

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  render_text->SetText(ASCIIToUTF16(kTestURL));
  render_text->SelectRange(Range(16, 20));
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("google.com"), GetSelectedText(render_text.get()));
  EXPECT_FALSE(render_text->selection().is_reversed());

  // SelectWord should preserve the selection direction.
  render_text->SelectRange(Range(20, 16));
  render_text->SelectWord();
  EXPECT_EQ(ASCIIToUTF16("google.com"), GetSelectedText(render_text.get()));
  EXPECT_TRUE(render_text->selection().is_reversed());
}

// TODO(asvitkine): Cursor movements tests disabled on Mac because RenderTextMac
//                  does not implement this yet. http://crbug.com/131618
#if !defined(OS_MACOSX)
TEST_F(RenderTextTest, DisplayRectShowsCursorLTR) {
  ASSERT_FALSE(base::i18n::IsRTL());
  ASSERT_FALSE(base::i18n::ICUIsRTL());

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(WideToUTF16(L"abcdefghijklmnopqrstuvwxzyabcdefg"));
  render_text->MoveCursorTo(SelectionModel(render_text->text().length(),
                                           CURSOR_FORWARD));
  int width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Repeat the test with RTL text.
  render_text->SetText(WideToUTF16(L"\x5d0\x5d1\x5d2\x5d3\x5d4\x5d5\x5d6\x5d7"
      L"\x5d8\x5d9\x5da\x5db\x5dc\x5dd\x5de\x5df"));
  render_text->MoveCursorTo(SelectionModel(0, CURSOR_FORWARD));
  width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(render_text->display_rect().width(),
            render_text->GetUpdatedCursorBounds().right());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(width, render_text->GetUpdatedCursorBounds().x());
}

TEST_F(RenderTextTest, DisplayRectShowsCursorRTL) {
  // Set the application default text direction to RTL.
  const bool was_rtl = base::i18n::IsRTL();
  SetRTL(true);

  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->SetText(WideToUTF16(L"abcdefghijklmnopqrstuvwxzyabcdefg"));
  render_text->MoveCursorTo(SelectionModel(0, CURSOR_FORWARD));
  int width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Repeat the test with RTL text.
  render_text->SetText(WideToUTF16(L"\x5d0\x5d1\x5d2\x5d3\x5d4\x5d5\x5d6\x5d7"
      L"\x5d8\x5d9\x5da\x5db\x5dc\x5dd\x5de\x5df"));
  render_text->MoveCursorTo(SelectionModel(render_text->text().length(),
                                           CURSOR_FORWARD));
  width = render_text->GetStringSize().width();
  ASSERT_GT(width, 10);

  // Ensure that the cursor is placed at the width of its preceding text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Ensure that shrinking the display rectangle keeps the cursor in view.
  render_text->SetDisplayRect(Rect(width - 10, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that the text will pan to fill its expanding display rectangle.
  render_text->SetDisplayRect(Rect(width - 5, 1));
  EXPECT_EQ(0, render_text->GetUpdatedCursorBounds().x());

  // Ensure that a sufficiently large display rectangle shows all the text.
  render_text->SetDisplayRect(Rect(width + 10, 1));
  EXPECT_EQ(render_text->display_rect().width() - width - 1,
            render_text->GetUpdatedCursorBounds().x());

  // Reset the application default text direction to LTR.
  SetRTL(was_rtl);
  EXPECT_EQ(was_rtl, base::i18n::IsRTL());
}
#endif  // !defined(OS_MACOSX)

// Changing colors between or inside ligated glyphs should not break shaping.
TEST_F(RenderTextTest, SelectionKeepsLigatures) {
  const wchar_t* kTestStrings[] = { L"\x644\x623", L"\x633\x627" };
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());
  render_text->set_selection_color(SK_ColorRED);
  Canvas canvas;

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    render_text->SetText(WideToUTF16(kTestStrings[i]));
    const int expected_width = render_text->GetStringSize().width();
    render_text->MoveCursorTo(SelectionModel(Range(0, 1), CURSOR_FORWARD));
    EXPECT_EQ(expected_width, render_text->GetStringSize().width());
    // Drawing the text should not DCHECK or crash; see http://crbug.com/262119
    render_text->Draw(&canvas);
    render_text->MoveCursorTo(SelectionModel(0, CURSOR_FORWARD));
  }
}

#if defined(OS_WIN)
// Ensure strings wrap onto multiple lines for a small available width.
TEST_F(RenderTextTest, Multiline_MinWidth) {
  const wchar_t* kTestStrings[] = { kWeak, kLtr, kLtrRtl, kLtrRtlLtr, kRtl,
                                    kRtlLtr, kRtlLtrRtl };

  scoped_ptr<RenderTextWin> render_text(
      static_cast<RenderTextWin*>(RenderText::CreateInstance()));
  render_text->SetDisplayRect(Rect(1, 1000));
  render_text->SetMultiline(true);
  Canvas canvas;

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(WideToUTF16(kTestStrings[i]));
    render_text->Draw(&canvas);
    EXPECT_GT(render_text->lines_.size(), 1U);
  }
}

// Ensure strings wrap onto multiple lines for a normal available width.
TEST_F(RenderTextTest, Multiline_NormalWidth) {
  const struct {
    const wchar_t* const text;
    const Range first_line_char_range;
    const Range second_line_char_range;
  } kTestStrings[] = {
    { L"abc defg hijkl", Range(0, 9), Range(9, 14) },
    { L"qwertyzxcvbn", Range(0, 8), Range(8, 12) },
    { L"\x062A\x0641\x0627\x062D\x05EA\x05E4\x05D5\x05D6\x05D9\x05DD",
          Range(4, 10), Range(0, 4) }
  };

  scoped_ptr<RenderTextWin> render_text(
      static_cast<RenderTextWin*>(RenderText::CreateInstance()));
  render_text->SetDisplayRect(Rect(50, 1000));
  render_text->SetMultiline(true);
  Canvas canvas;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(WideToUTF16(kTestStrings[i].text));
    render_text->Draw(&canvas);
    ASSERT_EQ(2U, render_text->lines_.size());
    ASSERT_EQ(1U, render_text->lines_[0].segments.size());
    EXPECT_EQ(kTestStrings[i].first_line_char_range,
              render_text->lines_[0].segments[0].char_range);
    ASSERT_EQ(1U, render_text->lines_[1].segments.size());
    EXPECT_EQ(kTestStrings[i].second_line_char_range,
              render_text->lines_[1].segments[0].char_range);
  }
}

// Ensure strings don't wrap onto multiple lines for a sufficient available
// width.
TEST_F(RenderTextTest, Multiline_SufficientWidth) {
  const wchar_t* kTestStrings[] = { L"", L" ", L".", L" . ", L"abc", L"a b c",
                                    L"\x62E\x628\x632", L"\x62E \x628 \x632" };

  scoped_ptr<RenderTextWin> render_text(
      static_cast<RenderTextWin*>(RenderText::CreateInstance()));
  render_text->SetDisplayRect(Rect(30, 1000));
  render_text->SetMultiline(true);
  Canvas canvas;

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(WideToUTF16(kTestStrings[i]));
    render_text->Draw(&canvas);
    EXPECT_EQ(1U, render_text->lines_.size());
  }
}

TEST_F(RenderTextTest, Multiline_Newline) {
  const struct {
    const wchar_t* const text;
    // Ranges of the characters on each line preceding the newline.
    const Range first_line_char_range;
    const Range second_line_char_range;
  } kTestStrings[] = {
    { L"abc\ndef", Range(0, 3), Range(4, 7) },
    { L"a \n b ", Range(0, 2), Range(3, 6) },
    { L"\n" , Range::InvalidRange(), Range::InvalidRange() }
  };

  scoped_ptr<RenderTextWin> render_text(
      static_cast<RenderTextWin*>(RenderText::CreateInstance()));
  render_text->SetDisplayRect(Rect(200, 1000));
  render_text->SetMultiline(true);
  Canvas canvas;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestStrings); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestStrings[%" PRIuS "]", i));
    render_text->SetText(WideToUTF16(kTestStrings[i].text));
    render_text->Draw(&canvas);

    ASSERT_EQ(2U, render_text->lines_.size());

    const Range first_expected_range = kTestStrings[i].first_line_char_range;
    ASSERT_EQ(first_expected_range.IsValid() ? 2U : 1U,
              render_text->lines_[0].segments.size());
    if (first_expected_range.IsValid())
      EXPECT_EQ(first_expected_range,
                render_text->lines_[0].segments[0].char_range);

    const internal::LineSegment& newline_segment =
        render_text->lines_[0].segments[first_expected_range.IsValid() ? 1 : 0];
    ASSERT_EQ(1U, newline_segment.char_range.length());
    EXPECT_EQ(L'\n', kTestStrings[i].text[newline_segment.char_range.start()]);

    const Range second_expected_range = kTestStrings[i].second_line_char_range;
    ASSERT_EQ(second_expected_range.IsValid() ? 1U : 0U,
              render_text->lines_[1].segments.size());
    if (second_expected_range.IsValid())
      EXPECT_EQ(second_expected_range,
                render_text->lines_[1].segments[0].char_range);
  }
}

TEST_F(RenderTextTest, Win_BreakRunsByUnicodeBlocks) {
  scoped_ptr<RenderTextWin> render_text(
      static_cast<RenderTextWin*>(RenderText::CreateInstance()));

  // The '\x25B6' "play character" should break runs. http://crbug.com/278913
  render_text->SetText(WideToUTF16(L"x\x25B6y"));
  render_text->EnsureLayout();
  ASSERT_EQ(3U, render_text->runs_.size());
  EXPECT_EQ(Range(0, 1), render_text->runs_[0]->range);
  EXPECT_EQ(Range(1, 2), render_text->runs_[1]->range);
  EXPECT_EQ(Range(2, 3), render_text->runs_[2]->range);

  render_text->SetText(WideToUTF16(L"x \x25B6 y"));
  render_text->EnsureLayout();
  ASSERT_EQ(3U, render_text->runs_.size());
  EXPECT_EQ(Range(0, 2), render_text->runs_[0]->range);
  EXPECT_EQ(Range(2, 3), render_text->runs_[1]->range);
  EXPECT_EQ(Range(3, 5), render_text->runs_[2]->range);
}
#endif  // defined(OS_WIN)

TEST_F(RenderTextTest, HarfBuzz_CharToGlyph) {
  struct {
    uint32 glyph_to_char[4];
    size_t char_to_glyph_expected[4];
    Range char_range_to_glyph_range_expected[4];
    bool is_rtl;
  } cases[] = {
    { // From string "A B C D" to glyphs "a b c d".
      { 0, 1, 2, 3 },
      { 0, 1, 2, 3 },
      { Range(0, 1), Range(1, 2), Range(2, 3), Range(3, 4) },
      false
    },
    { // From string "A B C D" to glyphs "d b c a".
      { 3, 2, 1, 0 },
      { 3, 2, 1, 0 },
      { Range(3, 4), Range(2, 3), Range(1, 2), Range(0, 1) },
      true
    },
    { // From string "A B C D" to glyphs "ab c c d".
      { 0, 2, 2, 3 },
      { 0, 0, 1, 3 },
      { Range(0, 1), Range(0, 1), Range(1, 3), Range(3, 4) },
      false
    },
    { // From string "A B C D" to glyphs "d c c ba".
      { 3, 2, 2, 0 },
      { 3, 3, 1, 0 },
      { Range(3, 4), Range(3, 4), Range(1, 3), Range(0, 1) },
      true
    },
  };

  internal::TextRunHarfBuzz run;
  run.range = Range(0, 4);
  run.glyph_count = 4;
  run.glyph_to_char.reset(new uint32[4]);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    std::copy(cases[i].glyph_to_char, cases[i].glyph_to_char + 4,
              run.glyph_to_char.get());
    run.is_rtl = cases[i].is_rtl;
    for (size_t j = 0; j < 4; ++j) {
      SCOPED_TRACE(base::StringPrintf("Case %" PRIuS ", char %" PRIuS, i, j));
      EXPECT_EQ(cases[i].char_to_glyph_expected[j], run.CharToGlyph(j));
      EXPECT_EQ(cases[i].char_range_to_glyph_range_expected[j],
                run.CharRangeToGlyphRange(Range(j, j + 1)));
    }
  }
}

TEST_F(RenderTextTest, HarfBuzz_RunDirection) {
  RenderTextHarfBuzz render_text;
  const base::string16 mixed =
      WideToUTF16(L"\x05D0\x05D1" L"1234" L"\x05D2\x05D3");
  render_text.SetText(mixed);
  render_text.EnsureLayout();
  ASSERT_EQ(3U, render_text.runs_.size());
  EXPECT_TRUE(render_text.runs_[0]->is_rtl);
  EXPECT_FALSE(render_text.runs_[1]->is_rtl);
  EXPECT_TRUE(render_text.runs_[2]->is_rtl);
}

TEST_F(RenderTextTest, HarfBuzz_BreakRunsByUnicodeBlocks) {
  RenderTextHarfBuzz render_text;

  // The '\x25B6' "play character" should break runs. http://crbug.com/278913
  render_text.SetText(WideToUTF16(L"x\x25B6y"));
  render_text.EnsureLayout();
  ASSERT_EQ(3U, render_text.runs_.size());
  EXPECT_EQ(Range(0, 1), render_text.runs_[0]->range);
  EXPECT_EQ(Range(1, 2), render_text.runs_[1]->range);
  EXPECT_EQ(Range(2, 3), render_text.runs_[2]->range);

  render_text.SetText(WideToUTF16(L"x \x25B6 y"));
  render_text.EnsureLayout();
  ASSERT_EQ(3U, render_text.runs_.size());
  EXPECT_EQ(Range(0, 2), render_text.runs_[0]->range);
  EXPECT_EQ(Range(2, 3), render_text.runs_[1]->range);
  EXPECT_EQ(Range(3, 5), render_text.runs_[2]->range);
}

// Disabled on Mac because RenderTextMac doesn't implement GetGlyphBounds.
#if !defined(OS_MACOSX)
TEST_F(RenderTextTest, GlyphBounds) {
  const wchar_t* kTestStrings[] = {
      L"asdf 1234 qwer", L"\x0647\x0654", L"\x0645\x0631\x062D\x0628\x0627"
  };
  scoped_ptr<RenderText> render_text(RenderText::CreateInstance());

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    render_text->SetText(WideToUTF16(kTestStrings[i]));
    render_text->EnsureLayout();

    for (size_t j = 0; j < render_text->text().length(); ++j)
      EXPECT_FALSE(render_text->GetGlyphBounds(j).is_empty());
  }
}
#endif

// Remove this after making RTHB default in favor of RenderTextTest.GlyphBounds.
TEST_F(RenderTextTest, HarfBuzz_GlyphBounds) {
  const wchar_t* kTestStrings[] = {
      L"asdf 1234 qwer", L"\x0647\x0654", L"\x0645\x0631\x062D\x0628\x0627"
  };
  scoped_ptr<RenderText> render_text(new RenderTextHarfBuzz);

  for (size_t i = 0; i < arraysize(kTestStrings); ++i) {
    render_text->SetText(WideToUTF16(kTestStrings[i]));
    render_text->EnsureLayout();

    for (size_t j = 0; j < render_text->text().length(); ++j)
      EXPECT_FALSE(render_text->GetGlyphBounds(j).is_empty());
  }
}

}  // namespace gfx
