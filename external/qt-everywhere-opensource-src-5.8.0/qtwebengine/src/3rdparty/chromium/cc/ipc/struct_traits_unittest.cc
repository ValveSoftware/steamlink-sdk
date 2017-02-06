// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "cc/input/selection.h"
#include "cc/ipc/traits_test_service.mojom.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/render_pass_id.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"

namespace cc {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoBeginFrameArgs(const BeginFrameArgs& b,
                          const EchoBeginFrameArgsCallback& callback) override {
    callback.Run(b);
  }

  void EchoCompositorFrame(
      CompositorFrame c,
      const EchoCompositorFrameCallback& callback) override {
    callback.Run(std::move(c));
  }

  void EchoCompositorFrameMetadata(
      const CompositorFrameMetadata& c,
      const EchoCompositorFrameMetadataCallback& callback) override {
    callback.Run(c);
  }

  void EchoFilterOperation(
      const FilterOperation& f,
      const EchoFilterOperationCallback& callback) override {
    callback.Run(f);
  }

  void EchoFilterOperations(
      const FilterOperations& f,
      const EchoFilterOperationsCallback& callback) override {
    callback.Run(f);
  }

  void EchoQuadList(const QuadList& q,
                    const EchoQuadListCallback& callback) override {
    callback.Run(q);
  }

  void EchoRenderPass(const std::unique_ptr<RenderPass>& r,
                      const EchoRenderPassCallback& callback) override {
    callback.Run(r);
  }

  void EchoRenderPassId(const RenderPassId& r,
                        const EchoRenderPassIdCallback& callback) override {
    callback.Run(r);
  }

  void EchoReturnedResource(
      const ReturnedResource& r,
      const EchoReturnedResourceCallback& callback) override {
    callback.Run(r);
  }

  void EchoSelection(const Selection<gfx::SelectionBound>& s,
                     const EchoSelectionCallback& callback) override {
    callback.Run(s);
  }

  void EchoSharedQuadState(
      const SharedQuadState& s,
      const EchoSharedQuadStateCallback& callback) override {
    callback.Run(s);
  }

  void EchoSurfaceId(const SurfaceId& s,
                     const EchoSurfaceIdCallback& callback) override {
    callback.Run(s);
  }

  void EchoSurfaceSequence(
      const SurfaceSequence& s,
      const EchoSurfaceSequenceCallback& callback) override {
    callback.Run(s);
  }

  void EchoTransferableResource(
      const TransferableResource& t,
      const EchoTransferableResourceCallback& callback) override {
    callback.Run(t);
  }

  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, BeginFrameArgs) {
  const base::TimeTicks frame_time = base::TimeTicks::Now();
  const base::TimeTicks deadline = base::TimeTicks::Now();
  const base::TimeDelta interval = base::TimeDelta::FromMilliseconds(1337);
  const BeginFrameArgs::BeginFrameArgsType type = BeginFrameArgs::NORMAL;
  const bool on_critical_path = true;
  BeginFrameArgs input;
  input.frame_time = frame_time;
  input.deadline = deadline;
  input.interval = interval;
  input.type = type;
  input.on_critical_path = on_critical_path;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  BeginFrameArgs output;
  proxy->EchoBeginFrameArgs(input, &output);
  EXPECT_EQ(frame_time, output.frame_time);
  EXPECT_EQ(deadline, output.deadline);
  EXPECT_EQ(interval, output.interval);
  EXPECT_EQ(type, output.type);
  EXPECT_EQ(on_critical_path, output.on_critical_path);
}

// Note that this is a fairly trivial test of CompositorFrame serialization as
// most of the heavy lifting has already been done by CompositorFrameMetadata,
// RenderPass, and QuadListBasic unit tests.
TEST_F(StructTraitsTest, CompositorFrame) {
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();

  // SharedQuadState.
  const gfx::Transform sqs_quad_to_target_transform(
      1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 14.f,
      15.f, 16.f);
  const gfx::Size sqs_layer_bounds(1234, 5678);
  const gfx::Rect sqs_visible_layer_rect(12, 34, 56, 78);
  const gfx::Rect sqs_clip_rect(123, 456, 789, 101112);
  const bool sqs_is_clipped = true;
  const float sqs_opacity = 0.9f;
  const SkXfermode::Mode sqs_blend_mode = SkXfermode::kSrcOver_Mode;
  const int sqs_sorting_context_id = 1337;
  SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(sqs_quad_to_target_transform, sqs_layer_bounds,
              sqs_visible_layer_rect, sqs_clip_rect, sqs_is_clipped,
              sqs_opacity, sqs_blend_mode, sqs_sorting_context_id);

  // DebugBorderDrawQuad.
  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor color1 = SK_ColorRED;
  const int32_t width1 = 1337;
  DebugBorderDrawQuad* debug_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_quad->SetNew(sqs, rect1, rect1, color1, width1);

  // SolidColorDrawQuad.
  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const uint32_t color2 = 0xffffffff;
  const bool force_anti_aliasing_off = true;
  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(sqs, rect2, rect2, color2, force_anti_aliasing_off);

  // TransferableResource constants.
  const uint32_t tr_id = 1337;
  const ResourceFormat tr_format = ALPHA_8;
  const uint32_t tr_filter = 1234;
  const gfx::Size tr_size(1234, 5678);
  TransferableResource resource;
  resource.id = tr_id;
  resource.format = tr_format;
  resource.filter = tr_filter;
  resource.size = tr_size;

  // CompositorFrameMetadata constants.
  const float device_scale_factor = 2.6f;
  const gfx::Vector2dF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);

  CompositorFrame input;
  input.metadata.device_scale_factor = device_scale_factor;
  input.metadata.root_scroll_offset = root_scroll_offset;
  input.metadata.page_scale_factor = page_scale_factor;
  input.metadata.scrollable_viewport_size = scrollable_viewport_size;
  input.delegated_frame_data.reset(new DelegatedFrameData);
  input.delegated_frame_data->render_pass_list.push_back(
      std::move(render_pass));
  input.delegated_frame_data->resource_list.push_back(resource);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  CompositorFrame output;
  proxy->EchoCompositorFrame(std::move(input), &output);

  EXPECT_EQ(device_scale_factor, output.metadata.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.metadata.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.metadata.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.metadata.scrollable_viewport_size);

  EXPECT_NE(nullptr, output.delegated_frame_data);
  ASSERT_EQ(1u, output.delegated_frame_data->resource_list.size());
  TransferableResource out_resource =
      output.delegated_frame_data->resource_list[0];
  EXPECT_EQ(tr_id, out_resource.id);
  EXPECT_EQ(tr_format, out_resource.format);
  EXPECT_EQ(tr_filter, out_resource.filter);
  EXPECT_EQ(tr_size, out_resource.size);

  EXPECT_EQ(1u, output.delegated_frame_data->render_pass_list.size());
  const RenderPass* out_render_pass =
      output.delegated_frame_data->render_pass_list[0].get();
  ASSERT_EQ(2u, out_render_pass->quad_list.size());
  ASSERT_EQ(1u, out_render_pass->shared_quad_state_list.size());

  const SharedQuadState* out_sqs =
      out_render_pass->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(sqs_quad_to_target_transform, out_sqs->quad_to_target_transform);
  EXPECT_EQ(sqs_layer_bounds, out_sqs->quad_layer_bounds);
  EXPECT_EQ(sqs_visible_layer_rect, out_sqs->visible_quad_layer_rect);
  EXPECT_EQ(sqs_clip_rect, out_sqs->clip_rect);
  EXPECT_EQ(sqs_is_clipped, out_sqs->is_clipped);
  EXPECT_EQ(sqs_opacity, out_sqs->opacity);
  EXPECT_EQ(sqs_blend_mode, out_sqs->blend_mode);
  EXPECT_EQ(sqs_sorting_context_id, out_sqs->sorting_context_id);

  const DebugBorderDrawQuad* out_debug_border_draw_quad =
      DebugBorderDrawQuad::MaterialCast(
          out_render_pass->quad_list.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const SolidColorDrawQuad* out_solid_color_draw_quad =
      SolidColorDrawQuad::MaterialCast(out_render_pass->quad_list.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);
}

TEST_F(StructTraitsTest, CompositorFrameMetadata) {
  const float device_scale_factor = 2.6f;
  const gfx::Vector2dF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const gfx::SizeF root_layer_size(1234.5f, 5432.1f);
  const float min_page_scale_factor = 3.5f;
  const float max_page_scale_factor = 4.6f;
  const bool root_overflow_x_hidden = true;
  const bool root_overflow_y_hidden = true;
  const gfx::Vector2dF location_bar_offset(1234.5f, 5432.1f);
  const gfx::Vector2dF location_bar_content_translation(1234.5f, 5432.1f);
  const uint32_t root_background_color = 1337;
  Selection<gfx::SelectionBound> selection;
  selection.start.SetEdge(gfx::PointF(1234.5f, 67891.f),
                          gfx::PointF(5432.1f, 1987.6f));
  selection.start.set_visible(true);
  selection.start.set_type(gfx::SelectionBound::CENTER);
  selection.end.SetEdge(gfx::PointF(1337.5f, 52124.f),
                        gfx::PointF(1234.3f, 8765.6f));
  selection.end.set_visible(false);
  selection.end.set_type(gfx::SelectionBound::RIGHT);
  selection.is_editable = true;
  selection.is_empty_text_form_control = true;
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337, 7331);
  std::vector<ui::LatencyInfo> latency_infos = {latency_info};
  std::vector<uint32_t> satisfies_sequences = {1234, 1337};
  std::vector<SurfaceId> referenced_surfaces;
  SurfaceId id(1234, 5678, 9101112);
  referenced_surfaces.push_back(id);

  CompositorFrameMetadata input;
  input.device_scale_factor = device_scale_factor;
  input.root_scroll_offset = root_scroll_offset;
  input.page_scale_factor = page_scale_factor;
  input.scrollable_viewport_size = scrollable_viewport_size;
  input.root_layer_size = root_layer_size;
  input.min_page_scale_factor = min_page_scale_factor;
  input.max_page_scale_factor = max_page_scale_factor;
  input.root_overflow_x_hidden = root_overflow_x_hidden;
  input.root_overflow_y_hidden = root_overflow_y_hidden;
  input.location_bar_offset = location_bar_offset;
  input.location_bar_content_translation = location_bar_content_translation;
  input.root_background_color = root_background_color;
  input.selection = selection;
  input.latency_info = latency_infos;
  input.satisfies_sequences = satisfies_sequences;
  input.referenced_surfaces = referenced_surfaces;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  CompositorFrameMetadata output;
  proxy->EchoCompositorFrameMetadata(input, &output);
  EXPECT_EQ(device_scale_factor, output.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.scrollable_viewport_size);
  EXPECT_EQ(root_layer_size, output.root_layer_size);
  EXPECT_EQ(min_page_scale_factor, output.min_page_scale_factor);
  EXPECT_EQ(max_page_scale_factor, output.max_page_scale_factor);
  EXPECT_EQ(root_overflow_x_hidden, output.root_overflow_x_hidden);
  EXPECT_EQ(root_overflow_y_hidden, output.root_overflow_y_hidden);
  EXPECT_EQ(location_bar_offset, output.location_bar_offset);
  EXPECT_EQ(location_bar_content_translation,
            output.location_bar_content_translation);
  EXPECT_EQ(root_background_color, output.root_background_color);
  EXPECT_EQ(selection, output.selection);
  EXPECT_EQ(latency_infos.size(), output.latency_info.size());
  ui::LatencyInfo::LatencyComponent component;
  EXPECT_TRUE(output.latency_info[0].FindLatency(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337,
      &component));
  EXPECT_EQ(7331, component.sequence_number);
  EXPECT_EQ(satisfies_sequences.size(), output.satisfies_sequences.size());
  for (uint32_t i = 0; i < satisfies_sequences.size(); ++i)
    EXPECT_EQ(satisfies_sequences[i], output.satisfies_sequences[i]);
  EXPECT_EQ(referenced_surfaces.size(), output.referenced_surfaces.size());
  for (uint32_t i = 0; i < referenced_surfaces.size(); ++i)
    EXPECT_EQ(referenced_surfaces[i], output.referenced_surfaces[i]);
}

TEST_F(StructTraitsTest, FilterOperation) {
  const FilterOperation inputs[] = {
      FilterOperation::CreateBlurFilter(20),
      FilterOperation::CreateDropShadowFilter(gfx::Point(4, 4), 4.0f,
                                              SkColorSetARGB(255, 40, 0, 0)),
      FilterOperation::CreateReferenceFilter(SkDropShadowImageFilter::Make(
          SkIntToScalar(3), SkIntToScalar(8), SkIntToScalar(4),
          SkIntToScalar(9), SK_ColorBLACK,
          SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode,
          nullptr))};
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (const auto& input : inputs) {
    FilterOperation output;
    proxy->EchoFilterOperation(input, &output);
    EXPECT_EQ(input.type(), output.type());
    switch (input.type()) {
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::OPACITY:
      case FilterOperation::BLUR:
        EXPECT_EQ(input.amount(), output.amount());
        break;
      case FilterOperation::DROP_SHADOW:
        EXPECT_EQ(input.amount(), output.amount());
        EXPECT_EQ(input.drop_shadow_offset(), output.drop_shadow_offset());
        EXPECT_EQ(input.drop_shadow_color(), output.drop_shadow_color());
        break;
      case FilterOperation::COLOR_MATRIX:
        EXPECT_EQ(0, memcmp(input.matrix(), output.matrix(), 20));
        break;
      case FilterOperation::ZOOM:
        EXPECT_EQ(input.amount(), output.amount());
        EXPECT_EQ(input.zoom_inset(), output.zoom_inset());
        break;
      case FilterOperation::REFERENCE: {
        SkString input_str;
        input.image_filter()->toString(&input_str);
        SkString output_str;
        output.image_filter()->toString(&output_str);
        EXPECT_EQ(input_str, output_str);
        break;
      }
      case FilterOperation::ALPHA_THRESHOLD:
        NOTREACHED();
        break;
    }
  }
}

TEST_F(StructTraitsTest, FilterOperations) {
  FilterOperations input;
  input.Append(FilterOperation::CreateBlurFilter(0.f));
  input.Append(FilterOperation::CreateSaturateFilter(4.f));
  input.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  FilterOperations output;
  proxy->EchoFilterOperations(input, &output);
  EXPECT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input.at(i), output.at(i));
  }
}

TEST_F(StructTraitsTest, QuadListBasic) {
  SharedQuadState sqs;
  QuadList input;

  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor color1 = SK_ColorRED;
  const int32_t width1 = 1337;
  DebugBorderDrawQuad* debug_quad =
      input.AllocateAndConstruct<DebugBorderDrawQuad>();
  debug_quad->SetNew(&sqs, rect1, rect1, color1, width1);

  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const uint32_t color2 = 0xffffffff;
  const bool force_anti_aliasing_off = true;
  SolidColorDrawQuad* solid_quad =
      input.AllocateAndConstruct<SolidColorDrawQuad>();
  solid_quad->SetNew(&sqs, rect2, rect2, color2, force_anti_aliasing_off);

  const gfx::Rect rect3(1029, 3847, 5610, 2938);
  const SurfaceId surface_id(1234, 5678, 2468);
  SurfaceDrawQuad* surface_quad = input.AllocateAndConstruct<SurfaceDrawQuad>();
  surface_quad->SetNew(&sqs, rect3, rect3, surface_id);

  const gfx::Rect rect4(1234, 5678, 9101112, 13141516);
  const ResourceId resource_id4(1337);
  const RenderPassId render_pass_id(1234, 5678);
  const gfx::Vector2dF mask_uv_scale(1337.1f, 1234.2f);
  const gfx::Size mask_texture_size(1234, 5678);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(0.f));
  filters.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  gfx::Vector2dF filters_scale(1234.1f, 4321.2f);
  FilterOperations background_filters;
  background_filters.Append(FilterOperation::CreateSaturateFilter(4.f));
  background_filters.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  background_filters.Append(FilterOperation::CreateSaturateFilter(2.f));

  RenderPassDrawQuad* render_pass_quad =
      input.AllocateAndConstruct<RenderPassDrawQuad>();
  render_pass_quad->SetNew(&sqs, rect4, rect4, render_pass_id, resource_id4,
                           mask_uv_scale, mask_texture_size, filters,
                           filters_scale, background_filters);

  const gfx::Rect rect5(123, 567, 91011, 131415);
  const ResourceId resource_id5(1337);
  const float vertex_opacity[4] = {1.f, 2.f, 3.f, 4.f};
  const bool premultiplied_alpha = true;
  const gfx::PointF uv_top_left(12.1f, 34.2f);
  const gfx::PointF uv_bottom_right(56.3f, 78.4f);
  const SkColor background_color = SK_ColorGREEN;
  const bool y_flipped = true;
  const bool nearest_neighbor = true;
  const bool secure_output_only = true;
  TextureDrawQuad* texture_draw_quad =
      input.AllocateAndConstruct<TextureDrawQuad>();
  texture_draw_quad->SetNew(&sqs, rect5, rect5, rect5, resource_id5,
                            premultiplied_alpha, uv_top_left, uv_bottom_right,
                            background_color, vertex_opacity, y_flipped,
                            nearest_neighbor, secure_output_only);

  const gfx::Rect rect6(321, 765, 11109, 151413);
  const ResourceId resource_id6(1234);
  const gfx::Size resource_size_in_pixels(1234, 5678);
  const gfx::Transform matrix(16.1f, 15.3f, 14.3f, 13.7f, 12.2f, 11.4f, 10.4f,
                              9.8f, 8.1f, 7.3f, 6.3f, 5.7f, 4.8f, 3.4f, 2.4f,
                              1.2f);
  StreamVideoDrawQuad* stream_video_draw_quad =
      input.AllocateAndConstruct<StreamVideoDrawQuad>();
  stream_video_draw_quad->SetNew(&sqs, rect6, rect6, rect6, resource_id6,
                                 resource_size_in_pixels, matrix);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  QuadList output;
  proxy->EchoQuadList(input, &output);

  ASSERT_EQ(input.size(), output.size());

  const DebugBorderDrawQuad* out_debug_border_draw_quad =
      DebugBorderDrawQuad::MaterialCast(output.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const SolidColorDrawQuad* out_solid_color_draw_quad =
      SolidColorDrawQuad::MaterialCast(output.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);

  const SurfaceDrawQuad* out_surface_draw_quad =
      SurfaceDrawQuad::MaterialCast(output.ElementAt(2));
  EXPECT_EQ(rect3, out_surface_draw_quad->rect);
  EXPECT_EQ(rect3, out_surface_draw_quad->visible_rect);
  EXPECT_EQ(surface_id, out_surface_draw_quad->surface_id);

  const RenderPassDrawQuad* out_render_pass_draw_quad =
      RenderPassDrawQuad::MaterialCast(output.ElementAt(3));
  EXPECT_EQ(rect4, out_render_pass_draw_quad->rect);
  EXPECT_EQ(rect4, out_render_pass_draw_quad->visible_rect);
  EXPECT_EQ(render_pass_id, out_render_pass_draw_quad->render_pass_id);
  EXPECT_EQ(resource_id4, out_render_pass_draw_quad->mask_resource_id());
  EXPECT_EQ(mask_texture_size, out_render_pass_draw_quad->mask_texture_size);
  EXPECT_EQ(filters.size(), out_render_pass_draw_quad->filters.size());
  for (size_t i = 0; i < filters.size(); ++i)
    EXPECT_EQ(filters.at(i), out_render_pass_draw_quad->filters.at(i));
  EXPECT_EQ(filters_scale, out_render_pass_draw_quad->filters_scale);
  EXPECT_EQ(background_filters.size(),
            out_render_pass_draw_quad->background_filters.size());
  for (size_t i = 0; i < background_filters.size(); ++i)
    EXPECT_EQ(background_filters.at(i),
              out_render_pass_draw_quad->background_filters.at(i));

  const TextureDrawQuad* out_texture_draw_quad =
      TextureDrawQuad::MaterialCast(output.ElementAt(4));
  EXPECT_EQ(rect5, out_texture_draw_quad->rect);
  EXPECT_EQ(rect5, out_texture_draw_quad->opaque_rect);
  EXPECT_EQ(rect5, out_texture_draw_quad->visible_rect);
  EXPECT_EQ(resource_id5, out_texture_draw_quad->resource_id());
  EXPECT_EQ(premultiplied_alpha, out_texture_draw_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, out_texture_draw_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, out_texture_draw_quad->uv_bottom_right);
  EXPECT_EQ(background_color, out_texture_draw_quad->background_color);
  EXPECT_EQ(vertex_opacity[0], out_texture_draw_quad->vertex_opacity[0]);
  EXPECT_EQ(vertex_opacity[1], out_texture_draw_quad->vertex_opacity[1]);
  EXPECT_EQ(vertex_opacity[2], out_texture_draw_quad->vertex_opacity[2]);
  EXPECT_EQ(vertex_opacity[3], out_texture_draw_quad->vertex_opacity[3]);
  EXPECT_EQ(y_flipped, out_texture_draw_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, out_texture_draw_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, out_texture_draw_quad->secure_output_only);

  const StreamVideoDrawQuad* out_stream_video_draw_quad =
      StreamVideoDrawQuad::MaterialCast(output.ElementAt(5));
  EXPECT_EQ(rect6, out_stream_video_draw_quad->rect);
  EXPECT_EQ(rect6, out_stream_video_draw_quad->opaque_rect);
  EXPECT_EQ(rect6, out_stream_video_draw_quad->visible_rect);
  EXPECT_EQ(resource_id6, out_stream_video_draw_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels,
            out_stream_video_draw_quad->resource_size_in_pixels());
  EXPECT_EQ(matrix, out_stream_video_draw_quad->matrix);
}

TEST_F(StructTraitsTest, RenderPass) {
  const RenderPassId id(3, 2);
  const gfx::Rect output_rect(45, 22, 120, 13);
  const gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  const gfx::Rect damage_rect(56, 123, 19, 43);
  const bool has_transparent_background = true;
  std::unique_ptr<RenderPass> input = RenderPass::Create();
  input->SetAll(id, output_rect, damage_rect, transform_to_root,
                has_transparent_background);

  SharedQuadState* shared_state_1 = input->CreateAndAppendSharedQuadState();
  shared_state_1->SetAll(
      gfx::Transform(16.1f, 15.3f, 14.3f, 13.7f, 12.2f, 11.4f, 10.4f, 9.8f,
                     8.1f, 7.3f, 6.3f, 5.7f, 4.8f, 3.4f, 2.4f, 1.2f),
      gfx::Size(1, 2), gfx::Rect(1337, 5679, 9101112, 131415),
      gfx::Rect(1357, 2468, 121314, 1337), true, 2, SkXfermode::kSrcOver_Mode,
      1);

  SharedQuadState* shared_state_2 = input->CreateAndAppendSharedQuadState();
  shared_state_2->SetAll(
      gfx::Transform(1.1f, 2.3f, 3.3f, 4.7f, 5.2f, 6.4f, 7.4f, 8.8f, 9.1f,
                     10.3f, 11.3f, 12.7f, 13.8f, 14.4f, 15.4f, 16.2f),
      gfx::Size(1337, 1234), gfx::Rect(1234, 5678, 9101112, 13141516),
      gfx::Rect(1357, 2468, 121314, 1337), true, 2, SkXfermode::kSrcOver_Mode,
      1);

  // This quad uses the first shared quad state. The next two quads use the
  // second shared quad state.
  DebugBorderDrawQuad* debug_quad =
      input->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  const gfx::Rect debug_quad_rect(12, 56, 89, 10);
  debug_quad->SetNew(shared_state_1, debug_quad_rect, debug_quad_rect,
                     SK_ColorBLUE, 1337);

  SolidColorDrawQuad* color_quad =
      input->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  const gfx::Rect color_quad_rect(123, 456, 789, 101);
  color_quad->SetNew(shared_state_2, color_quad_rect, color_quad_rect,
                     SK_ColorRED, true);

  SurfaceDrawQuad* surface_quad =
      input->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  const gfx::Rect surface_quad_rect(1337, 2448, 1234, 5678);
  surface_quad->SetNew(shared_state_2, surface_quad_rect, surface_quad_rect,
                       SurfaceId(1337, 1234, 2468));

  std::unique_ptr<RenderPass> output;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoRenderPass(input, &output);

  EXPECT_EQ(input->quad_list.size(), output->quad_list.size());
  EXPECT_EQ(input->shared_quad_state_list.size(),
            output->shared_quad_state_list.size());
  EXPECT_EQ(id, output->id);
  EXPECT_EQ(output_rect, output->output_rect);
  EXPECT_EQ(damage_rect, output->damage_rect);
  EXPECT_EQ(transform_to_root, output->transform_to_root_target);
  EXPECT_EQ(has_transparent_background, output->has_transparent_background);

  SharedQuadState* out_sqs1 = output->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(shared_state_1->quad_to_target_transform,
            out_sqs1->quad_to_target_transform);
  EXPECT_EQ(shared_state_1->quad_layer_bounds, out_sqs1->quad_layer_bounds);
  EXPECT_EQ(shared_state_1->visible_quad_layer_rect,
            out_sqs1->visible_quad_layer_rect);
  EXPECT_EQ(shared_state_1->clip_rect, out_sqs1->clip_rect);
  EXPECT_EQ(shared_state_1->is_clipped, out_sqs1->is_clipped);
  EXPECT_EQ(shared_state_1->opacity, out_sqs1->opacity);
  EXPECT_EQ(shared_state_1->blend_mode, out_sqs1->blend_mode);
  EXPECT_EQ(shared_state_1->sorting_context_id, out_sqs1->sorting_context_id);

  SharedQuadState* out_sqs2 = output->shared_quad_state_list.ElementAt(1);
  EXPECT_EQ(shared_state_2->quad_to_target_transform,
            out_sqs2->quad_to_target_transform);
  EXPECT_EQ(shared_state_2->quad_layer_bounds, out_sqs2->quad_layer_bounds);
  EXPECT_EQ(shared_state_2->visible_quad_layer_rect,
            out_sqs2->visible_quad_layer_rect);
  EXPECT_EQ(shared_state_2->clip_rect, out_sqs2->clip_rect);
  EXPECT_EQ(shared_state_2->is_clipped, out_sqs2->is_clipped);
  EXPECT_EQ(shared_state_2->opacity, out_sqs2->opacity);
  EXPECT_EQ(shared_state_2->blend_mode, out_sqs2->blend_mode);
  EXPECT_EQ(shared_state_2->sorting_context_id, out_sqs2->sorting_context_id);

  const DebugBorderDrawQuad* out_debug_quad =
      DebugBorderDrawQuad::MaterialCast(output->quad_list.ElementAt(0));
  EXPECT_EQ(out_debug_quad->shared_quad_state, out_sqs1);
  EXPECT_EQ(debug_quad->rect, out_debug_quad->rect);
  EXPECT_EQ(debug_quad->visible_rect, out_debug_quad->visible_rect);
  EXPECT_EQ(debug_quad->color, out_debug_quad->color);
  EXPECT_EQ(debug_quad->width, out_debug_quad->width);

  const SolidColorDrawQuad* out_color_quad =
      SolidColorDrawQuad::MaterialCast(output->quad_list.ElementAt(1));
  EXPECT_EQ(out_color_quad->shared_quad_state, out_sqs2);
  EXPECT_EQ(color_quad->rect, out_color_quad->rect);
  EXPECT_EQ(color_quad->visible_rect, out_color_quad->visible_rect);
  EXPECT_EQ(color_quad->color, out_color_quad->color);
  EXPECT_EQ(color_quad->force_anti_aliasing_off,
            out_color_quad->force_anti_aliasing_off);

  const SurfaceDrawQuad* out_surface_quad =
      SurfaceDrawQuad::MaterialCast(output->quad_list.ElementAt(2));
  EXPECT_EQ(out_surface_quad->shared_quad_state, out_sqs2);
  EXPECT_EQ(surface_quad->rect, out_surface_quad->rect);
  EXPECT_EQ(surface_quad->visible_rect, out_surface_quad->visible_rect);
  EXPECT_EQ(surface_quad->surface_id, out_surface_quad->surface_id);
}

TEST_F(StructTraitsTest, RenderPassId) {
  const int layer_id = 1337;
  const uint32_t index = 0xdeadbeef;
  RenderPassId input(layer_id, index);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  RenderPassId output;
  proxy->EchoRenderPassId(input, &output);
  EXPECT_EQ(layer_id, output.layer_id);
  EXPECT_EQ(index, output.index);
}

TEST_F(StructTraitsTest, ReturnedResource) {
  const uint32_t id = 1337;
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  const gpu::SyncToken sync_token(command_buffer_namespace, extra_data_field,
                                  command_buffer_id, release_count);
  const int count = 1234;
  const bool lost = true;

  ReturnedResource input;
  input.id = id;
  input.sync_token = sync_token;
  input.count = count;
  input.lost = lost;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  ReturnedResource output;
  proxy->EchoReturnedResource(input, &output);
  EXPECT_EQ(id, output.id);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(count, output.count);
  EXPECT_EQ(lost, output.lost);
}

TEST_F(StructTraitsTest, Selection) {
  gfx::SelectionBound start;
  start.SetEdge(gfx::PointF(1234.5f, 67891.f), gfx::PointF(5432.1f, 1987.6f));
  start.set_visible(true);
  start.set_type(gfx::SelectionBound::CENTER);
  gfx::SelectionBound end;
  end.SetEdge(gfx::PointF(1337.5f, 52124.f), gfx::PointF(1234.3f, 8765.6f));
  end.set_visible(false);
  end.set_type(gfx::SelectionBound::RIGHT);
  const bool is_editable = true;
  const bool is_empty_text_form_control = true;
  Selection<gfx::SelectionBound> input;
  input.start = start;
  input.end = end;
  input.is_editable = is_editable;
  input.is_empty_text_form_control = is_empty_text_form_control;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  Selection<gfx::SelectionBound> output;
  proxy->EchoSelection(input, &output);
  EXPECT_EQ(start, output.start);
  EXPECT_EQ(end, output.end);
  EXPECT_EQ(is_editable, output.is_editable);
  EXPECT_EQ(is_empty_text_form_control, output.is_empty_text_form_control);
}

TEST_F(StructTraitsTest, SurfaceId) {
  const uint32_t id_namespace = 1337;
  const uint32_t local_id = 0xfbadbeef;
  const uint64_t nonce = 0xdeadbeef;
  SurfaceId input(id_namespace, local_id, nonce);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceId output;
  proxy->EchoSurfaceId(input, &output);
  EXPECT_EQ(id_namespace, output.id_namespace());
  EXPECT_EQ(local_id, output.local_id());
  EXPECT_EQ(nonce, output.nonce());
}

TEST_F(StructTraitsTest, SurfaceSequence) {
  const uint32_t id_namespace = 2016;
  const uint32_t sequence = 0xfbadbeef;
  SurfaceSequence input(id_namespace, sequence);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceSequence output;
  proxy->EchoSurfaceSequence(input, &output);
  EXPECT_EQ(id_namespace, output.id_namespace);
  EXPECT_EQ(sequence, output.sequence);
}

TEST_F(StructTraitsTest, SharedQuadState) {
  const gfx::Transform quad_to_target_transform(1.f, 2.f, 3.f, 4.f, 5.f, 6.f,
                                                7.f, 8.f, 9.f, 10.f, 11.f, 12.f,
                                                13.f, 14.f, 15.f, 16.f);
  const gfx::Size layer_bounds(1234, 5678);
  const gfx::Rect visible_layer_rect(12, 34, 56, 78);
  const gfx::Rect clip_rect(123, 456, 789, 101112);
  const bool is_clipped = true;
  const float opacity = 0.9f;
  const SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;
  const int sorting_context_id = 1337;
  SharedQuadState input_sqs;
  input_sqs.SetAll(quad_to_target_transform, layer_bounds, visible_layer_rect,
                   clip_rect, is_clipped, opacity, blend_mode,
                   sorting_context_id);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SharedQuadState output_sqs;
  proxy->EchoSharedQuadState(input_sqs, &output_sqs);
  EXPECT_EQ(quad_to_target_transform, output_sqs.quad_to_target_transform);
  EXPECT_EQ(layer_bounds, output_sqs.quad_layer_bounds);
  EXPECT_EQ(visible_layer_rect, output_sqs.visible_quad_layer_rect);
  EXPECT_EQ(clip_rect, output_sqs.clip_rect);
  EXPECT_EQ(is_clipped, output_sqs.is_clipped);
  EXPECT_EQ(opacity, output_sqs.opacity);
  EXPECT_EQ(blend_mode, output_sqs.blend_mode);
  EXPECT_EQ(sorting_context_id, output_sqs.sorting_context_id);
}

TEST_F(StructTraitsTest, TransferableResource) {
  const uint32_t id = 1337;
  const ResourceFormat format = ALPHA_8;
  const uint32_t filter = 1234;
  const gfx::Size size(1234, 5678);
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  const uint32_t texture_target = 1337;
  const bool read_lock_fences_enabled = true;
  const bool is_software = false;
  const bool is_overlay_candidate = true;

  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.mailbox.SetName(mailbox_name);
  mailbox_holder.sync_token =
      gpu::SyncToken(command_buffer_namespace, extra_data_field,
                     command_buffer_id, release_count);
  mailbox_holder.texture_target = texture_target;
  TransferableResource input;
  input.id = id;
  input.format = format;
  input.filter = filter;
  input.size = size;
  input.mailbox_holder = mailbox_holder;
  input.read_lock_fences_enabled = read_lock_fences_enabled;
  input.is_software = is_software;
  input.is_overlay_candidate = is_overlay_candidate;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  TransferableResource output;
  proxy->EchoTransferableResource(input, &output);
  EXPECT_EQ(id, output.id);
  EXPECT_EQ(format, output.format);
  EXPECT_EQ(filter, output.filter);
  EXPECT_EQ(size, output.size);
  EXPECT_EQ(mailbox_holder.mailbox, output.mailbox_holder.mailbox);
  EXPECT_EQ(mailbox_holder.sync_token, output.mailbox_holder.sync_token);
  EXPECT_EQ(mailbox_holder.texture_target,
            output.mailbox_holder.texture_target);
  EXPECT_EQ(read_lock_fences_enabled, output.read_lock_fences_enabled);
  EXPECT_EQ(is_software, output.is_software);
  EXPECT_EQ(is_overlay_candidate, output.is_overlay_candidate);
}

TEST_F(StructTraitsTest, YUVDrawQuad) {
  const DrawQuad::Material material = DrawQuad::YUV_VIDEO_CONTENT;
  const gfx::Rect rect(1234, 4321, 1357, 7531);
  const gfx::Rect opaque_rect(1357, 8642, 432, 123);
  const gfx::Rect visible_rect(1337, 7331, 561, 293);
  const bool needs_blending = true;
  const gfx::RectF ya_tex_coord_rect(1234.1f, 5678.2f, 9101112.3f, 13141516.4f);
  const gfx::RectF uv_tex_coord_rect(1234.1f, 4321.2f, 1357.3f, 7531.4f);
  const gfx::Size ya_tex_size(1234, 5678);
  const gfx::Size uv_tex_size(4321, 8765);
  const uint32_t y_plane_resource_id = 1337;
  const uint32_t u_plane_resource_id = 1234;
  const uint32_t v_plane_resource_id = 2468;
  const uint32_t a_plane_resource_id = 7890;
  const YUVVideoDrawQuad::ColorSpace color_space = YUVVideoDrawQuad::JPEG;
  const float resource_offset = 1337.5f;
  const float resource_multiplier = 1234.6f;

  SharedQuadState sqs;
  QuadList input;
  YUVVideoDrawQuad* quad = input.AllocateAndConstruct<YUVVideoDrawQuad>();
  quad->SetAll(&sqs, rect, opaque_rect, visible_rect, needs_blending,
               ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size, uv_tex_size,
               y_plane_resource_id, u_plane_resource_id, v_plane_resource_id,
               a_plane_resource_id, color_space, resource_offset,
               resource_multiplier);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  QuadList output;
  proxy->EchoQuadList(input, &output);

  ASSERT_EQ(input.size(), output.size());

  ASSERT_EQ(material, output.ElementAt(0)->material);
  const YUVVideoDrawQuad* out_quad =
      YUVVideoDrawQuad::MaterialCast(output.ElementAt(0));
  EXPECT_EQ(rect, out_quad->rect);
  EXPECT_EQ(opaque_rect, out_quad->opaque_rect);
  EXPECT_EQ(visible_rect, out_quad->visible_rect);
  EXPECT_EQ(needs_blending, out_quad->needs_blending);
  EXPECT_EQ(ya_tex_coord_rect, out_quad->ya_tex_coord_rect);
  EXPECT_EQ(uv_tex_coord_rect, out_quad->uv_tex_coord_rect);
  EXPECT_EQ(ya_tex_size, out_quad->ya_tex_size);
  EXPECT_EQ(uv_tex_size, out_quad->uv_tex_size);
  EXPECT_EQ(y_plane_resource_id, out_quad->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, out_quad->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, out_quad->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, out_quad->a_plane_resource_id());
  EXPECT_EQ(color_space, out_quad->color_space);
  EXPECT_EQ(resource_offset, out_quad->resource_offset);
  EXPECT_EQ(resource_multiplier, out_quad->resource_multiplier);
}

}  // namespace cc
