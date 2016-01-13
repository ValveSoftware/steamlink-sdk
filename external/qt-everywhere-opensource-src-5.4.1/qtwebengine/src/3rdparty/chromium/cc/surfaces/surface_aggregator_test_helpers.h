// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_AGGREGATOR_TEST_HELPERS_H_
#define CC_SURFACES_SURFACE_AGGREGATOR_TEST_HELPERS_H_

#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace cc {

class Surface;
class TestRenderPass;

namespace test {

struct Quad {
  static Quad SolidColorQuad(SkColor color) {
    Quad quad;
    quad.material = DrawQuad::SOLID_COLOR;
    quad.color = color;
    return quad;
  }

  static Quad SurfaceQuad(SurfaceId surface_id) {
    Quad quad;
    quad.material = DrawQuad::SURFACE_CONTENT;
    quad.surface_id = surface_id;
    return quad;
  }

  static Quad RenderPassQuad(RenderPass::Id id) {
    Quad quad;
    quad.material = DrawQuad::RENDER_PASS;
    quad.render_pass_id = id;
    return quad;
  }

  DrawQuad::Material material;
  // Set when material==DrawQuad::SURFACE_CONTENT.
  SurfaceId surface_id;
  // Set when material==DrawQuad::SOLID_COLOR.
  SkColor color;
  // Set when material==DrawQuad::RENDER_PASS.
  RenderPass::Id render_pass_id;

 private:
  Quad()
      : material(DrawQuad::INVALID),
        color(SK_ColorWHITE),
        render_pass_id(-1, -1) {}
};

struct Pass {
  Pass(Quad* quads, size_t quad_count, RenderPass::Id id)
      : quads(quads), quad_count(quad_count), id(id) {}
  Pass(Quad* quads, size_t quad_count)
      : quads(quads), quad_count(quad_count), id(1, 1) {}

  Quad* quads;
  size_t quad_count;
  RenderPass::Id id;
};

void AddSurfaceQuad(TestRenderPass* pass,
                    const gfx::Size& surface_size,
                    int surface_id);

void AddQuadInPass(TestRenderPass* pass, Quad desc);

void AddPasses(RenderPassList* pass_list,
               const gfx::Rect& output_rect,
               Pass* passes,
               size_t pass_count);

void TestQuadMatchesExpectations(Quad expected_quad, DrawQuad* quad);

void TestPassMatchesExpectations(Pass expected_pass, RenderPass* pass);

void TestPassesMatchExpectations(Pass* expected_passes,
                                 size_t expected_pass_count,
                                 RenderPassList* passes);

void SubmitFrame(Pass* passes, size_t pass_count, Surface* surface);

void QueuePassAsFrame(scoped_ptr<RenderPass> pass, Surface* surface);

}  // namespace test
}  // namespace cc

#endif  // CC_SURFACES_SURFACE_AGGREGATOR_TEST_HELPERS_H_
