// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/InputMethodController.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class InputMethodControllerTest : public ::testing::Test {
 protected:
  InputMethodController& controller() {
    return frame().inputMethodController();
  }
  Document& document() const { return *m_document; }
  LocalFrame& frame() const { return m_dummyPageHolder->frame(); }
  Element* insertHTMLElement(const char* elementCode, const char* elementId);

 private:
  void SetUp() override;

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<Document> m_document;
};

void InputMethodControllerTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
  m_document = &m_dummyPageHolder->document();
  DCHECK(m_document);
}

Element* InputMethodControllerTest::insertHTMLElement(const char* elementCode,
                                                      const char* elementId) {
  document().write(elementCode);
  document().updateStyleAndLayout();
  Element* element = document().getElementById(elementId);
  element->focus();
  return element;
}

TEST_F(InputMethodControllerTest, BackspaceFromEndOfInput) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("fooX");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("fooX", input->value().utf8().data());
  controller().extendSelectionAndDelete(0, 0);
  EXPECT_STREQ("fooX", input->value().utf8().data());

  input->setValue("fooX");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("fooX", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  input->setValue(
      String::fromUTF8("foo\xE2\x98\x85"));  // U+2605 == "black star"
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  input->setValue(
      String::fromUTF8("foo\xF0\x9F\x8F\x86"));  // U+1F3C6 == "trophy"
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho"
  input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  input->setValue("fooX");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("fooX", input->value().utf8().data());
  controller().extendSelectionAndDelete(0, 1);
  EXPECT_STREQ("fooX", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingText) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'>hello world</div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 5);

  Range* range = controller().compositionRange();
  EXPECT_EQ(0, range->startOffset());
  EXPECT_EQ(5, range->endOffset());

  PlainTextRange plainTextRange(PlainTextRange::create(*div, *range));
  EXPECT_EQ(0u, plainTextRange.start());
  EXPECT_EQ(5u, plainTextRange.end());
}

TEST_F(InputMethodControllerTest, SetCompositionKeepingStyle) {
  Element* div = insertHTMLElement(
      "<div id='sample' "
      "contenteditable='true'>abc1<b>2</b>34567<b>8</b>9</div>",
      "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(3, 12, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 3, 12);

  // Subtract a character.
  controller().setComposition(String("12345789"), underlines, 8, 8);
  EXPECT_STREQ("abc1<b>2</b>3457<b>8</b>9", div->innerHTML().utf8().data());

  // Append a character.
  controller().setComposition(String("123456789"), underlines, 9, 9);
  EXPECT_STREQ("abc1<b>2</b>34567<b>8</b>9", div->innerHTML().utf8().data());

  // Subtract and append characters.
  controller().setComposition(String("123hello789"), underlines, 11, 11);
  EXPECT_STREQ("abc1<b>2</b>3hello7<b>8</b>9", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionWithEmojiKeepingStyle) {
  // U+1F3E0 = 0xF0 0x9F 0x8F 0xA0 (UTF8). It's an emoji character.
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'><b>&#x1f3e0</b></div>",
      "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  controller().setCompositionFromExistingText(underlines, 0, 2);

  // 0xF0 0x9F 0x8F 0xAB is also an emoji character, with the same leading
  // surrogate pair to the previous one.
  controller().setComposition(String::fromUTF8("\xF0\x9F\x8F\xAB"), underlines,
                              2, 2);
  EXPECT_STREQ("<b>\xF0\x9F\x8F\xAB</b>", div->innerHTML().utf8().data());

  controller().setComposition(String::fromUTF8("\xF0\x9F\x8F\xA0"), underlines,
                              2, 2);
  EXPECT_STREQ("<b>\xF0\x9F\x8F\xA0</b>", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest,
       SetCompositionWithTeluguSignVisargaKeepingStyle) {
  // U+0C03 = 0xE0 0xB0 0x83 (UTF8), a telugu sign visarga with one code point.
  // It's one grapheme cluster if separated. It can also form one grapheme
  // cluster with another code point(e.g, itself).
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'><b>&#xc03</b></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 1);

  // 0xE0 0xB0 0x83 0xE0 0xB0 0x83, a telugu character with 2 code points in
  // 1 grapheme cluster.
  controller().setComposition(String::fromUTF8("\xE0\xB0\x83\xE0\xB0\x83"),
                              underlines, 2, 2);
  EXPECT_STREQ("<b>\xE0\xB0\x83\xE0\xB0\x83</b>",
               div->innerHTML().utf8().data());

  controller().setComposition(String::fromUTF8("\xE0\xB0\x83"), underlines, 1,
                              1);
  EXPECT_STREQ("<b>\xE0\xB0\x83</b>", div->innerHTML().utf8().data());
}

TEST_F(InputMethodControllerTest, SelectionOnConfirmExistingText) {
  insertHTMLElement("<div id='sample' contenteditable='true'>hello world</div>",
                    "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 5);

  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_EQ(0, frame().selection().start().computeOffsetInContainerNode());
  EXPECT_EQ(0, frame().selection().end().computeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest, DeleteBySettingEmptyComposition) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("foo ");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo ", input->value().utf8().data());
  controller().extendSelectionAndDelete(0, 0);
  EXPECT_STREQ("foo ", input->value().utf8().data());

  input->setValue("foo ");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo ", input->value().utf8().data());
  controller().extendSelectionAndDelete(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 3, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 3);

  controller().setComposition(String(""), underlines, 0, 3);

  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       SetCompositionFromExistingTextWithCollapsedWhiteSpace) {
  // Creates a div with one leading new line char. The new line char is hidden
  // from the user and IME, but is visible to InputMethodController.
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'>\nhello world</div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 0, 5);

  Range* range = controller().compositionRange();
  EXPECT_EQ(1, range->startOffset());
  EXPECT_EQ(6, range->endOffset());

  PlainTextRange plainTextRange(PlainTextRange::create(*div, *range));
  EXPECT_EQ(0u, plainTextRange.start());
  EXPECT_EQ(5u, plainTextRange.end());
}

TEST_F(InputMethodControllerTest,
       SetCompositionFromExistingTextWithInvalidOffsets) {
  insertHTMLElement("<div id='sample' contenteditable='true'>test</div>",
                    "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(7, 8, Color(255, 0, 0), false, 0));
  controller().setCompositionFromExistingText(underlines, 7, 8);

  EXPECT_FALSE(controller().compositionRange());
}

TEST_F(InputMethodControllerTest, ConfirmPasswordComposition) {
  HTMLInputElement* input = toHTMLInputElement(insertHTMLElement(
      "<input id='sample' type='password' size='24'>", "sample"));

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setComposition("foo", underlines, 0, 3);
  controller().finishComposingText(InputMethodController::KeepSelection);

  EXPECT_STREQ("foo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithEmptyText) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(0, 0);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("");
  document().updateStyleAndLayout();
  EXPECT_STREQ("", input->value().utf8().data());
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithRangeSelection) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(0, 0);
  EXPECT_STREQ("hello", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("ell", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(100, 0);
  EXPECT_STREQ("ello", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(0, 100);
  EXPECT_STREQ("hell", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 4));
  controller().deleteSurroundingText(100, 100);
  EXPECT_STREQ("ell", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextWithCursorSelection) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("hllo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ("helo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(0, 0);
  EXPECT_STREQ("hello", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("hlo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(100, 0);
  EXPECT_STREQ("llo", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(0, 100);
  EXPECT_STREQ("he", input->value().utf8().data());

  input->setValue("hello");
  document().updateStyleAndLayout();
  EXPECT_STREQ("hello", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  controller().deleteSurroundingText(100, 100);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("h");
  document().updateStyleAndLayout();
  EXPECT_STREQ("h", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(1, 1));
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("", input->value().utf8().data());

  input->setValue("h");
  document().updateStyleAndLayout();
  EXPECT_STREQ("h", input->value().utf8().data());
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnTheLeft) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // U+2605 == "black star". It takes up 1 space.
  input->setValue(String::fromUTF8("foo\xE2\x98\x85"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
  EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // U+1F3C6 == "trophy". It takes up 2 space.
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(5, 5));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho". It takes up 2 space.
  input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(5, 5));
  EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(2, 0);
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(3, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(4, 0);
  EXPECT_STREQ("foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(7, 7));
  EXPECT_STREQ("foo\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(5, 0);
  EXPECT_STREQ("fo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnTheRight) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // U+2605 == "black star". It takes up 1 space.
  input->setValue(String::fromUTF8("\xE2\x98\x85 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xE2\x98\x85 foo", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // U+1F3C6 == "trophy". It takes up 2 space.
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86 foo", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // composed U+0E01 "ka kai" + U+0E49 "mai tho". It takes up 2 space.
  input->setValue(String::fromUTF8("\xE0\xB8\x81\xE0\xB9\x89 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xE0\xB8\x81\xE0\xB9\x89 foo", input->value().utf8().data());
  controller().deleteSurroundingText(0, 1);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 2);
  EXPECT_STREQ("\xF0\x9F\x8F\x86 foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 3);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 4);
  EXPECT_STREQ(" foo", input->value().utf8().data());

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(0, 0));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86 foo",
               input->value().utf8().data());
  controller().deleteSurroundingText(0, 5);
  EXPECT_STREQ("foo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest,
       DeleteSurroundingTextWithMultiCodeTextOnBothSides) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  // "trophy" + "trophy".
  input->setValue(String::fromUTF8("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86"));
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_STREQ("\xF0\x9F\x8F\x86\xF0\x9F\x8F\x86",
               input->value().utf8().data());
  controller().deleteSurroundingText(1, 1);
  EXPECT_STREQ("", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, DeleteSurroundingTextForMultipleNodes) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'>aaa"
      "<div id='sample2' contenteditable='true'>bbb"
      "<div id='sample3' contenteditable='true'>ccc"
      "<div id='sample4' contenteditable='true'>ddd"
      "<div id='sample5' contenteditable='true'>eee"
      "</div></div></div></div></div>",
      "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(8, 8));
  EXPECT_STREQ("aaa\nbbb\nccc\nddd\neee", div->innerText().utf8().data());
  EXPECT_EQ(8u, controller().getSelectionOffsets().start());
  EXPECT_EQ(8u, controller().getSelectionOffsets().end());

  controller().deleteSurroundingText(1, 0);
  EXPECT_STREQ("aaa\nbbbccc\nddd\neee", div->innerText().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());

  controller().deleteSurroundingText(0, 4);
  EXPECT_STREQ("aaa\nbbbddd\neee", div->innerText().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());

  controller().deleteSurroundingText(5, 5);
  EXPECT_STREQ("aaee", div->innerText().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, SetCompositionForInputWithNewCaretPositions) {
  HTMLInputElement* input =
      toHTMLInputElement(insertHTMLElement("<input id='sample'>", "sample"));

  input->setValue("hello");
  document().updateStyleAndLayout();
  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_STREQ("hello", input->value().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  // The caret exceeds left boundary.
  // "*heABllo", where * stands for caret.
  controller().setComposition("AB", underlines, -100, -100);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is on left boundary.
  // "*heABllo".
  controller().setComposition("AB", underlines, -2, -2);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is before the composing text.
  // "he*ABllo".
  controller().setComposition("AB", underlines, 0, 0);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  // The caret is after the composing text.
  // "heAB*llo".
  controller().setComposition("AB", underlines, 2, 2);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(4u, controller().getSelectionOffsets().start());
  EXPECT_EQ(4u, controller().getSelectionOffsets().end());

  // The caret is on right boundary.
  // "heABllo*".
  controller().setComposition("AB", underlines, 5, 5);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());

  // The caret exceeds right boundary.
  // "heABllo*".
  controller().setComposition("AB", underlines, 100, 100);
  EXPECT_STREQ("heABllo", input->value().utf8().data());
  EXPECT_EQ(7u, controller().getSelectionOffsets().start());
  EXPECT_EQ(7u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest,
       SetCompositionForContentEditableWithNewCaretPositions) {
  // There are 7 nodes and 5+1+5+1+3+4+3 characters: "hello", '\n', "world",
  // "\n", "012", "3456", "789".
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'>"
      "hello"
      "<div id='sample2' contenteditable='true'>world"
      "<p>012<b>3456</b><i>789</i></p>"
      "</div>"
      "</div>",
      "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(17, 17));
  EXPECT_STREQ("hello\nworld\n0123456789", div->innerText().utf8().data());
  EXPECT_EQ(17u, controller().getSelectionOffsets().start());
  EXPECT_EQ(17u, controller().getSelectionOffsets().end());

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  // The caret exceeds left boundary.
  // "*hello\nworld\n01234AB56789", where * stands for caret.
  controller().setComposition("AB", underlines, -100, -100);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is on left boundary.
  // "*hello\nworld\n01234AB56789".
  controller().setComposition("AB", underlines, -17, -17);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(0u, controller().getSelectionOffsets().start());
  EXPECT_EQ(0u, controller().getSelectionOffsets().end());

  // The caret is in the 1st node.
  // "he*llo\nworld\n01234AB56789".
  controller().setComposition("AB", underlines, -15, -15);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 1st node.
  // "hello*\nworld\n01234AB56789".
  controller().setComposition("AB", underlines, -12, -12);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(5u, controller().getSelectionOffsets().start());
  EXPECT_EQ(5u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 2nd node.
  // "hello\n*world\n01234AB56789".
  controller().setComposition("AB", underlines, -11, -11);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(6u, controller().getSelectionOffsets().start());
  EXPECT_EQ(6u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 3rd node.
  // "hello\nworld*\n01234AB56789".
  controller().setComposition("AB", underlines, -6, -6);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(11u, controller().getSelectionOffsets().start());
  EXPECT_EQ(11u, controller().getSelectionOffsets().end());

  // The caret is on right boundary of the 4th node.
  // "hello\nworld\n*01234AB56789".
  controller().setComposition("AB", underlines, -5, -5);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(12u, controller().getSelectionOffsets().start());
  EXPECT_EQ(12u, controller().getSelectionOffsets().end());

  // The caret is before the composing text.
  // "hello\nworld\n01234*AB56789".
  controller().setComposition("AB", underlines, 0, 0);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(17u, controller().getSelectionOffsets().start());
  EXPECT_EQ(17u, controller().getSelectionOffsets().end());

  // The caret is after the composing text.
  // "hello\nworld\n01234AB*56789".
  controller().setComposition("AB", underlines, 2, 2);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(19u, controller().getSelectionOffsets().start());
  EXPECT_EQ(19u, controller().getSelectionOffsets().end());

  // The caret is on right boundary.
  // "hello\nworld\n01234AB56789*".
  controller().setComposition("AB", underlines, 7, 7);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(24u, controller().getSelectionOffsets().start());
  EXPECT_EQ(24u, controller().getSelectionOffsets().end());

  // The caret exceeds right boundary.
  // "hello\nworld\n01234AB56789*".
  controller().setComposition("AB", underlines, 100, 100);
  EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
  EXPECT_EQ(24u, controller().getSelectionOffsets().start());
  EXPECT_EQ(24u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, SetCompositionWithEmptyText) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'>hello</div>", "sample");

  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  Vector<CompositionUnderline> underlines0;
  underlines0.append(CompositionUnderline(0, 0, Color(255, 0, 0), false, 0));
  Vector<CompositionUnderline> underlines2;
  underlines2.append(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

  controller().setComposition("AB", underlines2, 2, 2);
  // With previous composition.
  controller().setComposition("", underlines0, 2, 2);
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(4u, controller().getSelectionOffsets().start());
  EXPECT_EQ(4u, controller().getSelectionOffsets().end());

  // Without previous composition.
  controller().setComposition("", underlines0, -1, -1);
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(3u, controller().getSelectionOffsets().start());
  EXPECT_EQ(3u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, InsertLineBreakWhileComposingText) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'></div>", "sample");

  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  controller().setComposition("hello", underlines, 5, 5);
  EXPECT_STREQ("hello", div->innerText().utf8().data());
  EXPECT_EQ(5u, controller().getSelectionOffsets().start());
  EXPECT_EQ(5u, controller().getSelectionOffsets().end());

  frame().editor().insertLineBreak();
  EXPECT_STREQ("\n\n", div->innerText().utf8().data());
  EXPECT_EQ(1u, controller().getSelectionOffsets().start());
  EXPECT_EQ(1u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, InsertLineBreakAfterConfirmingText) {
  Element* div = insertHTMLElement(
      "<div id='sample' contenteditable='true'></div>", "sample");

  controller().commitText("hello", 0);
  EXPECT_STREQ("hello", div->innerText().utf8().data());

  controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
  EXPECT_EQ(2u, controller().getSelectionOffsets().start());
  EXPECT_EQ(2u, controller().getSelectionOffsets().end());

  frame().editor().insertLineBreak();
  EXPECT_STREQ("he\nllo", div->innerText().utf8().data());
  EXPECT_EQ(3u, controller().getSelectionOffsets().start());
  EXPECT_EQ(3u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, CompositionInputEventIsComposing) {
  document().settings()->setScriptEnabled(true);
  Element* editable = insertHTMLElement(
      "<div id='sample' contentEditable='true'></div>", "sample");
  Element* script = document().createElement("script");
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "function(event) {"
      "    document.title = `beforeinput.isComposing:${event.isComposing};`;"
      "});"
      "document.getElementById('sample').addEventListener('input', "
      "function(event) {"
      "    document.title += `input.isComposing:${event.isComposing};`;"
      "});");
  document().body()->appendChild(script);
  document().view()->updateAllLifecyclePhases();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  editable->focus();

  document().setTitle(emptyString());
  controller().setComposition("foo", underlines, 0, 3);
  EXPECT_STREQ("beforeinput.isComposing:true;input.isComposing:true;",
               document().title().utf8().data());

  document().setTitle(emptyString());
  controller().finishComposingText(InputMethodController::KeepSelection);
  // Last pair of InputEvent should also be inside composition scope.
  EXPECT_STREQ("beforeinput.isComposing:true;input.isComposing:true;",
               document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, CompositionInputEventData) {
  document().settings()->setScriptEnabled(true);
  Element* editable = insertHTMLElement(
      "<div id='sample' contentEditable='true'></div>", "sample");
  Element* script = document().createElement("script");
  script->setInnerHTML(
      "document.getElementById('sample').addEventListener('beforeinput', "
      "function(event) {"
      "    document.title = `beforeinput.data:${event.data};`;"
      "});"
      "document.getElementById('sample').addEventListener('input', "
      "function(event) {"
      "    document.title += `input.data:${event.data};`;"
      "});");
  document().body()->appendChild(script);
  document().view()->updateAllLifecyclePhases();

  // Simulate composition in the |contentEditable|.
  Vector<CompositionUnderline> underlines;
  underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
  editable->focus();

  document().setTitle(emptyString());
  controller().setComposition("n", underlines, 0, 1);
  EXPECT_STREQ("beforeinput.data:n;input.data:n;",
               document().title().utf8().data());

  document().setTitle(emptyString());
  controller().setComposition("ni", underlines, 0, 1);
  EXPECT_STREQ("beforeinput.data:i;input.data:i;",
               document().title().utf8().data());

  document().setTitle(emptyString());
  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_STREQ("beforeinput.data:ni;input.data:ni;",
               document().title().utf8().data());
}

TEST_F(InputMethodControllerTest, FinishCompositionRemovedRange) {
  Element* inputA =
      insertHTMLElement("<input id='a' /><br><input type='tel' id='b' />", "a");

  EXPECT_EQ(WebTextInputTypeText, controller().textInputType());

  // The test requires non-empty composition.
  controller().setComposition("hello", Vector<CompositionUnderline>(), 5, 5);
  EXPECT_EQ(WebTextInputTypeText, controller().textInputType());

  // Remove element 'a'.
  inputA->setOuterHTML("", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(WebTextInputTypeNone, controller().textInputType());

  document().getElementById("b")->focus();
  EXPECT_EQ(WebTextInputTypeTelephone, controller().textInputType());

  controller().finishComposingText(InputMethodController::KeepSelection);
  EXPECT_EQ(WebTextInputTypeTelephone, controller().textInputType());
}

}  // namespace blink
