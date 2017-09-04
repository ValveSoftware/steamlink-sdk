// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/animation/AnimationInputHelpers.h"

#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/animation/TimingFunction.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class AnimationAnimationInputHelpersTest : public ::testing::Test {
 public:
  CSSPropertyID keyframeAttributeToCSSProperty(const String& property) {
    return AnimationInputHelpers::keyframeAttributeToCSSProperty(property,
                                                                 *document);
  }

  PassRefPtr<TimingFunction> parseTimingFunction(
      const String& string,
      ExceptionState& exceptionState) {
    return AnimationInputHelpers::parseTimingFunction(string, document,
                                                      exceptionState);
  }

  void timingFunctionRoundTrips(const String& string,
                                ExceptionState& exceptionState) {
    ASSERT_FALSE(exceptionState.hadException());
    RefPtr<TimingFunction> timingFunction =
        parseTimingFunction(string, exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_NE(nullptr, timingFunction);
    EXPECT_EQ(string, timingFunction->toString());
    exceptionState.clearException();
  }

  void timingFunctionThrows(const String& string,
                            ExceptionState& exceptionState) {
    ASSERT_FALSE(exceptionState.hadException());
    RefPtr<TimingFunction> timingFunction =
        parseTimingFunction(string, exceptionState);
    EXPECT_TRUE(exceptionState.hadException());
    EXPECT_EQ(V8TypeError, exceptionState.code());
    exceptionState.clearException();
  }

 protected:
  void SetUp() override {
    pageHolder = DummyPageHolder::create();
    document = &pageHolder->document();
  }

  void TearDown() override {
    document.release();
    ThreadState::current()->collectAllGarbage();
  }

  std::unique_ptr<DummyPageHolder> pageHolder;
  Persistent<Document> document;
};

TEST_F(AnimationAnimationInputHelpersTest, ParseKeyframePropertyAttributes) {
  EXPECT_EQ(CSSPropertyLineHeight,
            keyframeAttributeToCSSProperty("lineHeight"));
  EXPECT_EQ(CSSPropertyBorderTopWidth,
            keyframeAttributeToCSSProperty("borderTopWidth"));
  EXPECT_EQ(CSSPropertyWidth, keyframeAttributeToCSSProperty("width"));
  EXPECT_EQ(CSSPropertyFloat, keyframeAttributeToCSSProperty("float"));
  EXPECT_EQ(CSSPropertyFloat, keyframeAttributeToCSSProperty("cssFloat"));
  EXPECT_EQ(CSSPropertyVariable, keyframeAttributeToCSSProperty("--"));
  EXPECT_EQ(CSSPropertyVariable, keyframeAttributeToCSSProperty("---"));
  EXPECT_EQ(CSSPropertyVariable, keyframeAttributeToCSSProperty("--x"));
  EXPECT_EQ(CSSPropertyVariable,
            keyframeAttributeToCSSProperty("--webkit-custom-property"));

  EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty(""));
  EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("-"));
  EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("line-height"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("border-topWidth"));
  EXPECT_EQ(CSSPropertyInvalid, keyframeAttributeToCSSProperty("Width"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("-epub-text-transform"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("EpubTextTransform"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("-internal-marquee-repetition"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("InternalMarqueeRepetition"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("-webkit-filter"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("-webkit-transform"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("webkitTransform"));
  EXPECT_EQ(CSSPropertyInvalid,
            keyframeAttributeToCSSProperty("WebkitTransform"));
}

TEST_F(AnimationAnimationInputHelpersTest, ParseAnimationTimingFunction) {
  TrackExceptionState exceptionState;
  timingFunctionThrows("", exceptionState);
  timingFunctionThrows("initial", exceptionState);
  timingFunctionThrows("inherit", exceptionState);
  timingFunctionThrows("unset", exceptionState);

  timingFunctionRoundTrips("ease", exceptionState);
  timingFunctionRoundTrips("linear", exceptionState);
  timingFunctionRoundTrips("ease-in", exceptionState);
  timingFunctionRoundTrips("ease-out", exceptionState);
  timingFunctionRoundTrips("ease-in-out", exceptionState);
  timingFunctionRoundTrips("cubic-bezier(0.1, 5, 0.23, 0)", exceptionState);

  EXPECT_EQ("steps(1, start)",
            parseTimingFunction("step-start", exceptionState)->toString());
  EXPECT_EQ("steps(1, middle)",
            parseTimingFunction("step-middle", exceptionState)->toString());
  EXPECT_EQ("steps(1)",
            parseTimingFunction("step-end", exceptionState)->toString());
  EXPECT_EQ("steps(3, start)",
            parseTimingFunction("steps(3, start)", exceptionState)->toString());
  EXPECT_EQ(
      "steps(3, middle)",
      parseTimingFunction("steps(3, middle)", exceptionState)->toString());
  EXPECT_EQ("steps(3)",
            parseTimingFunction("steps(3, end)", exceptionState)->toString());
  EXPECT_EQ("steps(3)",
            parseTimingFunction("steps(3)", exceptionState)->toString());

  timingFunctionThrows("steps(3, nowhere)", exceptionState);
  timingFunctionThrows("steps(-3, end)", exceptionState);
  timingFunctionThrows("cubic-bezier(0.1, 0, 4, 0.4)", exceptionState);
}

}  // namespace blink
