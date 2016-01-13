// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/auto_reset.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(base::ASCIIToUTF16(ascii), utf16)

namespace {

struct WordAndCursor {
  WordAndCursor(const wchar_t* w, size_t c) : word(w), cursor(c) {}

  const wchar_t* word;
  size_t cursor;
};

void MoveCursorTo(views::TextfieldModel& model, size_t pos) {
  model.MoveCursorTo(gfx::SelectionModel(pos, gfx::CURSOR_FORWARD));
}

}  // namespace

namespace views {

class TextfieldModelTest : public ViewsTestBase,
                           public TextfieldModel::Delegate {
 public:
  TextfieldModelTest()
      : ViewsTestBase(),
        composition_text_confirmed_or_cleared_(false) {
  }

  virtual void OnCompositionTextConfirmedOrCleared() OVERRIDE {
    composition_text_confirmed_or_cleared_ = true;
  }

 protected:
  void ResetModel(TextfieldModel* model) const {
    model->SetText(base::string16());
    model->ClearEditHistory();
  }

  bool composition_text_confirmed_or_cleared_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextfieldModelTest);
};

TEST_F(TextfieldModelTest, EditString) {
  TextfieldModel model(NULL);
  // Append two strings.
  model.Append(base::ASCIIToUTF16("HILL"));
  EXPECT_STR_EQ("HILL", model.text());
  model.Append(base::ASCIIToUTF16("WORLD"));
  EXPECT_STR_EQ("HILLWORLD", model.text());

  // Insert "E" and replace "I" with "L" to make "HELLO".
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  model.InsertChar('E');
  EXPECT_STR_EQ("HEILLWORLD", model.text());
  model.ReplaceChar('L');
  EXPECT_STR_EQ("HELLLWORLD", model.text());
  model.ReplaceChar('L');
  model.ReplaceChar('O');
  EXPECT_STR_EQ("HELLOWORLD", model.text());

  // Delete 6th char "W", then delete 5th char "O".
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("HELLOORLD", model.text());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_STR_EQ("HELLORLD", model.text());

  // Move the cursor to start; backspace should fail.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_FALSE(model.Backspace());
  EXPECT_STR_EQ("HELLORLD", model.text());
  // Move the cursor to the end; delete should fail, but backspace should work.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_FALSE(model.Delete());
  EXPECT_STR_EQ("HELLORLD", model.text());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("HELLORL", model.text());

  MoveCursorTo(model, 5);
  model.ReplaceText(base::ASCIIToUTF16(" WOR"));
  EXPECT_STR_EQ("HELLO WORL", model.text());
}

TEST_F(TextfieldModelTest, EditString_SimpleRTL) {
  TextfieldModel model(NULL);
  // Append two strings.
  model.Append(base::WideToUTF16(L"\x05d0\x05d1\x05d2"));
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05d1\x05d2"), model.text());
  model.Append(base::WideToUTF16(L"\x05e0\x05e1\x05e2"));
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05d1\x05d2\x05e0\x05e1\x05e2"),
            model.text());

  // Insert "\x05f0".
  MoveCursorTo(model, 1);
  model.InsertChar(0x05f0);
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x05d1\x05d2\x05e0\x05e1\x05e2"),
            model.text());

  // Replace "\x05d1" with "\x05f1".
  model.ReplaceChar(0x05f1);
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x5f1\x05d2\x05e0\x05e1\x05e2"),
            model.text());

  // Test Delete and backspace.
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x5f1\x05e0\x05e1\x05e2"),
            model.text());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_EQ(base::WideToUTF16(L"\x05d0\x05f0\x05e0\x05e1\x05e2"), model.text());
}

TEST_F(TextfieldModelTest, EditString_ComplexScript) {
  // TODO(msw): XP fails due to lack of font support: http://crbug.com/106450
  bool on_windows_xp = false;
#if defined(OS_WIN)
  on_windows_xp = base::win::GetVersion() < base::win::VERSION_VISTA;
#endif

  TextfieldModel model(NULL);

  // Append two Hindi strings.
  model.Append(base::WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"));
  EXPECT_EQ(base::WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"), model.text());
  model.Append(base::WideToUTF16(L"\x0915\x094d\x092e\x094d"));
  EXPECT_EQ(base::WideToUTF16(
      L"\x0915\x093f\x0915\x094d\x0915\x0915\x094d\x092e\x094d"), model.text());

  if (!on_windows_xp) {
    // Ensure the cursor cannot be placed in the middle of a grapheme.
    MoveCursorTo(model, 1);
    EXPECT_EQ(0U, model.GetCursorPosition());

    MoveCursorTo(model, 2);
    EXPECT_EQ(2U, model.GetCursorPosition());
    model.InsertChar('a');
    EXPECT_EQ(base::WideToUTF16(
        L"\x0915\x093f\x0061\x0915\x094d\x0915\x0915\x094d\x092e\x094d"),
        model.text());

    // ReplaceChar will replace the whole grapheme.
    model.ReplaceChar('b');
    // TODO(xji): temporarily disable in platform Win since the complex script
    // characters turned into empty square due to font regression. So, not able
    // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
    EXPECT_EQ(base::WideToUTF16(
        L"\x0915\x093f\x0061\x0062\x0915\x0915\x094d\x092e\x094d"),
        model.text());
#endif
    EXPECT_EQ(4U, model.GetCursorPosition());
  }

  // Delete should delete the whole grapheme.
  MoveCursorTo(model, 0);
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(base::WideToUTF16(L"\x0061\x0062\x0915\x0915\x094d\x092e\x094d"),
            model.text());
  MoveCursorTo(model, model.text().length());
  EXPECT_EQ(model.text().length(), model.GetCursorPosition());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(base::WideToUTF16(L"\x0061\x0062\x0915\x0915\x094d\x092e"),
            model.text());
#endif

  // Test cursor position and deletion for Hindi Virama.
  model.SetText(base::WideToUTF16(L"\x0D38\x0D4D\x0D15\x0D16\x0D2E"));
  MoveCursorTo(model, 0);
  EXPECT_EQ(0U, model.GetCursorPosition());

  if (!on_windows_xp) {
    MoveCursorTo(model, 1);
    EXPECT_EQ(0U, model.GetCursorPosition());
    MoveCursorTo(model, 3);
    EXPECT_EQ(3U, model.GetCursorPosition());
  }

  // TODO(asvitkine): Temporarily disable the following check on Windows. It
  // seems Windows treats "\x0D38\x0D4D\x0D15" as a single grapheme.
#if !defined(OS_WIN)
  MoveCursorTo(model, 2);
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(base::WideToUTF16(L"\x0D38\x0D15\x0D16\x0D2E"), model.text());
#endif

  model.SetText(
      base::WideToUTF16(L"\x05d5\x05b7\x05D9\x05B0\x05D4\x05B4\x05D9"));
  MoveCursorTo(model, 0);
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(base::WideToUTF16(L""), model.text());

  // The first 2 characters are not strong directionality characters.
  model.SetText(
      base::WideToUTF16(L"\x002C\x0020\x05D1\x05BC\x05B7\x05E9\x05BC"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(base::WideToUTF16(L"\x002C\x0020\x05D1\x05BC\x05B7\x05E9"),
            model.text());
}

TEST_F(TextfieldModelTest, EmptyString) {
  TextfieldModel model(NULL);
  EXPECT_EQ(base::string16(), model.text());
  EXPECT_EQ(base::string16(), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(0U, model.GetCursorPosition());

  EXPECT_EQ(base::string16(), model.GetSelectedText());

  EXPECT_FALSE(model.Delete());
  EXPECT_FALSE(model.Backspace());
}

TEST_F(TextfieldModelTest, Selection) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_STR_EQ("E", model.GetSelectedText());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_STR_EQ("EL", model.GetSelectedText());

  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_STR_EQ("H", model.GetSelectedText());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_STR_EQ("ELLO", model.GetSelectedText());
  model.ClearSelection();
  EXPECT_EQ(base::string16(), model.GetSelectedText());

  // SelectAll(false) selects towards the end.
  model.SelectAll(false);
  EXPECT_STR_EQ("HELLO", model.GetSelectedText());
  EXPECT_EQ(gfx::Range(0, 5), model.render_text()->selection());

  // SelectAll(true) selects towards the beginning.
  model.SelectAll(true);
  EXPECT_STR_EQ("HELLO", model.GetSelectedText());
  EXPECT_EQ(gfx::Range(5, 0), model.render_text()->selection());

  // Select and move cursor.
  model.SelectRange(gfx::Range(1U, 3U));
  EXPECT_STR_EQ("EL", model.GetSelectedText());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SelectRange(gfx::Range(1U, 3U));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Select all and move cursor.
  model.SelectAll(false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.SelectAll(false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(5U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, Selection_BidiWithNonSpacingMarks) {
  // Selection is a logical operation. And it should work with the arrow
  // keys doing visual movements, while the selection is logical between
  // the (logical) start and end points. Selection is simply defined as
  // the portion of text between the logical positions of the start and end
  // caret positions.
  TextfieldModel model(NULL);
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  model.Append(base::WideToUTF16(
      L"abc\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8" L"def"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(gfx::Range(2, 3), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(gfx::Range(2, 7), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c\x05E9\x05BC\x05C1\x05B8"),
            model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(gfx::Range(2, 3), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(gfx::Range(2, 10), model.render_text()->selection());
  EXPECT_EQ(base::WideToUTF16(L"c\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8" L"d"),
            model.GetSelectedText());

  model.ClearSelection();
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  model.SelectAll(false);
  EXPECT_EQ(
      base::WideToUTF16(L"abc\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8" L"def"),
      model.GetSelectedText());
#endif

  // In case of "aBc", this test shows how to select "aB" or "Bc", assume 'B' is
  // an RTL character.
  model.SetText(base::WideToUTF16(L"a\x05E9" L"b"));
  MoveCursorTo(model, 0);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(base::WideToUTF16(L"a"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(base::WideToUTF16(L"a"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9" L"b"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_EQ(base::WideToUTF16(L"b"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_EQ(base::WideToUTF16(L"b"), model.GetSelectedText());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9" L"b"), model.GetSelectedText());

  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9"), model.GetSelectedText());

  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(base::WideToUTF16(L"\x05E9" L"b"), model.GetSelectedText());

  model.ClearSelection();
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  model.SelectAll(false);
  EXPECT_EQ(base::WideToUTF16(L"a\x05E9" L"b"), model.GetSelectedText());
}

TEST_F(TextfieldModelTest, SelectionAndEdit) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);  // "EL"
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("HLO", model.text());

  model.Append(base::ASCIIToUTF16("ILL"));
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);  // "LO"
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("HILL", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);  // "I"
  model.InsertChar('E');
  EXPECT_STR_EQ("HELL", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);  // "H"
  model.ReplaceChar('B');
  EXPECT_STR_EQ("BELL", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);  // "ELL"
  model.ReplaceChar('E');
  EXPECT_STR_EQ("BEE", model.text());
}

TEST_F(TextfieldModelTest, Word) {
  TextfieldModel model(NULL);
  model.Append(
      base::ASCIIToUTF16("The answer to Life, the Universe, and Everything"));
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(10U, model.GetCursorPosition());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(18U, model.GetCursorPosition());

  // Should passes the non word char ','
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_EQ(23U, model.GetCursorPosition());
  EXPECT_STR_EQ(", the", model.GetSelectedText());

  // Move to the end.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_STR_EQ(", the Universe, and Everything", model.GetSelectedText());
  // Should be safe to go next word at the end.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  EXPECT_STR_EQ(", the Universe, and Everything", model.GetSelectedText());
  model.InsertChar('2');
  EXPECT_EQ(19U, model.GetCursorPosition());

  // Now backwards.
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, false);  // leave 2.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_STR_EQ("Life", model.GetSelectedText());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_STR_EQ("to Life", model.GetSelectedText());
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);  // Now at start.
  EXPECT_STR_EQ("The answer to Life", model.GetSelectedText());
  // Should be safe to go to the previous word at the beginning.
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_STR_EQ("The answer to Life", model.GetSelectedText());
  model.ReplaceChar('4');
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  EXPECT_STR_EQ("42", model.text());
}

TEST_F(TextfieldModelTest, SetText) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.SetText(base::ASCIIToUTF16("GOODBYE"));
  EXPECT_STR_EQ("GOODBYE", model.text());
  // SetText move the cursor to the end of the new text.
  EXPECT_EQ(7U, model.GetCursorPosition());
  model.SelectAll(false);
  EXPECT_STR_EQ("GOODBYE", model.GetSelectedText());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(7U, model.GetCursorPosition());

  model.SetText(base::ASCIIToUTF16("BYE"));
  // Setting shorter string moves the cursor to the end of the new string.
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_EQ(base::string16(), model.GetSelectedText());
  model.SetText(base::string16());
  EXPECT_EQ(0U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, Clipboard) {
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  const base::string16 initial_clipboard_text =
      base::ASCIIToUTF16("initial text");
  ui::ScopedClipboardWriter(clipboard, ui::CLIPBOARD_TYPE_COPY_PASTE).
      WriteText(initial_clipboard_text);

  base::string16 clipboard_text;
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));

  // Cut with an empty selection should do nothing.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_FALSE(model.Cut());
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());

  // Copy with an empty selection should do nothing.
  model.Copy();
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_EQ(11U, model.GetCursorPosition());

  // Cut on obscured (password) text should do nothing.
  model.render_text()->SetObscured(true);
  model.SelectAll(false);
  EXPECT_FALSE(model.Cut());
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_STR_EQ("HELLO WORLD", model.GetSelectedText());

  // Copy on obscured (password) text should do nothing.
  model.SelectAll(false);
  EXPECT_FALSE(model.Copy());
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard_text);
  EXPECT_EQ(initial_clipboard_text, clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.text());
  EXPECT_STR_EQ("HELLO WORLD", model.GetSelectedText());

  // Cut with non-empty selection.
  model.render_text()->SetObscured(false);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_TRUE(model.Cut());
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard_text);
  EXPECT_STR_EQ("WORLD", clipboard_text);
  EXPECT_STR_EQ("HELLO ", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Copy with non-empty selection.
  model.SelectAll(false);
  EXPECT_TRUE(model.Copy());
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard_text);
  EXPECT_STR_EQ("HELLO ", clipboard_text);
  EXPECT_STR_EQ("HELLO ", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Test that paste works regardless of the obscured bit.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO HELLO ", model.text());
  EXPECT_EQ(12U, model.GetCursorPosition());
  model.render_text()->SetObscured(true);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("HELLO HELLO HELLO ", model.text());
  EXPECT_EQ(18U, model.GetCursorPosition());
}

static void SelectWordTestVerifier(
    const TextfieldModel& model,
    const base::string16 &expected_selected_string,
    size_t expected_cursor_pos) {
  EXPECT_EQ(expected_selected_string, model.GetSelectedText());
  EXPECT_EQ(expected_cursor_pos, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, SelectWordTest) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("  HELLO  !!  WO     RLD "));

  // Test when cursor is at the beginning.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("  "), 2U);

  // Test when cursor is at the beginning of a word.
  MoveCursorTo(model, 2);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("HELLO"), 7U);

  // Test when cursor is at the end of a word.
  MoveCursorTo(model, 15);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("     "), 20U);

  // Test when cursor is somewhere in a non-alpha-numeric fragment.
  for (size_t cursor_pos = 8; cursor_pos < 13U; cursor_pos++) {
    MoveCursorTo(model, cursor_pos);
    model.SelectWord();
    SelectWordTestVerifier(model, base::ASCIIToUTF16("  !!  "), 13U);
  }

  // Test when cursor is somewhere in a whitespace fragment.
  MoveCursorTo(model, 17);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16("     "), 20U);

  // Test when cursor is at the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.SelectWord();
  SelectWordTestVerifier(model, base::ASCIIToUTF16(" "), 24U);
}

// TODO(xji): temporarily disable in platform Win since the complex script
// characters and Chinese characters are turned into empty square due to font
// regression.
#if defined(OS_LINUX)
TEST_F(TextfieldModelTest, SelectWordTest_MixScripts) {
  TextfieldModel model(NULL);
  std::vector<WordAndCursor> word_and_cursor;
  word_and_cursor.push_back(WordAndCursor(L"a\x05d0", 2));
  word_and_cursor.push_back(WordAndCursor(L"a\x05d0", 2));
  word_and_cursor.push_back(WordAndCursor(L"\x05d1\x05d2", 5));
  word_and_cursor.push_back(WordAndCursor(L"\x05d1\x05d2", 5));
  word_and_cursor.push_back(WordAndCursor(L" ", 3));
  word_and_cursor.push_back(WordAndCursor(L"a\x05d0", 2));
  word_and_cursor.push_back(WordAndCursor(L"\x0915\x094d\x0915", 9));
  word_and_cursor.push_back(WordAndCursor(L"\x0915\x094d\x0915", 9));
  word_and_cursor.push_back(WordAndCursor(L" ", 10));
  word_and_cursor.push_back(WordAndCursor(L"\x4E2D\x56FD", 12));
  word_and_cursor.push_back(WordAndCursor(L"\x4E2D\x56FD", 12));
  word_and_cursor.push_back(WordAndCursor(L"\x82B1", 13));
  word_and_cursor.push_back(WordAndCursor(L"\x5929", 14));

  // The text consists of Ascii, Hebrew, Hindi with Virama sign, and Chinese.
  model.SetText(base::WideToUTF16(L"a\x05d0 \x05d1\x05d2 \x0915\x094d\x0915 "
                                  L"\x4E2D\x56FD\x82B1\x5929"));
  for (size_t i = 0; i < word_and_cursor.size(); ++i) {
    model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
    for (size_t j = 0; j < i; ++j)
      model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
    model.SelectWord();
    SelectWordTestVerifier(model, base::WideToUTF16(word_and_cursor[i].word),
                           word_and_cursor[i].cursor);
  }
}
#endif

TEST_F(TextfieldModelTest, RangeTest) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  gfx::Range range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_FALSE(range.is_reversed());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(4U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

  // now from the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(6U, range.end());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(7U, range.end());

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  range = model.render_text()->selection();
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());

  // Select All
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, true);
  range = model.render_text()->selection();
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(0U, range.end());
}

TEST_F(TextfieldModelTest, SelectRangeTest) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  gfx::Range range(0, 6);
  EXPECT_FALSE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("HELLO ", model.GetSelectedText());

  range = gfx::Range(6, 1);
  EXPECT_TRUE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("ELLO ", model.GetSelectedText());

  range = gfx::Range(2, 1000);
  EXPECT_FALSE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("LLO WORLD", model.GetSelectedText());

  range = gfx::Range(1000, 3);
  EXPECT_TRUE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("LO WORLD", model.GetSelectedText());

  range = gfx::Range(0, 0);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = gfx::Range(3, 3);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = gfx::Range(1000, 100);
  EXPECT_FALSE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = gfx::Range(1000, 1000);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());
}

TEST_F(TextfieldModelTest, SelectionTest) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  gfx::Range selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0), selection);

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0, 5), selection);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0, 4), selection);

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(0), selection);

  // now from the end.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11), selection);

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11, 6), selection);

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11, 7), selection);

  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11), selection);

  // Select All
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, true);
  selection = model.render_text()->selection();
  EXPECT_EQ(gfx::Range(11, 0), selection);
}

TEST_F(TextfieldModelTest, SelectSelectionModelTest) {
  TextfieldModel model(NULL);
  model.Append(base::ASCIIToUTF16("HELLO WORLD"));
  model.SelectSelectionModel(gfx::SelectionModel(gfx::Range(0, 6),
      gfx::CURSOR_BACKWARD));
  EXPECT_STR_EQ("HELLO ", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(gfx::Range(6, 1),
      gfx::CURSOR_FORWARD));
  EXPECT_STR_EQ("ELLO ", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(gfx::Range(2, 1000),
      gfx::CURSOR_BACKWARD));
  EXPECT_STR_EQ("LLO WORLD", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(gfx::Range(1000, 3),
      gfx::CURSOR_FORWARD));
  EXPECT_STR_EQ("LO WORLD", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(0, gfx::CURSOR_FORWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(3, gfx::CURSOR_FORWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(gfx::Range(1000, 100),
      gfx::CURSOR_FORWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(1000, gfx::CURSOR_BACKWARD));
  EXPECT_TRUE(model.GetSelectedText().empty());
}

TEST_F(TextfieldModelTest, CompositionTextTest) {
  TextfieldModel model(this);
  model.Append(base::ASCIIToUTF16("1234590"));
  model.SelectRange(gfx::Range(5, 5));
  EXPECT_FALSE(model.HasSelection());
  EXPECT_EQ(5U, model.GetCursorPosition());

  gfx::Range range;
  model.GetTextRange(&range);
  EXPECT_EQ(gfx::Range(0, 7), range);

  ui::CompositionText composition;
  composition.text = base::ASCIIToUTF16("678");
  composition.underlines.push_back(ui::CompositionUnderline(0, 3, 0, false));

  // Cursor should be at the end of composition when characters are just typed.
  composition.selection = gfx::Range(3, 3);
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  // Cancel the composition.
  model.CancelCompositionText();
  composition_text_confirmed_or_cleared_ = false;

  // Restart composition with targeting "67" in "678".
  composition.selection = gfx::Range(0, 2);
  composition.underlines.clear();
  composition.underlines.push_back(ui::CompositionUnderline(0, 2, 0, true));
  composition.underlines.push_back(ui::CompositionUnderline(2, 3, 0, false));
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_TRUE(model.HasSelection());
  EXPECT_EQ(gfx::Range(5, 7), model.render_text()->selection());

  model.GetTextRange(&range);
  EXPECT_EQ(10U, range.end());
  EXPECT_STR_EQ("1234567890", model.text());

  model.GetCompositionTextRange(&range);
  EXPECT_EQ(gfx::Range(5, 8), range);
  // Check the composition text.
  EXPECT_STR_EQ("456", model.GetTextFromRange(gfx::Range(3, 6)));
  EXPECT_EQ(gfx::Range(5, 7), model.render_text()->selection());

  EXPECT_FALSE(composition_text_confirmed_or_cleared_);
  model.CancelCompositionText();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_EQ(5U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890", model.text());
  EXPECT_TRUE(model.SetText(base::ASCIIToUTF16("1234567890")));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);

  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890678", model.text());

  model.InsertText(base::UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-", model.text());
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_STR_EQ("-", model.GetSelectedText());
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890678", model.text());

  model.ReplaceText(base::UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-", model.text());
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  model.SetCompositionText(composition);
  model.Append(base::UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.text());

  model.SetCompositionText(composition);
  model.Delete();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.text());

  model.SetCompositionText(composition);
  model.Backspace();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.text());

  model.SetText(base::string16());
  model.SetCompositionText(composition);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("676788", model.text());
  EXPECT_EQ(6U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("676788678", model.text());

  model.SetText(base::string16());
  model.SetCompositionText(composition);
  model.MoveCursor(gfx::WORD_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, true);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678678", model.text());

  model.SetCompositionText(composition);
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  gfx::SelectionModel sel(
      gfx::Range(model.render_text()->selection().start(), 0),
      gfx::CURSOR_FORWARD);
  model.MoveCursorTo(sel);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678678", model.text());

  model.SetCompositionText(composition);
  model.SelectRange(gfx::Range(0, 3));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  model.SelectAll(false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  model.SelectWord();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.text());

  model.SetCompositionText(composition);
  model.ClearSelection();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;

  model.SetCompositionText(composition);
  EXPECT_FALSE(model.Cut());
  EXPECT_FALSE(composition_text_confirmed_or_cleared_);
}

TEST_F(TextfieldModelTest, UndoRedo_BasicTest) {
  TextfieldModel model(NULL);
  model.InsertChar('a');
  EXPECT_FALSE(model.Redo());  // There is nothing to redo.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a", model.text());

  // Continuous inserts are treated as one edit.
  model.InsertChar('b');
  model.InsertChar('c');
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Undoing further shouldn't change the text.
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Redoing to the latest text.
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Backspace ===============================
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  // Continous backspaces are treated as one edit.
  EXPECT_TRUE(model.Backspace());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("", model.text());
  // Extra backspace shouldn't affect the history.
  EXPECT_FALSE(model.Backspace());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abc", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());

  // Clear history
  model.ClearEditHistory();
  EXPECT_FALSE(model.Undo());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("a", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());

  // Delete ===============================
  model.SetText(base::ASCIIToUTF16("ABCDE"));
  model.ClearEditHistory();
  MoveCursorTo(model, 2);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("ABDE", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("BDE", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABDE", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABDE", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  // Continous deletes are treated as one edit.
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("AB", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABDE", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("AB", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, UndoRedo_SetText) {
  // This is to test the undo/redo behavior of omnibox.
  TextfieldModel model(NULL);
  model.InsertChar('w');
  EXPECT_STR_EQ("w", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SetText(base::ASCIIToUTF16("www.google.com"));
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_STR_EQ("www.google.com", model.text());
  model.SelectRange(gfx::Range(14, 1));
  model.InsertChar('w');
  EXPECT_STR_EQ("ww", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"));
  model.SelectRange(gfx::Range(14, 2));
  model.InsertChar('w');
  EXPECT_STR_EQ("www", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"));
  model.SelectRange(gfx::Range(14, 3));
  model.InsertChar('.');
  EXPECT_STR_EQ("www.", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"));
  model.SelectRange(gfx::Range(14, 4));
  model.InsertChar('y');
  EXPECT_STR_EQ("www.y", model.text());
  model.SetText(base::ASCIIToUTF16("www.youtube.com"));
  EXPECT_STR_EQ("www.youtube.com", model.text());
  EXPECT_EQ(15U, model.GetCursorPosition());

  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.youtube.com", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

TEST_F(TextfieldModelTest, UndoRedo_BackspaceThenSetText) {
  // This is to test the undo/redo behavior of omnibox.
  TextfieldModel model(NULL);
  model.InsertChar('w');
  EXPECT_STR_EQ("w", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SetText(base::ASCIIToUTF16("www.google.com"));
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_STR_EQ("www.google.com", model.text());
  model.SetText(base::ASCIIToUTF16("www.google.com"));  // Confirm the text.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_TRUE(model.Backspace());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("www.google.c", model.text());
  // Autocomplete sets the text.
  model.SetText(base::ASCIIToUTF16("www.google.com/search=www.google.c"));
  EXPECT_STR_EQ("www.google.com/search=www.google.c", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.c", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.text());
}

TEST_F(TextfieldModelTest, UndoRedo_CutCopyPasteTest) {
  TextfieldModel model(NULL);
  model.SetText(base::ASCIIToUTF16("ABCDE"));
  EXPECT_FALSE(model.Redo());  // There is nothing to redo.
  // Test Cut.
  model.SelectRange(gfx::Range(1, 3));
  model.Cut();
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());  // There is no more to undo.
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());  // There is no more to redo.
  EXPECT_STR_EQ("ADE", model.text());

  model.Paste();
  model.Paste();
  model.Paste();
  EXPECT_STR_EQ("ABCBCBCDE", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());

  // Test Redo.
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ADE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCBCBCDE", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());

  // Test using SelectRange.
  model.SelectRange(gfx::Range(1, 3));
  EXPECT_TRUE(model.Cut());
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SelectRange(gfx::Range(1, 1));
  EXPECT_FALSE(model.Cut());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("ABCBCDEBC", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCBCDE", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
  // An empty cut shouldn't create an edit.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCBCBCDE", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Test Copy.
  ResetModel(&model);
  model.SetText(base::ASCIIToUTF16("12345"));
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  model.SelectRange(gfx::Range(1, 3));
  model.Copy();  // Copy "23".
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Paste();  // Paste "23" into "23".
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Paste();
  EXPECT_STR_EQ("1232345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  // TODO(oshima): Change the return type from bool to enum.
  EXPECT_FALSE(model.Undo());  // No text change.
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_FALSE(model.Undo());
  // Test Redo.
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("12345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("12345", model.text());  // For 1st paste
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("1232345", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("1232345", model.text());

  // Test using SelectRange.
  model.SelectRange(gfx::Range(1, 3));
  model.Copy();
  EXPECT_STR_EQ("1232345", model.text());
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("123234523", model.text());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("1232345", model.text());
  EXPECT_EQ(7U, model.GetCursorPosition());
}

TEST_F(TextfieldModelTest, UndoRedo_CursorTest) {
  TextfieldModel model(NULL);
  model.InsertChar('a');
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_LEFT, false);
  model.MoveCursor(gfx::CHARACTER_BREAK, gfx::CURSOR_RIGHT, false);
  model.InsertChar('b');
  // Moving the cursor shouldn't create a new edit.
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_FALSE(model.Redo());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ab", model.text());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

void RunInsertReplaceTest(TextfieldModel& model) {
  const bool reverse = model.render_text()->selection().is_reversed();
  model.InsertChar('1');
  model.InsertChar('2');
  model.InsertChar('3');
  EXPECT_STR_EQ("a123d", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcd", model.text());
  EXPECT_EQ(reverse ? 1U : 3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abcd", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a123d", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

void RunOverwriteReplaceTest(TextfieldModel& model) {
  const bool reverse = model.render_text()->selection().is_reversed();
  model.ReplaceChar('1');
  model.ReplaceChar('2');
  model.ReplaceChar('3');
  model.ReplaceChar('4');
  EXPECT_STR_EQ("a1234", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcd", model.text());
  EXPECT_EQ(reverse ? 1U : 3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abcd", model.text());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a1234", model.text());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

TEST_F(TextfieldModelTest, UndoRedo_ReplaceTest) {
  {
    SCOPED_TRACE("Select forwards and insert.");
    TextfieldModel model(NULL);
    model.SetText(base::ASCIIToUTF16("abcd"));
    model.SelectRange(gfx::Range(1, 3));
    RunInsertReplaceTest(model);
  }
  {
    SCOPED_TRACE("Select reversed and insert.");
    TextfieldModel model(NULL);
    model.SetText(base::ASCIIToUTF16("abcd"));
    model.SelectRange(gfx::Range(3, 1));
    RunInsertReplaceTest(model);
  }
  {
    SCOPED_TRACE("Select forwards and overwrite.");
    TextfieldModel model(NULL);
    model.SetText(base::ASCIIToUTF16("abcd"));
    model.SelectRange(gfx::Range(1, 3));
    RunOverwriteReplaceTest(model);
  }
  {
    SCOPED_TRACE("Select reversed and overwrite.");
    TextfieldModel model(NULL);
    model.SetText(base::ASCIIToUTF16("abcd"));
    model.SelectRange(gfx::Range(3, 1));
    RunOverwriteReplaceTest(model);
  }
}

TEST_F(TextfieldModelTest, UndoRedo_CompositionText) {
  TextfieldModel model(NULL);

  ui::CompositionText composition;
  composition.text = base::ASCIIToUTF16("abc");
  composition.underlines.push_back(ui::CompositionUnderline(0, 3, 0, false));
  composition.selection = gfx::Range(2, 3);

  model.SetText(base::ASCIIToUTF16("ABCDE"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.InsertChar('x');
  EXPECT_STR_EQ("ABCDEx", model.text());
  EXPECT_TRUE(model.Undo());  // set composition should forget undone edit.
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("ABCDEabc", model.text());

  // Confirm the composition.
  model.ConfirmCompositionText();
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());

  // Cancel the composition.
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_LEFT, false);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("abcABCDEabc", model.text());
  model.CancelCompositionText();
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());

  // Call SetText with the same text as the result.
  ResetModel(&model);
  model.SetText(base::ASCIIToUTF16("ABCDE"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("ABCDEabc", model.text());
  model.SetText(base::ASCIIToUTF16("ABCDEabc"));
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.text());
  EXPECT_FALSE(model.Redo());

  // Call SetText with a different result; the composition should be forgotten.
  ResetModel(&model);
  model.SetText(base::ASCIIToUTF16("ABCDE"));
  model.MoveCursor(gfx::LINE_BREAK, gfx::CURSOR_RIGHT, false);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("ABCDEabc", model.text());
  model.SetText(base::ASCIIToUTF16("1234"));
  EXPECT_STR_EQ("1234", model.text());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.text());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("1234", model.text());
  EXPECT_FALSE(model.Redo());

  // TODO(oshima): Test the behavior with an IME.
}

}  // namespace views
