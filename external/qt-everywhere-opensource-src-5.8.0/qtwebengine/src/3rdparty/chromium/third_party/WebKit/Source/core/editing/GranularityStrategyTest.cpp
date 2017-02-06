// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLSpanElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include <memory>

namespace blink {

#define EXPECT_EQ_SELECTED_TEXT(text) \
    EXPECT_EQ(text, WebString(selection().selectedText()).utf8())

IntPoint visiblePositionToContentsPoint(const VisiblePosition& pos)
{
    IntPoint result = absoluteCaretBoundsOf(pos).minXMaxYCorner();
    // Need to move the point at least by 1 - caret's minXMaxYCorner is not
    // evaluated to the same line as the text by hit testing.
    result.move(0, -1);
    return result;
}

using TextNodeVector = HeapVector<Member<Text>>;

class GranularityStrategyTest : public ::testing::Test {
protected:
    void SetUp() override;

    DummyPageHolder& dummyPageHolder() const { return *m_dummyPageHolder; }
    HTMLDocument& document() const;
    void setSelection(const VisibleSelection&);
    FrameSelection& selection() const;
    Text* appendTextNode(const String& data);
    int layoutCount() const { return m_dummyPageHolder->frameView().layoutCount(); }
    void setInnerHTML(const char*);
    // Parses the text node, appending the info to m_letterPos and m_wordMiddles.
    void parseText(Text*);
    void parseText(const TextNodeVector&);

    Text* setupTranslateZ(String);
    Text* setupTransform(String);
    Text* setupRotate(String);
    void setupTextSpan(String str1, String str2, String str3, size_t selBegin, size_t selEnd);
    void setupVerticalAlign(String str1, String str2, String str3, size_t selBegin, size_t selEnd);
    void setupFontSize(String str1, String str2, String str3, size_t selBegin, size_t selEnd);

    void testDirectionExpand();
    void testDirectionShrink();
    void testDirectionSwitchSide();

    // Pixel coordinates of the positions for each letter within the text being tested.
    Vector<IntPoint> m_letterPos;
    // Pixel coordinates of the middles of the words in the text being tested.
    // (y coordinate is based on y coordinates of m_letterPos)
    Vector<IntPoint> m_wordMiddles;

private:
    std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
    Persistent<HTMLDocument> m_document;
};

void GranularityStrategyTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    DCHECK(m_document);
    dummyPageHolder().frame().settings()->setDefaultFontSize(12);
    dummyPageHolder().frame().settings()->setSelectionStrategy(SelectionStrategy::Direction);
}

HTMLDocument& GranularityStrategyTest::document() const
{
    return *m_document;
}

void GranularityStrategyTest::setSelection(const VisibleSelection& newSelection)
{
    m_dummyPageHolder->frame().selection().setSelection(newSelection);
}

FrameSelection& GranularityStrategyTest::selection() const
{
    return m_dummyPageHolder->frame().selection();
}

Text* GranularityStrategyTest::appendTextNode(const String& data)
{
    Text* text = document().createTextNode(data);
    document().body()->appendChild(text);
    return text;
}

void GranularityStrategyTest::setInnerHTML(const char* htmlContent)
{
    document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent), ASSERT_NO_EXCEPTION);
    document().view()->updateAllLifecyclePhases();
}

void GranularityStrategyTest::parseText(Text* text)
{
    TextNodeVector textNodes;
    textNodes.append(text);
    parseText(textNodes);
}

void GranularityStrategyTest::parseText(const TextNodeVector& textNodes)
{
    bool wordStarted = false;
    int wordStartIndex = 0;
    for (auto& text : textNodes) {
        int wordStartIndexOffset = m_letterPos.size();
        String str = text->wholeText();
        for (size_t i = 0; i < str.length(); i++) {
            m_letterPos.append(visiblePositionToContentsPoint(createVisiblePosition(Position(text, i))));
            char c = str[i];
            if (isASCIIAlphanumeric(c) && !wordStarted) {
                wordStartIndex = i + wordStartIndexOffset;
                wordStarted = true;
            } else if (!isASCIIAlphanumeric(c) && wordStarted) {
                IntPoint wordMiddle((m_letterPos[wordStartIndex].x() + m_letterPos[i + wordStartIndexOffset].x()) / 2, m_letterPos[wordStartIndex].y());
                m_wordMiddles.append(wordMiddle);
                wordStarted = false;
            }
        }
    }
    if (wordStarted) {
        const auto& lastNode = textNodes.last();
        int xEnd = visiblePositionToContentsPoint(createVisiblePosition(Position(lastNode, lastNode->wholeText().length()))).x();
        IntPoint wordMiddle((m_letterPos[wordStartIndex].x() + xEnd) / 2, m_letterPos[wordStartIndex].y());
        m_wordMiddles.append(wordMiddle);
    }
}

Text* GranularityStrategyTest::setupTranslateZ(String str)
{
    setInnerHTML(
        "<html>"
        "<head>"
        "<style>"
            "div {"
                "transform: translateZ(0);"
            "}"
        "</style>"
        "</head>"
        "<body>"
        "<div id='mytext'></div>"
        "</body>"
        "</html>");

    Text* text = document().createTextNode(str);
    Element* div = document().getElementById("mytext");
    div->appendChild(text);

    document().view()->updateAllLifecyclePhases();

    parseText(text);
    return text;
}

Text* GranularityStrategyTest::setupTransform(String str)
{
    setInnerHTML(
        "<html>"
        "<head>"
        "<style>"
            "div {"
                "transform: scale(1,-1) translate(0,-100px);"
            "}"
        "</style>"
        "</head>"
        "<body>"
        "<div id='mytext'></div>"
        "</body>"
        "</html>");

    Text* text = document().createTextNode(str);
    Element* div = document().getElementById("mytext");
    div->appendChild(text);

    document().view()->updateAllLifecyclePhases();

    parseText(text);
    return text;
}

Text* GranularityStrategyTest::setupRotate(String str)
{
    setInnerHTML(
        "<html>"
        "<head>"
        "<style>"
            "div {"
                "transform: translate(0px,600px) rotate(90deg);"
            "}"
        "</style>"
        "</head>"
        "<body>"
        "<div id='mytext'></div>"
        "</body>"
        "</html>");

    Text* text = document().createTextNode(str);
    Element* div = document().getElementById("mytext");
    div->appendChild(text);

    document().view()->updateAllLifecyclePhases();

    parseText(text);
    return text;
}

void GranularityStrategyTest::setupTextSpan(String str1, String str2, String str3, size_t selBegin, size_t selEnd)
{
    Text* text1 = document().createTextNode(str1);
    Text* text2 = document().createTextNode(str2);
    Text* text3 = document().createTextNode(str3);
    Element* span = HTMLSpanElement::create(document());
    Element* div = document().getElementById("mytext");
    div->appendChild(text1);
    div->appendChild(span);
    span->appendChild(text2);
    div->appendChild(text3);

    document().view()->updateAllLifecyclePhases();

    Vector<IntPoint> letterPos;
    Vector<IntPoint> wordMiddlePos;

    TextNodeVector textNodes;
    textNodes.append(text1);
    textNodes.append(text2);
    textNodes.append(text3);
    parseText(textNodes);

    Position p1;
    Position p2;
    if (selBegin < str1.length())
        p1 = Position(text1, selBegin);
    else if (selBegin < str1.length() + str2.length())
        p1 = Position(text2, selBegin - str1.length());
    else
        p1 = Position(text3, selBegin - str1.length() - str2.length());
    if (selEnd < str1.length())
        p2 = Position(text1, selEnd);
    else if (selEnd < str1.length() + str2.length())
        p2 = Position(text2, selEnd - str1.length());
    else
        p2 = Position(text3, selEnd - str1.length() - str2.length());

    selection().setSelection(VisibleSelection(p1, p2));
}

void GranularityStrategyTest::setupVerticalAlign(String str1, String str2, String str3, size_t selBegin, size_t selEnd)
{
    setInnerHTML(
        "<html>"
        "<head>"
        "<style>"
            "span {"
                "vertical-align:20px;"
            "}"
        "</style>"
        "</head>"
        "<body>"
        "<div id='mytext'></div>"
        "</body>"
        "</html>");

    setupTextSpan(str1, str2, str3, selBegin, selEnd);
}

void GranularityStrategyTest::setupFontSize(String str1, String str2, String str3, size_t selBegin, size_t selEnd)
{
    setInnerHTML(
        "<html>"
        "<head>"
        "<style>"
            "span {"
                "font-size: 200%;"
            "}"
        "</style>"
        "</head>"
        "<body>"
        "<div id='mytext'></div>"
        "</body>"
        "</html>");

    setupTextSpan(str1, str2, str3, selBegin, selEnd);
}


// Tests expanding selection on text "abcdef ghij kl mno^p|>qr stuvwi inm  mnii,"
// (^ means base, | means extent, < means start, and > means end).
// Text needs to be laid out on a single line with no rotation.
void GranularityStrategyTest::testDirectionExpand()
{
    // Expand selection using character granularity until the end of the word
    // is reached.
    // "abcdef ghij kl mno^pq|>r stuvwi inm  mnii,"
    selection().moveRangeSelectionExtent(m_letterPos[20]);
    EXPECT_EQ_SELECTED_TEXT("pq");
    // Move to the same postion shouldn't change anything.
    selection().moveRangeSelectionExtent(m_letterPos[20]);
    EXPECT_EQ_SELECTED_TEXT("pq");
    // "abcdef ghij kl mno^pqr|> stuvwi inm  mnii,"
    selection().moveRangeSelectionExtent(m_letterPos[21]);
    EXPECT_EQ_SELECTED_TEXT("pqr");
    // Selection should stay the same until the middle of the word is passed.
    // "abcdef ghij kl mno^pqr |>stuvwi inm  mnii," -
    selection().moveRangeSelectionExtent(m_letterPos[22]);
    EXPECT_EQ_SELECTED_TEXT("pqr ");
    // "abcdef ghij kl mno^pqr >st|uvwi inm  mnii,"
    selection().moveRangeSelectionExtent(m_letterPos[24]);
    EXPECT_EQ_SELECTED_TEXT("pqr ");
    IntPoint p = m_wordMiddles[4];
    p.move(-1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr ");
    p.move(1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi");
    // Selection should stay the same until the end of the word is reached.
    // "abcdef ghij kl mno^pqr stuvw|i> inm  mnii,"
    selection().moveRangeSelectionExtent(m_letterPos[27]);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi");
    // "abcdef ghij kl mno^pqr stuvwi|> inm  mnii,"
    selection().moveRangeSelectionExtent(m_letterPos[28]);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi");
    // "abcdef ghij kl mno^pqr stuvwi |>inm  mnii,"
    selection().moveRangeSelectionExtent(m_letterPos[29]);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi ");
    // Now expand slowly to the middle of word #5.
    int y = m_letterPos[29].y();
    for (int x = m_letterPos[29].x() + 1; x < m_wordMiddles[5].x(); x++) {
        selection().moveRangeSelectionExtent(IntPoint(x, y));
        selection().moveRangeSelectionExtent(IntPoint(x, y));
        EXPECT_EQ_SELECTED_TEXT("pqr stuvwi ");
    }
    selection().moveRangeSelectionExtent(m_wordMiddles[5]);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi inm");
    // Jump over quickly to just before the middle of the word #6 and then
    // move over it.
    p = m_wordMiddles[6];
    p.move(-1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi inm ");
    p.move(1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr stuvwi inm mnii");
}

// Tests shrinking selection on text "abcdef ghij kl mno^pqr|> iiinmni, abc"
// (^ means base, | means extent, < means start, and > means end).
// Text needs to be laid out on a single line with no rotation.
void GranularityStrategyTest::testDirectionShrink()
{
    // Move to the middle of word #4 to it and then move back, confirming
    // that the selection end is moving with the extent. The offset between the
    // extent and the selection end will be equal to half the width of "iiinmni".
    selection().moveRangeSelectionExtent(m_wordMiddles[4]);
    EXPECT_EQ_SELECTED_TEXT("pqr iiinmni");
    IntPoint p = m_wordMiddles[4];
    p.move(m_letterPos[28].x() - m_letterPos[29].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr iiinmn");
    p.move(m_letterPos[27].x() - m_letterPos[28].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr iiinm");
    p.move(m_letterPos[26].x() - m_letterPos[27].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr iiin");
    // Move right by the width of char 30 ('m'). Selection shouldn't change,
    // but offset should be reduced.
    p.move(m_letterPos[27].x() - m_letterPos[26].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr iiin");
    // Move back a couple of character widths and confirm the selection still
    // updates accordingly.
    p.move(m_letterPos[25].x() - m_letterPos[26].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr iii");
    p.move(m_letterPos[24].x() - m_letterPos[25].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr ii");
    // "Catch up" with the handle - move the extent to where the handle is.
    // "abcdef ghij kl mno^pqr ii|>inmni, abc"
    selection().moveRangeSelectionExtent(m_letterPos[24]);
    EXPECT_EQ_SELECTED_TEXT("pqr ii");
    // Move ahead and confirm the selection expands accordingly
    // "abcdef ghij kl mno^pqr iii|>nmni, abc"
    selection().moveRangeSelectionExtent(m_letterPos[25]);
    EXPECT_EQ_SELECTED_TEXT("pqr iii");

    // Confirm we stay in character granularity if the user moves within a word.
    // "abcdef ghij kl mno^pqr |>iiinmni, abc"
    selection().moveRangeSelectionExtent(m_letterPos[22]);
    EXPECT_EQ_SELECTED_TEXT("pqr ");
    // It's possible to get a move when position doesn't change.
    // It shouldn't affect anything.
    p = m_letterPos[22];
    p.move(1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("pqr ");
    // "abcdef ghij kl mno^pqr i|>iinmni, abc"
    selection().moveRangeSelectionExtent(m_letterPos[23]);
    EXPECT_EQ_SELECTED_TEXT("pqr i");
}

// Tests moving selection extent over to the other side of the base
// on text "abcd efgh ijkl mno^pqr|> iiinmni, abc"
// (^ means base, | means extent, < means start, and > means end).
// Text needs to be laid out on a single line with no rotation.
void GranularityStrategyTest::testDirectionSwitchSide()
{
    // Move to the middle of word #4, selecting it - this will set the offset to
    // be half the width of "iiinmni.
    selection().moveRangeSelectionExtent(m_wordMiddles[4]);
    EXPECT_EQ_SELECTED_TEXT("pqr iiinmni");
    // Move back leaving only one letter selected.
    IntPoint p = m_wordMiddles[4];
    p.move(m_letterPos[19].x() - m_letterPos[29].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("p");
    // Confirm selection doesn't change if extent is positioned at base.
    p.move(m_letterPos[18].x() - m_letterPos[19].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("p");
    // Move over to the other side of the base. Confirm the offset is preserved.
    // (i.e. the selection start stays on the right of the extent)
    // Confirm we stay in character granularity until the beginning of the word
    // is passed.
    p.move(m_letterPos[17].x() - m_letterPos[18].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("o");
    p.move(m_letterPos[16].x() - m_letterPos[17].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("no");
    p.move(m_letterPos[14].x() - m_letterPos[16].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT(" mno");
    // Move to just one pixel on the right before the middle of the word #2.
    // We should switch to word granularity, so the selection shouldn't change.
    p.move(m_wordMiddles[2].x() - m_letterPos[14].x() + 1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT(" mno");
    // Move over the middle of the word. The word should get selected.
    // This should reduce the offset, but it should still stay greated than 0,
    // since the width of "iiinmni" is greater than the width of "ijkl".
    p.move(-2, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("ijkl mno");
    // Move to just one pixel on the right of the middle of word #1.
    // The selection should now include the space between the words.
    p.move(m_wordMiddles[1].x() - m_letterPos[10].x() + 1, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT(" ijkl mno");
    // Move over the middle of the word. The word should get selected.
    p.move(-2, 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("efgh ijkl mno");
}

// Test for the default CharacterGranularityStrategy
TEST_F(GranularityStrategyTest, Character)
{
    dummyPageHolder().frame().settings()->setSelectionStrategy(SelectionStrategy::Character);
    dummyPageHolder().frame().settings()->setDefaultFontSize(12);
    // "Foo Bar Baz,"
    Text* text = appendTextNode("Foo Bar Baz,");
    // "Foo B^a|>r Baz," (^ means base, | means extent, , < means start, and > means end).
    selection().setSelection(VisibleSelection(Position(text, 5), Position(text, 6)));
    EXPECT_EQ_SELECTED_TEXT("a");
    // "Foo B^ar B|>az,"
    selection().moveRangeSelectionExtent(visiblePositionToContentsPoint(createVisiblePosition(Position(text, 9))));
    EXPECT_EQ_SELECTED_TEXT("ar B");
    // "F<|oo B^ar Baz,"
    selection().moveRangeSelectionExtent(visiblePositionToContentsPoint(createVisiblePosition(Position(text, 1))));
    EXPECT_EQ_SELECTED_TEXT("oo B");
}

// DirectionGranularityStrategy strategy on rotated text should revert to the
// same behavior as CharacterGranularityStrategy
TEST_F(GranularityStrategyTest, DirectionRotate)
{
    Text* text = setupRotate("Foo Bar Baz,");
    // "Foo B^a|>r Baz," (^ means base, | means extent, , < means start, and > means end).
    selection().setSelection(VisibleSelection(Position(text, 5), Position(text, 6)));
    EXPECT_EQ_SELECTED_TEXT("a");
    IntPoint p = m_letterPos[9];
    // Need to move by one pixel, otherwise this point is not evaluated
    // to the same line as the text by hit testing.
    p.move(1, 0);
    // "Foo B^ar B|>az,"
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("ar B");
    p = m_letterPos[1];
    p.move(1, 0);
    // "F<|oo B^ar Baz,"
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("oo B");
}

TEST_F(GranularityStrategyTest, DirectionExpandTranslateZ)
{
    Text* text = setupTranslateZ("abcdef ghij kl mnopqr stuvwi inm mnii,");
    // "abcdef ghij kl mno^p|>qr stuvwi inm  mnii," (^ means base, | means extent, < means start, and > means end).
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 19)));
    EXPECT_EQ_SELECTED_TEXT("p");
    testDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionExpandTransform)
{
    Text* text = setupTransform("abcdef ghij kl mnopqr stuvwi inm mnii,");
    // "abcdef ghij kl mno^p|>qr stuvwi inm  mnii," (^ means base, | means extent, < means start, and > means end).
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 19)));
    EXPECT_EQ_SELECTED_TEXT("p");
    testDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionExpandVerticalAlign)
{
    // "abcdef ghij kl mno^p|>qr stuvwi inm  mnii," (^ means base, | means extent, < means start, and > means end).
    setupVerticalAlign("abcdef ghij kl m", "nopq", "r stuvwi inm mnii,", 18, 19);
    EXPECT_EQ_SELECTED_TEXT("p");
    testDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionExpandFontSizes)
{
    setupFontSize("abcdef ghij kl mnopqr st", "uv", "wi inm mnii,", 18, 19);
    EXPECT_EQ_SELECTED_TEXT("p");
    testDirectionExpand();
}

TEST_F(GranularityStrategyTest, DirectionShrinkTranslateZ)
{
    Text* text = setupTranslateZ("abcdef ghij kl mnopqr iiinmni, abc");
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 21)));
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionShrinkTransform)
{
    Text* text = setupTransform("abcdef ghij kl mnopqr iiinmni, abc");
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 21)));
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionShrinkVerticalAlign)
{
    setupVerticalAlign("abcdef ghij kl mnopqr ii", "inm", "ni, abc", 18, 21);
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionShrinkFontSizes)
{
    setupFontSize("abcdef ghij kl mnopqr ii", "inm", "ni, abc", 18, 21);
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionShrink();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideTranslateZ)
{
    Text* text = setupTranslateZ("abcd efgh ijkl mnopqr iiinmni, abc");
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 21)));
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionSwitchSide();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideTransform)
{
    Text* text = setupTransform("abcd efgh ijkl mnopqr iiinmni, abc");
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 21)));
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionSwitchSide();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideVerticalAlign)
{
    setupVerticalAlign("abcd efgh ijkl", " mnopqr", " iiinmni, abc", 18, 21);
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionSwitchSide();
}

TEST_F(GranularityStrategyTest, DirectionSwitchSideFontSizes)
{
    setupFontSize("abcd efgh i", "jk", "l mnopqr iiinmni, abc", 18, 21);
    EXPECT_EQ_SELECTED_TEXT("pqr");
    testDirectionSwitchSide();
}

// Tests moving extent over to the other side of the vase and immediately
// passing the word boundary and going into word granularity.
TEST_F(GranularityStrategyTest, DirectionSwitchSideWordGranularityThenShrink)
{
    dummyPageHolder().frame().settings()->setDefaultFontSize(12);
    String str = "ab cd efghijkl mnopqr iiin, abc";
    Text* text = document().createTextNode(str);
    document().body()->appendChild(text);
    dummyPageHolder().frame().settings()->setSelectionStrategy(SelectionStrategy::Direction);

    parseText(text);

    // "abcd efgh ijkl mno^pqr|> iiin, abc" (^ means base, | means extent, < means start, and > means end).
    selection().setSelection(VisibleSelection(Position(text, 18), Position(text, 21)));
    EXPECT_EQ_SELECTED_TEXT("pqr");
    // Move to the middle of word #4 selecting it - this will set the offset to
    // be half the width of "iiin".
    selection().moveRangeSelectionExtent(m_wordMiddles[4]);
    EXPECT_EQ_SELECTED_TEXT("pqr iiin");
    // Move to the middle of word #2 - extent will switch over to the other
    // side of the base, and we should enter word granularity since we pass
    // the word boundary. The offset should become negative since the width
    // of "efghjkkl" is greater than that of "iiin".
    int offset = m_letterPos[26].x() - m_wordMiddles[4].x();
    IntPoint p = IntPoint(m_wordMiddles[2].x() - offset - 1, m_wordMiddles[2].y());
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("efghijkl mno");
    p.move(m_letterPos[7].x() - m_letterPos[6].x(), 0);
    selection().moveRangeSelectionExtent(p);
    EXPECT_EQ_SELECTED_TEXT("fghijkl mno");
}

// Make sure we switch to word granularity right away when starting on a
// word boundary and extending.
TEST_F(GranularityStrategyTest, DirectionSwitchStartOnBoundary)
{
    dummyPageHolder().frame().settings()->setDefaultFontSize(12);
    String str = "ab cd efghijkl mnopqr iiin, abc";
    Text* text = document().createTextNode(str);
    document().body()->appendChild(text);
    dummyPageHolder().frame().settings()->setSelectionStrategy(SelectionStrategy::Direction);

    parseText(text);

    // "ab cd efghijkl ^mnopqr |>stuvwi inm," (^ means base and | means extent,
    // > means end).
    selection().setSelection(VisibleSelection(Position(text, 15), Position(text, 22)));
    EXPECT_EQ_SELECTED_TEXT("mnopqr ");
    selection().moveRangeSelectionExtent(m_wordMiddles[4]);
    EXPECT_EQ_SELECTED_TEXT("mnopqr iiin");
}

} // namespace blink
