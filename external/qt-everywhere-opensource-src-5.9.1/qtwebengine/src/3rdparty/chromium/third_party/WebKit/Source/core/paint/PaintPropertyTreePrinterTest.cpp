// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreePrinter.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTestHelper.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifndef NDEBUG

namespace blink {

typedef bool TestParamRootLayerScrolling;
class PaintPropertyTreePrinterTest
    : public ::testing::WithParamInterface<TestParamRootLayerScrolling>,
      private ScopedSlimmingPaintV2ForTest,
      private ScopedRootLayerScrollingForTest,
      public RenderingTest {
 public:
  PaintPropertyTreePrinterTest()
      : ScopedSlimmingPaintV2ForTest(true),
        ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildFrameLoaderClient::create()) {}

 private:
  void SetUp() override {
    Settings::setMockScrollbarsEnabled(true);

    RenderingTest::SetUp();
    enableCompositing();
  }

  void TearDown() override {
    RenderingTest::TearDown();

    Settings::setMockScrollbarsEnabled(false);
  }
};

INSTANTIATE_TEST_CASE_P(All, PaintPropertyTreePrinterTest, ::testing::Bool());

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTree) {
  setBodyInnerHTML("hello world");
  String transformTreeAsString =
      transformPropertyTreeAsString(*document().view());
  EXPECT_THAT(transformTreeAsString.ascii().data(),
              testing::MatchesRegex("root .*"
                                    "  .*Translation \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTree) {
  setBodyInnerHTML("hello world");
  String clipTreeAsString = clipPropertyTreeAsString(*document().view());
  EXPECT_THAT(clipTreeAsString.ascii().data(),
              testing::MatchesRegex("root .*"
                                    "  .*Clip \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTree) {
  setBodyInnerHTML("<div style='opacity: 0.9;'>hello world</div>");
  String effectTreeAsString = effectPropertyTreeAsString(*document().view());
  EXPECT_THAT(effectTreeAsString.ascii().data(),
              testing::MatchesRegex("root .*"
                                    "  Effect \\(LayoutBlockFlow DIV\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTree) {
  setBodyInnerHTML("<div style='height: 4000px;'>hello world</div>");
  String scrollTreeAsString = scrollPropertyTreeAsString(*document().view());
  EXPECT_THAT(scrollTreeAsString.ascii().data(),
              testing::MatchesRegex("root .*"
                                    "  Scroll \\(.*\\) .*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleTransformTreePath) {
  setBodyInnerHTML(
      "<div id='transform' style='transform: translate3d(10px, 10px, "
      "0px);'></div>");
  LayoutObject* transformedObject =
      document().getElementById("transform")->layoutObject();
  const auto* transformedObjectProperties =
      transformedObject->paintProperties();
  String transformPathAsString = transformPaintPropertyPathAsString(
      transformedObjectProperties->transform());
  EXPECT_THAT(transformPathAsString.ascii().data(),
              testing::MatchesRegex("root .* transform.*"
                                    "  .* transform.*"
                                    "    .* transform.*"
                                    "       .* transform.*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleClipTreePath) {
  setBodyInnerHTML(
      "<div id='clip' style='position: absolute; clip: rect(10px, 80px, 70px, "
      "40px);'></div>");
  LayoutObject* clippedObject =
      document().getElementById("clip")->layoutObject();
  const auto* clippedObjectProperties = clippedObject->paintProperties();
  String clipPathAsString =
      clipPaintPropertyPathAsString(clippedObjectProperties->cssClip());
  EXPECT_THAT(clipPathAsString.ascii().data(),
              testing::MatchesRegex("root .* rect.*"
                                    "  .* rect.*"
                                    "    .* rect.*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleEffectTreePath) {
  setBodyInnerHTML("<div id='effect' style='opacity: 0.9;'></div>");
  LayoutObject* effectObject =
      document().getElementById("effect")->layoutObject();
  const auto* effectObjectProperties = effectObject->paintProperties();
  String effectPathAsString =
      effectPaintPropertyPathAsString(effectObjectProperties->effect());
  EXPECT_THAT(effectPathAsString.ascii().data(),
              testing::MatchesRegex("root .* opacity.*"
                                    "  .* opacity.*"));
}

TEST_P(PaintPropertyTreePrinterTest, SimpleScrollTreePath) {
  setBodyInnerHTML(
      "<div id='scroll' style='overflow: scroll; height: 100px;'>"
      "  <div id='forceScroll' style='height: 4000px;'></div>"
      "</div>");
  LayoutObject* scrollObject =
      document().getElementById("scroll")->layoutObject();
  const auto* scrollObjectProperties = scrollObject->paintProperties();
  String scrollPathAsString =
      scrollPaintPropertyPathAsString(scrollObjectProperties->scroll());
  EXPECT_THAT(scrollPathAsString.ascii().data(),
              testing::MatchesRegex("root .* scroll.*"
                                    "  .* scroll.*"));
}

}  // namespace blink

#endif
