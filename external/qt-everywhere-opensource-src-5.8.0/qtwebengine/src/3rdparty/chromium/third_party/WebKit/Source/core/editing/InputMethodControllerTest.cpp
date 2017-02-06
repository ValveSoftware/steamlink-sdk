// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/InputMethodController.h"

#include "core/dom/Element.h"
#include "core/dom/Range.h"
#include "core/editing/FrameSelection.h"
#include "core/events/MouseEvent.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class InputMethodControllerTest : public ::testing::Test {
protected:
    InputMethodController& controller() { return frame().inputMethodController(); }
    HTMLDocument& document() const { return *m_document; }
    LocalFrame& frame() const { return m_dummyPageHolder->frame(); }
    Element* insertHTMLElement(const char* elementCode, const char* elementId);

private:
    void SetUp() override;

    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLDocument> m_document;
};

void InputMethodControllerTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    DCHECK(m_document);
}

Element* InputMethodControllerTest::insertHTMLElement(
    const char* elementCode, const char* elementId)
{
    document().write(elementCode);
    document().updateStyleAndLayout();
    Element* element = document().getElementById(elementId);
    element->focus();
    return element;
}

TEST_F(InputMethodControllerTest, BackspaceFromEndOfInput)
{
    HTMLInputElement* input = toHTMLInputElement(
        insertHTMLElement("<input id='sample'>", "sample"));

    input->setValue("fooX");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller().extendSelectionAndDelete(0, 0);
    EXPECT_STREQ("fooX", input->value().utf8().data());

    input->setValue("fooX");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xE2\x98\x85")); // U+2605 == "black star"
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86")); // U+1F3C6 == "trophy"
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89")); // composed U+0E01 "ka kai" + U+0E49 "mai tho"
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
    controller().extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue("fooX");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller().extendSelectionAndDelete(0, 1);
    EXPECT_STREQ("fooX", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionFromExistingText)
{
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

TEST_F(InputMethodControllerTest, SelectionOnConfirmExistingText)
{
    insertHTMLElement(
        "<div id='sample' contenteditable='true'>hello world</div>", "sample");

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
    controller().setCompositionFromExistingText(underlines, 0, 5);

    controller().confirmComposition();
    EXPECT_EQ(0, frame().selection().start().computeOffsetInContainerNode());
    EXPECT_EQ(0, frame().selection().end().computeOffsetInContainerNode());
}

TEST_F(InputMethodControllerTest, DeleteBySettingEmptyComposition)
{
    HTMLInputElement* input = toHTMLInputElement(
        insertHTMLElement("<input id='sample'>", "sample"));

    input->setValue("foo ");
    controller().setEditableSelectionOffsets(PlainTextRange(4, 4));
    EXPECT_STREQ("foo ", input->value().utf8().data());
    controller().extendSelectionAndDelete(0, 0);
    EXPECT_STREQ("foo ", input->value().utf8().data());

    input->setValue("foo ");
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

TEST_F(InputMethodControllerTest, SetCompositionFromExistingTextWithCollapsedWhiteSpace)
{
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

TEST_F(InputMethodControllerTest, SetCompositionFromExistingTextWithInvalidOffsets)
{
    insertHTMLElement("<div id='sample' contenteditable='true'>test</div>", "sample");

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(7, 8, Color(255, 0, 0), false, 0));
    controller().setCompositionFromExistingText(underlines, 7, 8);

    EXPECT_FALSE(controller().compositionRange());
}

TEST_F(InputMethodControllerTest, ConfirmPasswordComposition)
{
    HTMLInputElement* input = toHTMLInputElement(
        insertHTMLElement("<input id='sample' type='password' size='24'>", "sample"));

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
    controller().setComposition("foo", underlines, 0, 3);
    controller().confirmComposition();

    EXPECT_STREQ("foo", input->value().utf8().data());
}

TEST_F(InputMethodControllerTest, SetCompositionForInputWithDifferentNewCursorPositions)
{
    HTMLInputElement* input = toHTMLInputElement(
        insertHTMLElement("<input id='sample'>", "sample"));

    input->setValue("hello");
    controller().setEditableSelectionOffsets(PlainTextRange(2, 2));
    EXPECT_STREQ("hello", input->value().utf8().data());
    EXPECT_EQ(2u, controller().getSelectionOffsets().start());
    EXPECT_EQ(2u, controller().getSelectionOffsets().end());

    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, 2, Color(255, 0, 0), false, 0));

    // The cursor exceeds left boundary.
    // "*heABllo", where * stands for cursor.
    controller().setComposition("AB", underlines, -100, -100);
    EXPECT_STREQ("heABllo", input->value().utf8().data());
    EXPECT_EQ(0u, controller().getSelectionOffsets().start());
    EXPECT_EQ(0u, controller().getSelectionOffsets().end());

    // The cursor is on left boundary.
    // "*heABllo".
    controller().setComposition("AB", underlines, -2, -2);
    EXPECT_STREQ("heABllo", input->value().utf8().data());
    EXPECT_EQ(0u, controller().getSelectionOffsets().start());
    EXPECT_EQ(0u, controller().getSelectionOffsets().end());

    // The cursor is before the composing text.
    // "he*ABllo".
    controller().setComposition("AB", underlines, 0, 0);
    EXPECT_STREQ("heABllo", input->value().utf8().data());
    EXPECT_EQ(2u, controller().getSelectionOffsets().start());
    EXPECT_EQ(2u, controller().getSelectionOffsets().end());

    // The cursor is after the composing text.
    // "heAB*llo".
    controller().setComposition("AB", underlines, 2, 2);
    EXPECT_STREQ("heABllo", input->value().utf8().data());
    EXPECT_EQ(4u, controller().getSelectionOffsets().start());
    EXPECT_EQ(4u, controller().getSelectionOffsets().end());

    // The cursor is on right boundary.
    // "heABllo*".
    controller().setComposition("AB", underlines, 5, 5);
    EXPECT_STREQ("heABllo", input->value().utf8().data());
    EXPECT_EQ(7u, controller().getSelectionOffsets().start());
    EXPECT_EQ(7u, controller().getSelectionOffsets().end());

    // The cursor exceeds right boundary.
    // "heABllo*".
    controller().setComposition("AB", underlines, 100, 100);
    EXPECT_STREQ("heABllo", input->value().utf8().data());
    EXPECT_EQ(7u, controller().getSelectionOffsets().start());
    EXPECT_EQ(7u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, SetCompositionForContentEditableWithDifferentNewCursorPositions)
{
    // There are 7 nodes and 5+1+5+1+3+4+3 characters: "hello", '\n', "world", "\n", "012", "3456", "789".
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

    // The cursor exceeds left boundary.
    // "*hello\nworld\n01234AB56789", where * stands for cursor.
    controller().setComposition("AB", underlines, -100, -100);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(0u, controller().getSelectionOffsets().start());
    EXPECT_EQ(0u, controller().getSelectionOffsets().end());

    // The cursor is on left boundary.
    // "*hello\nworld\n01234AB56789".
    controller().setComposition("AB", underlines, -17, -17);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(0u, controller().getSelectionOffsets().start());
    EXPECT_EQ(0u, controller().getSelectionOffsets().end());

    // The cursor is in the 1st node.
    // "he*llo\nworld\n01234AB56789".
    controller().setComposition("AB", underlines, -15, -15);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(2u, controller().getSelectionOffsets().start());
    EXPECT_EQ(2u, controller().getSelectionOffsets().end());

    // The cursor is on right boundary of the 1st node.
    // "hello*\nworld\n01234AB56789".
    controller().setComposition("AB", underlines, -12, -12);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(5u, controller().getSelectionOffsets().start());
    EXPECT_EQ(5u, controller().getSelectionOffsets().end());

    // The cursor is on right boundary of the 2nd node.
    // "hello\n*world\n01234AB56789".
    controller().setComposition("AB", underlines, -11, -11);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(6u, controller().getSelectionOffsets().start());
    EXPECT_EQ(6u, controller().getSelectionOffsets().end());

    // The cursor is on right boundary of the 3rd node.
    // "hello\nworld*\n01234AB56789".
    controller().setComposition("AB", underlines, -6, -6);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(11u, controller().getSelectionOffsets().start());
    EXPECT_EQ(11u, controller().getSelectionOffsets().end());

    // The cursor is on right boundary of the 4th node.
    // "hello\nworld\n*01234AB56789".
    controller().setComposition("AB", underlines, -5, -5);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(12u, controller().getSelectionOffsets().start());
    EXPECT_EQ(12u, controller().getSelectionOffsets().end());

    // The cursor is before the composing text.
    // "hello\nworld\n01234*AB56789".
    controller().setComposition("AB", underlines, 0, 0);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(17u, controller().getSelectionOffsets().start());
    EXPECT_EQ(17u, controller().getSelectionOffsets().end());

    // The cursor is after the composing text.
    // "hello\nworld\n01234AB*56789".
    controller().setComposition("AB", underlines, 2, 2);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(19u, controller().getSelectionOffsets().start());
    EXPECT_EQ(19u, controller().getSelectionOffsets().end());

    // The cursor is on right boundary.
    // "hello\nworld\n01234AB56789*".
    controller().setComposition("AB", underlines, 7, 7);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(24u, controller().getSelectionOffsets().start());
    EXPECT_EQ(24u, controller().getSelectionOffsets().end());

    // The cursor exceeds right boundary.
    // "hello\nworld\n01234AB56789*".
    controller().setComposition("AB", underlines, 100, 100);
    EXPECT_STREQ("hello\nworld\n01234AB56789", div->innerText().utf8().data());
    EXPECT_EQ(24u, controller().getSelectionOffsets().start());
    EXPECT_EQ(24u, controller().getSelectionOffsets().end());
}

TEST_F(InputMethodControllerTest, CompositionFireBeforeInput)
{
    document().settings()->setScriptEnabled(true);
    Element* editable = insertHTMLElement("<div id='sample' contentEditable='true'></div>", "sample");
    Element* script = document().createElement("script", ASSERT_NO_EXCEPTION);
    script->setInnerHTML(
        "document.getElementById('sample').addEventListener('beforeinput', function(event) {"
        "    document.title = `beforeinput.isComposing:${event.isComposing};`;"
        "});"
        "document.getElementById('sample').addEventListener('input', function(event) {"
        "    document.title += `input.isComposing:${event.isComposing};`;"
        "});",
        ASSERT_NO_EXCEPTION);
    document().body()->appendChild(script, ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();

    // Simulate composition in the |contentEditable|.
    Vector<CompositionUnderline> underlines;
    underlines.append(CompositionUnderline(0, 5, Color(255, 0, 0), false, 0));
    editable->focus();

    document().setTitle(emptyString());
    controller().setComposition("foo", underlines, 0, 3);
    EXPECT_STREQ("beforeinput.isComposing:true;input.isComposing:true;", document().title().utf8().data());

    document().setTitle(emptyString());
    controller().confirmComposition();
    // Last 'beforeinput' should also be inside composition scope.
    EXPECT_STREQ("beforeinput.isComposing:true;input.isComposing:true;", document().title().utf8().data());
}

} // namespace blink
