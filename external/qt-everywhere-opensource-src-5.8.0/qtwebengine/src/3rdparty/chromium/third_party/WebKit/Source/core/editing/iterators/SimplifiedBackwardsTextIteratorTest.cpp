// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/SimplifiedBackwardsTextIterator.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/EphemeralRange.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class SimplifiedBackwardsTextIteratorTest : public EditingTestBase {
};

template <typename Strategy>
static String extractString(const Element& element)
{
    const EphemeralRangeTemplate<Strategy> range = EphemeralRangeTemplate<Strategy>::rangeOfContents(element);
    BackwardsTextBuffer buffer;
    for (SimplifiedBackwardsTextIteratorAlgorithm<Strategy> it(range.startPosition(), range.endPosition()); !it.atEnd(); it.advance()) {
        it.copyTextTo(&buffer);
    }
    return String(buffer.data(), buffer.size());
}

TEST_F(SimplifiedBackwardsTextIteratorTest, SubrangeWithReplacedElements)
{
    static const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* host = document().getElementById("host");

    // We should not apply DOM tree version to containing shadow tree in
    // general. To record current behavior, we have this test. even if it
    // isn't intuitive.
    EXPECT_EQ("onetwo", extractString<EditingStrategy>(*host));
    EXPECT_EQ("three two one zero", extractString<EditingInFlatTreeStrategy>(*host));
}

TEST_F(SimplifiedBackwardsTextIteratorTest, characterAt)
{
    const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* host = document().getElementById("host");

    EphemeralRangeTemplate<EditingStrategy> range1(EphemeralRangeTemplate<EditingStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> backIter1(range1.startPosition(), range1.endPosition());
    const char* message1 = "|backIter1| should emit 'one' and 'two' in reverse order.";
    EXPECT_EQ('o', backIter1.characterAt(0)) << message1;
    EXPECT_EQ('w', backIter1.characterAt(1)) << message1;
    EXPECT_EQ('t', backIter1.characterAt(2)) << message1;
    backIter1.advance();
    EXPECT_EQ('e', backIter1.characterAt(0)) << message1;
    EXPECT_EQ('n', backIter1.characterAt(1)) << message1;
    EXPECT_EQ('o', backIter1.characterAt(2)) << message1;

    EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(EphemeralRangeTemplate<EditingInFlatTreeStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy> backIter2(range2.startPosition(), range2.endPosition());
    const char* message2 = "|backIter2| should emit 'three ', 'two', ' ', 'one' and ' zero' in reverse order.";
    EXPECT_EQ('o', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('r', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('e', backIter2.characterAt(2)) << message2;
    EXPECT_EQ('z', backIter2.characterAt(3)) << message2;
    EXPECT_EQ(' ', backIter2.characterAt(4)) << message2;
    backIter2.advance();
    EXPECT_EQ('e', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('n', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('o', backIter2.characterAt(2)) << message2;
    backIter2.advance();
    EXPECT_EQ(' ', backIter2.characterAt(0)) << message2;
    backIter2.advance();
    EXPECT_EQ('o', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('w', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('t', backIter2.characterAt(2)) << message2;
    backIter2.advance();
    EXPECT_EQ(' ', backIter2.characterAt(0)) << message2;
    EXPECT_EQ('e', backIter2.characterAt(1)) << message2;
    EXPECT_EQ('e', backIter2.characterAt(2)) << message2;
    EXPECT_EQ('r', backIter2.characterAt(3)) << message2;
    EXPECT_EQ('h', backIter2.characterAt(4)) << message2;
    EXPECT_EQ('t', backIter2.characterAt(5)) << message2;
}

TEST_F(SimplifiedBackwardsTextIteratorTest, copyTextTo)
{
    const char* bodyContent = "<a id=host><b id=one>one</b> not appeared <b id=two>two</b></a>";
    const char* shadowContent = "three <content select=#two></content> <content select=#one></content> zero";
    setBodyContent(bodyContent);
    setShadowContent(shadowContent, "host");

    Element* host = document().getElementById("host");
    const char* message = "|backIter%d| should have emitted '%s' in reverse order.";

    EphemeralRangeTemplate<EditingStrategy> range1(EphemeralRangeTemplate<EditingStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingStrategy> backIter1(range1.startPosition(), range1.endPosition());
    BackwardsTextBuffer output1;
    backIter1.copyTextTo(&output1, 0, 2);
    EXPECT_EQ("wo", String(output1.data(), output1.size())) << String::format(message, 1, "wo").utf8().data();
    backIter1.copyTextTo(&output1, 2, 1);
    EXPECT_EQ("two", String(output1.data(), output1.size())) << String::format(message, 1, "two").utf8().data();
    backIter1.advance();
    backIter1.copyTextTo(&output1, 0, 1);
    EXPECT_EQ("etwo", String(output1.data(), output1.size())) << String::format(message, 1, "etwo").utf8().data();
    backIter1.copyTextTo(&output1, 1, 2);
    EXPECT_EQ("onetwo", String(output1.data(), output1.size())) << String::format(message, 1, "onetwo").utf8().data();

    EphemeralRangeTemplate<EditingInFlatTreeStrategy> range2(EphemeralRangeTemplate<EditingInFlatTreeStrategy>::rangeOfContents(*host));
    SimplifiedBackwardsTextIteratorAlgorithm<EditingInFlatTreeStrategy> backIter2(range2.startPosition(), range2.endPosition());
    BackwardsTextBuffer output2;
    backIter2.copyTextTo(&output2, 0, 2);
    EXPECT_EQ("ro", String(output2.data(), output2.size())) << String::format(message, 2, "ro").utf8().data();
    backIter2.copyTextTo(&output2, 2, 3);
    EXPECT_EQ(" zero", String(output2.data(), output2.size())) << String::format(message, 2, " zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(&output2, 0, 1);
    EXPECT_EQ("e zero", String(output2.data(), output2.size())) << String::format(message, 2, "e zero").utf8().data();
    backIter2.copyTextTo(&output2, 1, 2);
    EXPECT_EQ("one zero", String(output2.data(), output2.size())) << String::format(message, 2, "one zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(&output2, 0, 1);
    EXPECT_EQ(" one zero", String(output2.data(), output2.size())) << String::format(message, 2, " one zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(&output2, 0, 2);
    EXPECT_EQ("wo one zero", String(output2.data(), output2.size())) << String::format(message, 2, "wo one zero").utf8().data();
    backIter2.copyTextTo(&output2, 2, 1);
    EXPECT_EQ("two one zero", String(output2.data(), output2.size())) << String::format(message, 2, "two one zero").utf8().data();
    backIter2.advance();
    backIter2.copyTextTo(&output2, 0, 3);
    EXPECT_EQ("ee two one zero", String(output2.data(), output2.size())) << String::format(message, 2, "ee two one zero").utf8().data();
    backIter2.copyTextTo(&output2, 3, 3);
    EXPECT_EQ("three two one zero", String(output2.data(), output2.size())) << String::format(message, 2, "three two one zero").utf8().data();
}

TEST_F(SimplifiedBackwardsTextIteratorTest, CopyWholeCodePoints)
{
    const char* bodyContent = "&#x13000;&#x13001;&#x13002; &#x13140;&#x13141;.";
    setBodyContent(bodyContent);

    const UChar expected[] = {0xD80C, 0xDC00, 0xD80C, 0xDC01, 0xD80C, 0xDC02, ' ', 0xD80C, 0xDD40, 0xD80C, 0xDD41, '.'};

    EphemeralRange range(EphemeralRange::rangeOfContents(document()));
    SimplifiedBackwardsTextIterator iter(range.startPosition(), range.endPosition());
    BackwardsTextBuffer buffer;
    EXPECT_EQ(1, iter.copyTextTo(&buffer, 0, 1)) << "Should emit 1 UChar for '.'.";
    EXPECT_EQ(2, iter.copyTextTo(&buffer, 1, 1)) << "Should emit 2 UChars for 'U+13141'.";
    EXPECT_EQ(2, iter.copyTextTo(&buffer, 3, 2)) << "Should emit 2 UChars for 'U+13140'.";
    EXPECT_EQ(5, iter.copyTextTo(&buffer, 5, 4)) << "Should emit 5 UChars for 'U+13001U+13002 '.";
    EXPECT_EQ(2, iter.copyTextTo(&buffer, 10, 2)) << "Should emit 2 UChars for 'U+13000'.";
    for (int i = 0; i < 12; i++)
        EXPECT_EQ(expected[i], buffer[i]);
}

} // namespace blink
