// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/serializers/StyledMarkupSerializer.h"

#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"

namespace blink {

// Returns the first mismatching index in |input1| and |input2|.
static size_t mismatch(const std::string& input1, const std::string& input2) {
  size_t index = 0;
  for (auto char1 : input1) {
    if (index == input2.size() || char1 != input2[index])
      return index;
    ++index;
  }
  return input1.size();
}

// This is smoke test of |StyledMarkupSerializer|. Full testing will be done
// in layout tests.
class StyledMarkupSerializerTest : public EditingTestBase {
 protected:
  template <typename Strategy>
  std::string serialize(EAnnotateForInterchange = DoNotAnnotateForInterchange);

  template <typename Strategy>
  std::string serializePart(
      const PositionTemplate<Strategy>& start,
      const PositionTemplate<Strategy>& end,
      EAnnotateForInterchange = DoNotAnnotateForInterchange);
};

template <typename Strategy>
std::string StyledMarkupSerializerTest::serialize(
    EAnnotateForInterchange shouldAnnotate) {
  PositionTemplate<Strategy> start = PositionTemplate<Strategy>(
      document().body(), PositionAnchorType::BeforeChildren);
  PositionTemplate<Strategy> end = PositionTemplate<Strategy>(
      document().body(), PositionAnchorType::AfterChildren);
  return createMarkup(start, end, shouldAnnotate).utf8().data();
}

template <typename Strategy>
std::string StyledMarkupSerializerTest::serializePart(
    const PositionTemplate<Strategy>& start,
    const PositionTemplate<Strategy>& end,
    EAnnotateForInterchange shouldAnnotate) {
  return createMarkup(start, end, shouldAnnotate).utf8().data();
}

TEST_F(StyledMarkupSerializerTest, TextOnly) {
  const char* bodyContent = "Hello world!";
  setBodyContent(bodyContent);
  const char* expectedResult =
      "<span style=\"display: inline !important; float: none;\">Hello "
      "world!</span>";
  EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
  EXPECT_EQ(expectedResult, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, BlockFormatting) {
  const char* bodyContent = "<div>Hello world!</div>";
  setBodyContent(bodyContent);
  EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
  EXPECT_EQ(bodyContent, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInput) {
  const char* bodyContent = "<input value='foo'>";
  setBodyContent(bodyContent);
  const char* expectedResult = "<input value=\"foo\">";
  EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
  EXPECT_EQ(expectedResult, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlInputRange) {
  const char* bodyContent = "<input type=range>";
  setBodyContent(bodyContent);
  const char* expectedResult = "<input type=\"range\">";
  EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
  EXPECT_EQ(expectedResult, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlSelect) {
  const char* bodyContent =
      "<select><option value=\"1\">one</option><option "
      "value=\"2\">two</option></select>";
  setBodyContent(bodyContent);
  EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
  EXPECT_EQ(bodyContent, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, FormControlTextArea) {
  const char* bodyContent = "<textarea>foo bar</textarea>";
  setBodyContent(bodyContent);
  const char* expectedResult = "<textarea></textarea>";
  EXPECT_EQ(expectedResult, serialize<EditingStrategy>())
      << "contents of TEXTAREA element should not be appeared.";
  EXPECT_EQ(expectedResult, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, HeadingFormatting) {
  const char* bodyContent = "<h4>Hello world!</h4>";
  setBodyContent(bodyContent);
  EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
  EXPECT_EQ(bodyContent, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, InlineFormatting) {
  const char* bodyContent = "<b>Hello world!</b>";
  setBodyContent(bodyContent);
  EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
  EXPECT_EQ(bodyContent, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, Mixed) {
  const char* bodyContent = "<i>foo<b>bar</b>baz</i>";
  setBodyContent(bodyContent);
  EXPECT_EQ(bodyContent, serialize<EditingStrategy>());
  EXPECT_EQ(bodyContent, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeDistributeOrder) {
  const char* bodyContent =
      "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
  const char* shadowContent =
      "<a><content select=#two></content><content select=#one></content></a>";
  setBodyContent(bodyContent);
  setShadowContent(shadowContent, "host");
  EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>",
            serialize<EditingStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
  EXPECT_EQ(
      "<p id=\"host\"><a><b id=\"two\">22</b><b id=\"one\">11</b></a></p>",
      serialize<EditingInFlatTreeStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeInput) {
  const char* bodyContent =
      "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\"><input "
      "value=\"22\"></b>33</p>";
  const char* shadowContent =
      "<a><content select=#two></content><content select=#one></content></a>";
  setBodyContent(bodyContent);
  setShadowContent(shadowContent, "host");
  EXPECT_EQ(
      "<p id=\"host\"><b id=\"one\">11</b><b id=\"two\"><input "
      "value=\"22\"></b></p>",
      serialize<EditingStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
  EXPECT_EQ(
      "<p id=\"host\"><a><b id=\"two\"><input value=\"22\"></b><b "
      "id=\"one\">11</b></a></p>",
      serialize<EditingInFlatTreeStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeNested) {
  const char* bodyContent =
      "<p id=\"host\">00<b id=\"one\">11</b><b id=\"two\">22</b>33</p>";
  const char* shadowContent1 =
      "<a><content select=#two></content><b id=host2></b><content "
      "select=#one></content></a>";
  const char* shadowContent2 = "NESTED";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot1 = setShadowContent(shadowContent1, "host");
  createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host2",
                                                  shadowContent2);

  EXPECT_EQ("<p id=\"host\"><b id=\"one\">11</b><b id=\"two\">22</b></p>",
            serialize<EditingStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
  EXPECT_EQ(
      "<p id=\"host\"><a><b id=\"two\">22</b><b id=\"host2\">NESTED</b><b "
      "id=\"one\">11</b></a></p>",
      serialize<EditingInFlatTreeStrategy>())
      << "00 and 33 aren't appeared since they aren't distributed.";
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeInterchangedNewline) {
  const char* bodyContent = "<a id=host><b id=one>1</b></a>";
  const char* shadowContent = "<content select=#one></content><div><br></div>";
  setBodyContent(bodyContent);
  setShadowContent(shadowContent, "host");

  std::string resultFromDOMTree =
      serialize<EditingStrategy>(AnnotateForInterchange);
  std::string resultFromFlatTree =
      serialize<EditingInFlatTreeStrategy>(AnnotateForInterchange);
  size_t mismatchedIndex = mismatch(resultFromDOMTree, resultFromFlatTree);

  // Note: We check difference between DOM tree result and flat tree
  // result, because results contain "style" attribute and this test
  // doesn't care about actual value of "style" attribute.
  EXPECT_EQ("/a>", resultFromDOMTree.substr(mismatchedIndex));
  EXPECT_EQ("div><br></div></a><br class=\"Apple-interchange-newline\">",
            resultFromFlatTree.substr(mismatchedIndex));
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNone) {
  const char* bodyContent = "<b>00<i style='display:none'>11</i>22</b>";
  setBodyContent(bodyContent);
  const char* expectedResult = "<b>0022</b>";
  EXPECT_EQ(expectedResult, serialize<EditingStrategy>());
  EXPECT_EQ(expectedResult, serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, StyleDisplayNoneAndNewLines) {
  const char* bodyContent = "<div style='display:none'>11</div>\n\n";
  setBodyContent(bodyContent);
  EXPECT_EQ("", serialize<EditingStrategy>());
  EXPECT_EQ("", serialize<EditingInFlatTreeStrategy>());
}

TEST_F(StyledMarkupSerializerTest, ShadowTreeStyle) {
  const char* bodyContent =
      "<p id='host' style='color: red'><span style='font-weight: bold;'><span "
      "id='one'>11</span></span></p>\n";
  setBodyContent(bodyContent);
  Element* one = document().getElementById("one");
  Text* text = toText(one->firstChild());
  Position startDOM(text, 0);
  Position endDOM(text, 2);
  const std::string& serializedDOM =
      serializePart<EditingStrategy>(startDOM, endDOM, AnnotateForInterchange);

  bodyContent =
      "<p id='host' style='color: red'>00<span id='one'>11</span>22</p>\n";
  const char* shadowContent =
      "<span style='font-weight: bold'><content select=#one></content></span>";
  setBodyContent(bodyContent);
  setShadowContent(shadowContent, "host");
  one = document().getElementById("one");
  text = toText(one->firstChild());
  PositionInFlatTree startICT(text, 0);
  PositionInFlatTree endICT(text, 2);
  const std::string& serializedICT = serializePart<EditingInFlatTreeStrategy>(
      startICT, endICT, AnnotateForInterchange);

  EXPECT_EQ(serializedDOM, serializedICT);
}

TEST_F(StyledMarkupSerializerTest, AcrossShadow) {
  const char* bodyContent =
      "<p id='host1'>[<span id='one'>11</span>]</p><p id='host2'>[<span "
      "id='two'>22</span>]</p>";
  setBodyContent(bodyContent);
  Element* one = document().getElementById("one");
  Element* two = document().getElementById("two");
  Position startDOM(toText(one->firstChild()), 0);
  Position endDOM(toText(two->firstChild()), 2);
  const std::string& serializedDOM =
      serializePart<EditingStrategy>(startDOM, endDOM, AnnotateForInterchange);

  bodyContent =
      "<p id='host1'><span id='one'>11</span></p><p id='host2'><span "
      "id='two'>22</span></p>";
  const char* shadowContent1 = "[<content select=#one></content>]";
  const char* shadowContent2 = "[<content select=#two></content>]";
  setBodyContent(bodyContent);
  setShadowContent(shadowContent1, "host1");
  setShadowContent(shadowContent2, "host2");
  one = document().getElementById("one");
  two = document().getElementById("two");
  PositionInFlatTree startICT(toText(one->firstChild()), 0);
  PositionInFlatTree endICT(toText(two->firstChild()), 2);
  const std::string& serializedICT = serializePart<EditingInFlatTreeStrategy>(
      startICT, endICT, AnnotateForInterchange);

  EXPECT_EQ(serializedDOM, serializedICT);
}

TEST_F(StyledMarkupSerializerTest, AcrossInvisibleElements) {
  const char* bodyContent =
      "<span id='span1' style='display: none'>11</span><span id='span2' "
      "style='display: none'>22</span>";
  setBodyContent(bodyContent);
  Element* span1 = document().getElementById("span1");
  Element* span2 = document().getElementById("span2");
  Position startDOM = Position::firstPositionInNode(span1);
  Position endDOM = Position::lastPositionInNode(span2);
  EXPECT_EQ("", serializePart<EditingStrategy>(startDOM, endDOM));
  PositionInFlatTree startICT = PositionInFlatTree::firstPositionInNode(span1);
  PositionInFlatTree endICT = PositionInFlatTree::lastPositionInNode(span2);
  EXPECT_EQ("", serializePart<EditingInFlatTreeStrategy>(startICT, endICT));
}

}  // namespace blink
