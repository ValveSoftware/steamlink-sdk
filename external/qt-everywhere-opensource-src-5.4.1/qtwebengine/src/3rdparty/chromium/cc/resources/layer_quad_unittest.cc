// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_quad.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/quad_f.h"

namespace cc {
namespace {

TEST(LayerQuadTest, QuadFConversion) {
  gfx::PointF p1(-0.5f, -0.5f);
  gfx::PointF p2(0.5f, -0.5f);
  gfx::PointF p3(0.5f, 0.5f);
  gfx::PointF p4(-0.5f, 0.5f);

  gfx::QuadF quad_cw(p1, p2, p3, p4);
  LayerQuad layer_quad_cw(quad_cw);
  EXPECT_EQ(layer_quad_cw.ToQuadF(), quad_cw);

  gfx::QuadF quad_ccw(p1, p4, p3, p2);
  LayerQuad layer_quad_ccw(quad_ccw);
  EXPECT_EQ(layer_quad_ccw.ToQuadF(), quad_ccw);
}

TEST(LayerQuadTest, Inflate) {
  gfx::PointF p1(-0.5f, -0.5f);
  gfx::PointF p2(0.5f, -0.5f);
  gfx::PointF p3(0.5f, 0.5f);
  gfx::PointF p4(-0.5f, 0.5f);

  gfx::QuadF quad(p1, p2, p3, p4);
  LayerQuad layer_quad(quad);
  quad.Scale(2.f, 2.f);
  layer_quad.Inflate(0.5f);
  EXPECT_EQ(layer_quad.ToQuadF(), quad);
}

}  // namespace
}  // namespace cc
