// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TextPainter.h"

#include "core/CSSPropertyNames.h"
#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/api/LineLayoutText.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ShadowData.h"
#include "core/style/ShadowList.h"
#include "platform/graphics/paint/PaintController.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {
namespace {

class TextPainterTest : public RenderingTest {
 public:
  TextPainterTest()
      : m_layoutText(nullptr),
        m_paintController(PaintController::create()),
        m_context(*m_paintController) {}

 protected:
  LineLayoutText getLineLayoutText() { return LineLayoutText(m_layoutText); }

  PaintInfo createPaintInfo(bool usesTextAsClip, bool isPrinting) {
    return PaintInfo(
        m_context, IntRect(),
        usesTextAsClip ? PaintPhaseTextClip : PaintPhaseSelfBlockBackgroundOnly,
        isPrinting ? GlobalPaintPrinting : GlobalPaintNormalPhase, 0);
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    setBodyInnerHTML("Hello world");
    m_layoutText =
        toLayoutText(document().body()->firstChild()->layoutObject());
    ASSERT_TRUE(m_layoutText);
    ASSERT_EQ("Hello world", m_layoutText->text());
  }

  LayoutText* m_layoutText;
  std::unique_ptr<PaintController> m_paintController;
  GraphicsContext m_context;
};

TEST_F(TextPainterTest, TextPaintingStyle_Simple) {
  document().body()->setInlineStyleProperty(CSSPropertyColor, CSSValueBlue);
  document().view()->updateAllLifecyclePhases();

  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      getLineLayoutText(), getLineLayoutText().styleRef(),
      createPaintInfo(false /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color(0, 0, 255), textStyle.fillColor);
  EXPECT_EQ(Color(0, 0, 255), textStyle.strokeColor);
  EXPECT_EQ(Color(0, 0, 255), textStyle.emphasisMarkColor);
  EXPECT_EQ(0, textStyle.strokeWidth);
  EXPECT_EQ(nullptr, textStyle.shadow);
}

TEST_F(TextPainterTest, TextPaintingStyle_AllProperties) {
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                            CSSValueRed);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                            CSSValueLime);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextEmphasisColor,
                                            CSSValueBlue);
  document().body()->setInlineStyleProperty(
      CSSPropertyWebkitTextStrokeWidth, 4, CSSPrimitiveValue::UnitType::Pixels);
  document().body()->setInlineStyleProperty(CSSPropertyTextShadow,
                                            "1px 2px 3px yellow");
  document().view()->updateAllLifecyclePhases();

  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      getLineLayoutText(), getLineLayoutText().styleRef(),
      createPaintInfo(false /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color(255, 0, 0), textStyle.fillColor);
  EXPECT_EQ(Color(0, 255, 0), textStyle.strokeColor);
  EXPECT_EQ(Color(0, 0, 255), textStyle.emphasisMarkColor);
  EXPECT_EQ(4, textStyle.strokeWidth);
  ASSERT_NE(nullptr, textStyle.shadow);
  EXPECT_EQ(1u, textStyle.shadow->shadows().size());
  EXPECT_EQ(1, textStyle.shadow->shadows()[0].x());
  EXPECT_EQ(2, textStyle.shadow->shadows()[0].y());
  EXPECT_EQ(3, textStyle.shadow->shadows()[0].blur());
  EXPECT_EQ(Color(255, 255, 0),
            textStyle.shadow->shadows()[0].color().getColor());
}

TEST_F(TextPainterTest, TextPaintingStyle_UsesTextAsClip) {
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                            CSSValueRed);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                            CSSValueLime);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextEmphasisColor,
                                            CSSValueBlue);
  document().body()->setInlineStyleProperty(
      CSSPropertyWebkitTextStrokeWidth, 4, CSSPrimitiveValue::UnitType::Pixels);
  document().body()->setInlineStyleProperty(CSSPropertyTextShadow,
                                            "1px 2px 3px yellow");
  document().view()->updateAllLifecyclePhases();

  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      getLineLayoutText(), getLineLayoutText().styleRef(),
      createPaintInfo(true /* usesTextAsClip */, false /* isPrinting */));
  EXPECT_EQ(Color::black, textStyle.fillColor);
  EXPECT_EQ(Color::black, textStyle.strokeColor);
  EXPECT_EQ(Color::black, textStyle.emphasisMarkColor);
  EXPECT_EQ(4, textStyle.strokeWidth);
  EXPECT_EQ(nullptr, textStyle.shadow);
}

TEST_F(TextPainterTest,
       TextPaintingStyle_ForceBackgroundToWhite_NoAdjustmentNeeded) {
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                            CSSValueRed);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                            CSSValueLime);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextEmphasisColor,
                                            CSSValueBlue);
  document().body()->setInlineStyleProperty(CSSPropertyWebkitPrintColorAdjust,
                                            CSSValueEconomy);
  document().settings()->setShouldPrintBackgrounds(false);
  document().setPrinting(Document::Printing);
  document().view()->updateAllLifecyclePhases();

  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      getLineLayoutText(), getLineLayoutText().styleRef(),
      createPaintInfo(false /* usesTextAsClip */, true /* isPrinting */));
  EXPECT_EQ(Color(255, 0, 0), textStyle.fillColor);
  EXPECT_EQ(Color(0, 255, 0), textStyle.strokeColor);
  EXPECT_EQ(Color(0, 0, 255), textStyle.emphasisMarkColor);
}

TEST_F(TextPainterTest, TextPaintingStyle_ForceBackgroundToWhite_Darkened) {
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextFillColor,
                                            "rgb(255, 220, 220)");
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextStrokeColor,
                                            "rgb(220, 255, 220)");
  document().body()->setInlineStyleProperty(CSSPropertyWebkitTextEmphasisColor,
                                            "rgb(220, 220, 255)");
  document().body()->setInlineStyleProperty(CSSPropertyWebkitPrintColorAdjust,
                                            CSSValueEconomy);
  document().settings()->setShouldPrintBackgrounds(false);
  document().setPrinting(Document::Printing);
  document().view()->updateAllLifecyclePhases();

  TextPainter::Style textStyle = TextPainter::textPaintingStyle(
      getLineLayoutText(), getLineLayoutText().styleRef(),
      createPaintInfo(false /* usesTextAsClip */, true /* isPrinting */));
  EXPECT_EQ(Color(255, 220, 220).dark(), textStyle.fillColor);
  EXPECT_EQ(Color(220, 255, 220).dark(), textStyle.strokeColor);
  EXPECT_EQ(Color(220, 220, 255).dark(), textStyle.emphasisMarkColor);
}

}  // namespace
}  // namespace blink
