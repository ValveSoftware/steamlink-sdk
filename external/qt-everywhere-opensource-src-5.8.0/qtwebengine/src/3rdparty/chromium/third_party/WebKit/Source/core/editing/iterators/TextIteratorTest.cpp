/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/iterators/TextIterator.h"

#include "core/editing/EditingTestBase.h"
#include "core/frame/FrameView.h"

namespace blink {

struct DOMTree : NodeTraversal {
    using PositionType = Position;
    using TextIteratorType = TextIterator;
};

struct FlatTree : FlatTreeTraversal {
    using PositionType = PositionInFlatTree;
    using TextIteratorType = TextIteratorInFlatTree;
};

class TextIteratorTest : public EditingTestBase {
protected:
    template <typename Tree>
    std::string iterate(TextIteratorBehavior = TextIteratorDefaultBehavior);

    template <typename Tree>
    std::string iteratePartial(const typename Tree::PositionType& start, const typename Tree::PositionType& end, TextIteratorBehavior = TextIteratorDefaultBehavior);

    Range* getBodyRange() const;

private:
    template <typename Tree>
    std::string iterateWithIterator(typename Tree::TextIteratorType&);
};

template <typename Tree>
std::string TextIteratorTest::iterate(TextIteratorBehavior iteratorBehavior)
{
    Element* body = document().body();
    using PositionType = typename Tree::PositionType;
    auto start = PositionType(body, 0);
    auto end = PositionType(body, Tree::countChildren(*body));
    typename Tree::TextIteratorType iterator(start, end, iteratorBehavior);
    return iterateWithIterator<Tree>(iterator);
}

template <typename Tree>
std::string TextIteratorTest::iteratePartial(const typename Tree::PositionType& start, const typename Tree::PositionType& end, TextIteratorBehavior iteratorBehavior)
{
    typename Tree::TextIteratorType iterator(start, end, iteratorBehavior);
    return iterateWithIterator<Tree>(iterator);
}

template <typename Tree>
std::string TextIteratorTest::iterateWithIterator(typename Tree::TextIteratorType& iterator)
{
    String textChunks;
    for (; !iterator.atEnd(); iterator.advance()) {
        textChunks.append('[');
        textChunks.append(iterator.text().substring(0, iterator.text().length()));
        textChunks.append(']');
    }
    return std::string(textChunks.utf8().data());
}

Range* TextIteratorTest::getBodyRange() const
{
    Range* range = Range::create(document());
    range->selectNode(document().body());
    return range;
}

TEST_F(TextIteratorTest, BitStackOverflow)
{
    const unsigned bitsInWord = sizeof(unsigned) * 8;
    BitStack bs;

    for (unsigned i = 0; i < bitsInWord + 1u; i++)
        bs.push(true);

    bs.pop();

    EXPECT_TRUE(bs.top());
}

TEST_F(TextIteratorTest, BasicIteration)
{
    static const char* input = "<p>Hello, \ntext</p><p>iterator.</p>";
    setBodyContent(input);
    EXPECT_EQ("[Hello, ][text][\n][\n][iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][text][\n][\n][iterator.]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, IgnoreAltTextInTextControls)
{
    static const char* input = "<p>Hello <input type='text' value='value'>!</p>";
    setBodyContent(input);
    EXPECT_EQ("[Hello ][][!]", iterate<DOMTree>(TextIteratorEmitsImageAltText));
    EXPECT_EQ("[Hello ][][\n][value][\n][!]", iterate<FlatTree>(TextIteratorEmitsImageAltText));
}

TEST_F(TextIteratorTest, DisplayAltTextInImageControls)
{
    static const char* input = "<p>Hello <input type='image' alt='alt'>!</p>";
    setBodyContent(input);
    EXPECT_EQ("[Hello ][alt][!]", iterate<DOMTree>(TextIteratorEmitsImageAltText));
    EXPECT_EQ("[Hello ][alt][!]", iterate<FlatTree>(TextIteratorEmitsImageAltText));
}

TEST_F(TextIteratorTest, NotEnteringTextControls)
{
    static const char* input = "<p>Hello <input type='text' value='input'>!</p>";
    setBodyContent(input);
    EXPECT_EQ("[Hello ][][!]", iterate<DOMTree>());
    EXPECT_EQ("[Hello ][][\n][input][\n][!]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOption)
{
    static const char* input = "<p>Hello <input type='text' value='input'>!</p>";
    setBodyContent(input);
    EXPECT_EQ("[Hello ][\n][input][!]", iterate<DOMTree>(TextIteratorEntersTextControls));
    EXPECT_EQ("[Hello ][][\n][input][\n][!]", iterate<FlatTree>(TextIteratorEntersTextControls));
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOptionComplex)
{
    static const char* input = "<input type='text' value='Beginning of range'><div><div><input type='text' value='Under DOM nodes'></div></div><input type='text' value='End of range'>";
    setBodyContent(input);
    EXPECT_EQ("[\n][Beginning of range][\n][Under DOM nodes][\n][End of range]", iterate<DOMTree>(TextIteratorEntersTextControls));
    EXPECT_EQ("[][\n][Beginning of range][\n][][\n][Under DOM nodes][\n][][\n][End of range]", iterate<FlatTree>(TextIteratorEntersTextControls));
}

TEST_F(TextIteratorTest, NotEnteringShadowTree)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // TextIterator doesn't emit "text" since its layoutObject is not created. The shadow tree is ignored.
    EXPECT_EQ("[Hello, ][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][shadow][ iterator.]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithMultipleShadowTrees)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first shadow</span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent2);
    EXPECT_EQ("[Hello, ][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][second shadow][ iterator.]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithNestedShadowTrees)
{
    static const char* bodyContent = "<div>Hello, <span id='host-in-document'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first <span id='host-in-shadow'>shadow</span></span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot1 = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host-in-document", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host-in-shadow", shadowContent2);
    EXPECT_EQ("[Hello, ][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithContentInsertionPoint)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow <content>content</content></span>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // In this case a layoutObject for "text" is created, so it shows up here.
    EXPECT_EQ("[Hello, ][text][ iterator.]", iterate<DOMTree>());
    EXPECT_EQ("[Hello, ][shadow ][text][ iterator.]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // TextIterator emits "shadow" since TextIteratorEntersOpenShadowRoots is specified.
    EXPECT_EQ("[Hello, ][shadow][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][shadow][ iterator.]", iterate<FlatTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithMultipleShadowTreesWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first shadow</span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent2);
    // The first isn't emitted because a layoutObject for the first is not created.
    EXPECT_EQ("[Hello, ][second shadow][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][second shadow][ iterator.]", iterate<FlatTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithNestedShadowTreesWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host-in-document'>text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first <span id='host-in-shadow'>shadow</span></span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot1 = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host-in-document", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host-in-shadow", shadowContent2);
    EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][first ][second shadow][ iterator.]", iterate<FlatTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithContentInsertionPointWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    // In this case a layoutObject for "text" is created, and emitted AFTER any nodes in the shadow tree.
    // This order does not match the order of the rendered texts, but at this moment it's the expected behavior.
    // FIXME: Fix this. We probably need pure-renderer-based implementation of TextIterator to achieve this.
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    EXPECT_EQ("[Hello, ][ shadow][text][ iterator.]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Hello, ][text][ shadow][ iterator.]", iterate<FlatTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, StartingAtNodeInShadowRoot)
{
    static const char* bodyContent = "<div id='outer'>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    Node* outerDiv = document().getElementById("outer");
    Node* spanInShadow = shadowRoot->firstChild();
    Position start(spanInShadow, PositionAnchorType::BeforeChildren);
    Position end(outerDiv, PositionAnchorType::AfterChildren);
    EXPECT_EQ("[ shadow][text][ iterator.]", iteratePartial<DOMTree>(start, end, TextIteratorEntersOpenShadowRoots));

    PositionInFlatTree startInFlatTree(spanInShadow, PositionAnchorType::BeforeChildren);
    PositionInFlatTree endInFlatTree(outerDiv, PositionAnchorType::AfterChildren);
    EXPECT_EQ("[text][ shadow][ iterator.]", iteratePartial<FlatTree>(startInFlatTree, endInFlatTree, TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, FinishingAtNodeInShadowRoot)
{
    static const char* bodyContent = "<div id='outer'>Hello, <span id='host'>text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    setBodyContent(bodyContent);
    ShadowRoot* shadowRoot = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    Node* outerDiv = document().getElementById("outer");
    Node* spanInShadow = shadowRoot->firstChild();
    Position start(outerDiv, PositionAnchorType::BeforeChildren);
    Position end(spanInShadow, PositionAnchorType::AfterChildren);
    EXPECT_EQ("[Hello, ][ shadow]", iteratePartial<DOMTree>(start, end, TextIteratorEntersOpenShadowRoots));

    PositionInFlatTree startInFlatTree(outerDiv, PositionAnchorType::BeforeChildren);
    PositionInFlatTree endInFlatTree(spanInShadow, PositionAnchorType::AfterChildren);
    EXPECT_EQ("[Hello, ][text][ shadow]", iteratePartial<FlatTree>(startInFlatTree, endInFlatTree, TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, FullyClipsContents)
{
    static const char* bodyContent =
        "<div style='overflow: hidden; width: 200px; height: 0;'>"
        "I'm invisible"
        "</div>";
    setBodyContent(bodyContent);
    EXPECT_EQ("", iterate<DOMTree>());
    EXPECT_EQ("", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, IgnoresContainerClip)
{
    static const char* bodyContent =
        "<div style='overflow: hidden; width: 200px; height: 0;'>"
        "<div>I'm not visible</div>"
        "<div style='position: absolute; width: 200px; height: 200px; top: 0; right: 0;'>"
        "but I am!"
        "</div>"
        "</div>";
    setBodyContent(bodyContent);
    EXPECT_EQ("[but I am!]", iterate<DOMTree>());
    EXPECT_EQ("[but I am!]", iterate<FlatTree>());
}

TEST_F(TextIteratorTest, FullyClippedContentsDistributed)
{
    static const char* bodyContent =
        "<div id='host'>"
        "<div>Am I visible?</div>"
        "</div>";
    static const char* shadowContent =
        "<div style='overflow: hidden; width: 200px; height: 0;'>"
        "<content></content>"
        "</div>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // FIXME: The text below is actually invisible but TextIterator currently thinks it's visible.
    EXPECT_EQ("[\n][Am I visible?]", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("", iterate<FlatTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, IgnoresContainersClipDistributed)
{
    static const char* bodyContent =
        "<div id='host' style='overflow: hidden; width: 200px; height: 0;'>"
        "<div>Nobody can find me!</div>"
        "</div>";
    static const char* shadowContent =
        "<div style='position: absolute; width: 200px; height: 200px; top: 0; right: 0;'>"
        "<content></content>"
        "</div>";
    setBodyContent(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);
    // FIXME: The text below is actually visible but TextIterator currently thinks it's invisible.
    // [\n][Nobody can find me!]
    EXPECT_EQ("", iterate<DOMTree>(TextIteratorEntersOpenShadowRoots));
    EXPECT_EQ("[Nobody can find me!]", iterate<FlatTree>(TextIteratorEntersOpenShadowRoots));
}

TEST_F(TextIteratorTest, EmitsReplacementCharForInput)
{
    static const char* bodyContent =
        "<div contenteditable='true'>"
        "Before"
        "<img src='foo.png'>"
        "After"
        "</div>";
    setBodyContent(bodyContent);
    EXPECT_EQ("[Before][\xEF\xBF\xBC][After]", iterate<DOMTree>(TextIteratorEmitsObjectReplacementCharacter));
    EXPECT_EQ("[Before][\xEF\xBF\xBC][After]", iterate<FlatTree>(TextIteratorEmitsObjectReplacementCharacter));
}

TEST_F(TextIteratorTest, RangeLengthWithReplacedElements)
{
    static const char* bodyContent =
        "<div id='div' contenteditable='true'>1<img src='foo.png'>3</div>";
    setBodyContent(bodyContent);
    document().view()->updateAllLifecyclePhases();

    Node* divNode = document().getElementById("div");
    Range* range = Range::create(document(), divNode, 0, divNode, 3);

    EXPECT_EQ(3, TextIterator::rangeLength(range->startPosition(), range->endPosition()));
}

TEST_F(TextIteratorTest, WhitespaceCollapseForReplacedElements)
{
    static const char* bodyContent = "<span>Some text </span> <input type='button' value='Button text'/><span>Some more text</span>";
    setBodyContent(bodyContent);
    EXPECT_EQ("[Some text ][][Some more text]", iterate<DOMTree>(TextIteratorCollapseTrailingSpace));
    EXPECT_EQ("[Some text ][][Button text][Some more text]", iterate<FlatTree>(TextIteratorCollapseTrailingSpace));
}

TEST_F(TextIteratorTest, copyTextTo)
{
    const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* host = document().getElementById("host");
    const char* message = "|iter%d| should have emitted '%s'.";

    EphemeralRangeTemplate<EditingStrategy> range1(EphemeralRangeTemplate<EditingStrategy>::rangeOfContents(*host));
    TextIteratorAlgorithm<EditingStrategy> iter1(range1.startPosition(), range1.endPosition());
    ForwardsTextBuffer output1;
    iter1.copyTextTo(&output1, 0, 2);
    EXPECT_EQ("on", String(output1.data(), output1.size())) << String::format(message, 1, "on").utf8().data();
    iter1.copyTextTo(&output1, 2, 1);
    EXPECT_EQ("one", String(output1.data(), output1.size())) << String::format(message, 1, "one").utf8().data();
    iter1.advance();
    iter1.copyTextTo(&output1, 0, 1);
    EXPECT_EQ("onet", String(output1.data(), output1.size())) << String::format(message, 1, "onet").utf8().data();
    iter1.copyTextTo(&output1, 1, 2);
    EXPECT_EQ("onetwo", String(output1.data(), output1.size())) << String::format(message, 1, "onetwo").utf8().data();

    EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(EphemeralRangeTemplate<EditingInFlatTreeStrategy>::rangeOfContents(*host));
    TextIteratorAlgorithm<EditingInFlatTreeStrategy> iter2(range2.startPosition(), range2.endPosition());
    ForwardsTextBuffer output2;
    iter2.copyTextTo(&output2, 0, 3);
    EXPECT_EQ("thr", String(output2.data(), output2.size())) << String::format(message, 2, "thr").utf8().data();
    iter2.copyTextTo(&output2, 3, 3);
    EXPECT_EQ("three ", String(output2.data(), output2.size())) << String::format(message, 2, "three ").utf8().data();
    iter2.advance();
    iter2.copyTextTo(&output2, 0, 2);
    EXPECT_EQ("three tw", String(output2.data(), output2.size())) << String::format(message, 2, "three tw").utf8().data();
    iter2.copyTextTo(&output2, 2, 1);
    EXPECT_EQ("three two", String(output2.data(), output2.size())) << String::format(message, 2, "three two").utf8().data();
    iter2.advance();
    iter2.copyTextTo(&output2, 0, 1);
    EXPECT_EQ("three two ", String(output2.data(), output2.size())) << String::format(message, 2, "three two ").utf8().data();
    iter2.advance();
    iter2.copyTextTo(&output2, 0, 1);
    EXPECT_EQ("three two o", String(output2.data(), output2.size())) << String::format(message, 2, "three two o").utf8().data();
    iter2.copyTextTo(&output2, 1, 2);
    EXPECT_EQ("three two one", String(output2.data(), output2.size())) << String::format(message, 2, "three two one").utf8().data();
    iter2.advance();
    iter2.copyTextTo(&output2, 0, 2);
    EXPECT_EQ("three two one z", String(output2.data(), output2.size())) << String::format(message, 2, "three two one z").utf8().data();
    iter2.copyTextTo(&output2, 2, 3);
    EXPECT_EQ("three two one zero", String(output2.data(), output2.size())) << String::format(message, 2, "three two one zero").utf8().data();
}

TEST_F(TextIteratorTest, characterAt)
{
    const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* host = document().getElementById("host");

    EphemeralRangeTemplate<EditingStrategy> range1(EphemeralRangeTemplate<EditingStrategy>::rangeOfContents(*host));
    TextIteratorAlgorithm<EditingStrategy> iter1(range1.startPosition(), range1.endPosition());
    const char* message1 = "|iter1| should emit 'one' and 'two'.";
    EXPECT_EQ('o', iter1.characterAt(0)) << message1;
    EXPECT_EQ('n', iter1.characterAt(1)) << message1;
    EXPECT_EQ('e', iter1.characterAt(2)) << message1;
    iter1.advance();
    EXPECT_EQ('t', iter1.characterAt(0)) << message1;
    EXPECT_EQ('w', iter1.characterAt(1)) << message1;
    EXPECT_EQ('o', iter1.characterAt(2)) << message1;

    EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(EphemeralRangeTemplate<EditingInFlatTreeStrategy>::rangeOfContents(*host));
    TextIteratorAlgorithm<EditingInFlatTreeStrategy> iter2(range2.startPosition(), range2.endPosition());
    const char* message2 = "|iter2| should emit 'three ', 'two', ' ', 'one' and ' zero'.";
    EXPECT_EQ('t', iter2.characterAt(0)) << message2;
    EXPECT_EQ('h', iter2.characterAt(1)) << message2;
    EXPECT_EQ('r', iter2.characterAt(2)) << message2;
    EXPECT_EQ('e', iter2.characterAt(3)) << message2;
    EXPECT_EQ('e', iter2.characterAt(4)) << message2;
    EXPECT_EQ(' ', iter2.characterAt(5)) << message2;
    iter2.advance();
    EXPECT_EQ('t', iter2.characterAt(0)) << message2;
    EXPECT_EQ('w', iter2.characterAt(1)) << message2;
    EXPECT_EQ('o', iter2.characterAt(2)) << message2;
    iter2.advance();
    EXPECT_EQ(' ', iter2.characterAt(0)) << message2;
    iter2.advance();
    EXPECT_EQ('o', iter2.characterAt(0)) << message2;
    EXPECT_EQ('n', iter2.characterAt(1)) << message2;
    EXPECT_EQ('e', iter2.characterAt(2)) << message2;
    iter2.advance();
    EXPECT_EQ(' ', iter2.characterAt(0)) << message2;
    EXPECT_EQ('z', iter2.characterAt(1)) << message2;
    EXPECT_EQ('e', iter2.characterAt(2)) << message2;
    EXPECT_EQ('r', iter2.characterAt(3)) << message2;
    EXPECT_EQ('o', iter2.characterAt(4)) << message2;
}

TEST_F(TextIteratorTest, CopyWholeCodePoints)
{
    const char* bodyContent = "&#x13000;&#x13001;&#x13002; &#x13140;&#x13141;.";
    setBodyContent(bodyContent);

    const UChar expected[] = {0xD80C, 0xDC00, 0xD80C, 0xDC01, 0xD80C, 0xDC02, ' ', 0xD80C, 0xDD40, 0xD80C, 0xDD41, '.'};

    EphemeralRange range(EphemeralRange::rangeOfContents(document()));
    TextIterator iter(range.startPosition(), range.endPosition());
    ForwardsTextBuffer buffer;
    EXPECT_EQ(2, iter.copyTextTo(&buffer, 0, 1)) << "Should emit 2 UChars for 'U+13000'.";
    EXPECT_EQ(4, iter.copyTextTo(&buffer, 2, 3)) << "Should emit 4 UChars for 'U+13001U+13002'.";
    EXPECT_EQ(3, iter.copyTextTo(&buffer, 6, 2)) << "Should emit 3 UChars for ' U+13140'.";
    EXPECT_EQ(2, iter.copyTextTo(&buffer, 9, 2)) << "Should emit 2 UChars for 'U+13141'.";
    EXPECT_EQ(1, iter.copyTextTo(&buffer, 11, 1)) << "Should emit 1 UChar for '.'.";
    for (int i = 0; i < 12; i++)
        EXPECT_EQ(expected[i], buffer[i]);
}

} // namespace blink
