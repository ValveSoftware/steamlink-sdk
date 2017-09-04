// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentStatisticsCollector.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/testing/DummyPageHolder.h"
#include "public/platform/WebDistillability.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

// Saturate the length of a paragraph to save time.
const unsigned kTextContentLengthSaturation = 1000;

// Filter out short P elements. The threshold is set to around 2 English
// sentences.
const unsigned kParagraphLengthThreshold = 140;

class DocumentStatisticsCollectorTest : public ::testing::Test {
 protected:
  void SetUp() override;

  void TearDown() override { ThreadState::current()->collectAllGarbage(); }

  Document& document() const { return m_dummyPageHolder->document(); }

  void setHtmlInnerHTML(const String&);

 private:
  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
};

void DocumentStatisticsCollectorTest::SetUp() {
  m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
}

void DocumentStatisticsCollectorTest::setHtmlInnerHTML(
    const String& htmlContent) {
  document().documentElement()->setInnerHTML((htmlContent));
}

// This test checks open graph articles can be recognized.
TEST_F(DocumentStatisticsCollectorTest, HasOpenGraphArticle) {
  setHtmlInnerHTML(
      "<head>"
      // Note the case-insensitive matching of the word "article".
      "    <meta property='og:type' content='arTiclE' />"
      "</head>");
  WebDistillabilityFeatures features =
      DocumentStatisticsCollector::collectStatistics(document());

  EXPECT_TRUE(features.openGraph);
}

// This test checks non-existence of open graph articles can be recognized.
TEST_F(DocumentStatisticsCollectorTest, NoOpenGraphArticle) {
  setHtmlInnerHTML(
      "<head>"
      "    <meta property='og:type' content='movie' />"
      "</head>");
  WebDistillabilityFeatures features =
      DocumentStatisticsCollector::collectStatistics(document());

  EXPECT_FALSE(features.openGraph);
}

// This test checks element counts are correct.
TEST_F(DocumentStatisticsCollectorTest, CountElements) {
  setHtmlInnerHTML(
      "<form>"
      "    <input type='text'>"
      "    <input type='password'>"
      "</form>"
      "<pre></pre>"
      "<p><a>    </a></p>"
      "<ul><li><p><a>    </a></p></li></ul>");
  WebDistillabilityFeatures features =
      DocumentStatisticsCollector::collectStatistics(document());

  EXPECT_FALSE(features.openGraph);

  EXPECT_EQ(10u, features.elementCount);
  EXPECT_EQ(2u, features.anchorCount);
  EXPECT_EQ(1u, features.formCount);
  EXPECT_EQ(1u, features.textInputCount);
  EXPECT_EQ(1u, features.passwordInputCount);
  EXPECT_EQ(2u, features.pCount);
  EXPECT_EQ(1u, features.preCount);
}

// This test checks score calculations are correct.
TEST_F(DocumentStatisticsCollectorTest, CountScore) {
  setHtmlInnerHTML(
      "<p class='menu' id='article'>1</p>"  // textContentLength = 1
      "<ul><li><p>12</p></li></ul>"  // textContentLength = 2, skipped because
                                     // under li
      "<p class='menu'>123</p>"      // textContentLength = 3, skipped because
                                     // unlikelyCandidates
      "<p>"
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234"
      "</p>"                               // textContentLength = 144
      "<p style='display:none'>12345</p>"  // textContentLength = 5, skipped
                                           // because invisible
      "<div style='display:none'><p>123456</p></div>"  // textContentLength = 6,
                                                       // skipped because
                                                       // invisible
      "<div style='visibility:hidden'><p>1234567</p></div>"  // textContentLength
                                                             // = 7, skipped
                                                             // because
                                                             // invisible
      "<p style='opacity:0'>12345678</p>"  // textContentLength = 8, skipped
                                           // because invisible
      "<p><a href='#'>1234 </a>6 <b> 9</b></p>"  // textContentLength = 9
      "<ul><li></li><p>123456789012</p></ul>"    // textContentLength = 12
      );
  WebDistillabilityFeatures features =
      DocumentStatisticsCollector::collectStatistics(document());

  EXPECT_DOUBLE_EQ(features.mozScore, sqrt(144 - kParagraphLengthThreshold));
  EXPECT_DOUBLE_EQ(features.mozScoreAllSqrt,
                   1 + sqrt(144) + sqrt(9) + sqrt(12));
  EXPECT_DOUBLE_EQ(features.mozScoreAllLinear, 1 + 144 + 9 + 12);
}

// This test checks saturation of score calculations is correct.
TEST_F(DocumentStatisticsCollectorTest, CountScoreSaturation) {
  StringBuilder html;
  for (int i = 0; i < 10; i++) {
    html.append("<p>");
    for (int j = 0; j < 1000; j++) {
      html.append("0123456789");
    }
    html.append("</p>");
  }
  setHtmlInnerHTML(html.toString());
  WebDistillabilityFeatures features =
      DocumentStatisticsCollector::collectStatistics(document());

  double error = 1e-5;
  EXPECT_NEAR(features.mozScore, 6 * sqrt(kTextContentLengthSaturation -
                                          kParagraphLengthThreshold),
              error);
  EXPECT_NEAR(features.mozScoreAllSqrt, 6 * sqrt(kTextContentLengthSaturation),
              error);
  EXPECT_NEAR(features.mozScoreAllLinear, 6 * kTextContentLengthSaturation,
              error);
}

}  // namespace blink
