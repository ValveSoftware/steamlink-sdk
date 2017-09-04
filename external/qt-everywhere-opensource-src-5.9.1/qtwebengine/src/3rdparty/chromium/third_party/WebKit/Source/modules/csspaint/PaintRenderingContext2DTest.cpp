// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/PaintRenderingContext2D.h"

#include "platform/graphics/ImageBuffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

static const int s_width = 50;
static const int s_height = 75;
static const float s_zoom = 1.0;

class PaintRenderingContext2DTest : public ::testing::Test {
 protected:
  void SetUp() override;

  Persistent<PaintRenderingContext2D> m_ctx;
};

void PaintRenderingContext2DTest::SetUp() {
  m_ctx = PaintRenderingContext2D::create(
      ImageBuffer::create(IntSize(s_width, s_height)), false /* hasAlpha */,
      s_zoom);
}

void trySettingStrokeStyle(PaintRenderingContext2D* ctx,
                           const String& expected,
                           const String& value) {
  StringOrCanvasGradientOrCanvasPattern result, arg, dummy;
  dummy.setString("red");
  arg.setString(value);
  ctx->setStrokeStyle(dummy);
  ctx->setStrokeStyle(arg);
  ctx->strokeStyle(result);
  EXPECT_EQ(expected, result.getAsString());
}

TEST_F(PaintRenderingContext2DTest, testParseColorOrCurrentColor) {
  trySettingStrokeStyle(m_ctx.get(), "#0000ff", "blue");
  trySettingStrokeStyle(m_ctx.get(), "#000000", "currentColor");
}

TEST_F(PaintRenderingContext2DTest, testWidthAndHeight) {
  EXPECT_EQ(s_width, m_ctx->width());
  EXPECT_EQ(s_height, m_ctx->height());
}

TEST_F(PaintRenderingContext2DTest, testBasicState) {
  const double shadowBlurBefore = 2;
  const double shadowBlurAfter = 3;

  const String lineJoinBefore = "bevel";
  const String lineJoinAfter = "round";

  m_ctx->setShadowBlur(shadowBlurBefore);
  m_ctx->setLineJoin(lineJoinBefore);
  EXPECT_EQ(shadowBlurBefore, m_ctx->shadowBlur());
  EXPECT_EQ(lineJoinBefore, m_ctx->lineJoin());

  m_ctx->save();

  m_ctx->setShadowBlur(shadowBlurAfter);
  m_ctx->setLineJoin(lineJoinAfter);
  EXPECT_EQ(shadowBlurAfter, m_ctx->shadowBlur());
  EXPECT_EQ(lineJoinAfter, m_ctx->lineJoin());

  m_ctx->restore();

  EXPECT_EQ(shadowBlurBefore, m_ctx->shadowBlur());
  EXPECT_EQ(lineJoinBefore, m_ctx->lineJoin());
}

}  // namespace
}  // namespace blink
