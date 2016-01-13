// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "cc/layers/append_quads_data.h"
#include "cc/output/gl_renderer.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/resources/video_resource_updater.h"
#include "cc/test/fake_picture_pile_impl.h"
#include "cc/test/pixel_test.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/rect_conversions.h"

using gpu::gles2::GLES2Interface;

namespace cc {
namespace {

#if !defined(OS_ANDROID)
scoped_ptr<RenderPass> CreateTestRootRenderPass(RenderPass::Id id,
                                                const gfx::Rect& rect) {
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::Rect damage_rect = rect;
  const gfx::Transform transform_to_root_target;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass.Pass();
}

scoped_ptr<RenderPass> CreateTestRenderPass(
    RenderPass::Id id,
    const gfx::Rect& rect,
    const gfx::Transform& transform_to_root_target) {
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::Rect damage_rect = rect;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass.Pass();
}

SharedQuadState* CreateTestSharedQuadState(
    RenderPass* render_pass,
    gfx::Transform content_to_target_transform,
    const gfx::Rect& rect) {
  const gfx::Size content_bounds = rect.size();
  const gfx::Rect visible_content_rect = rect;
  const gfx::Rect clip_rect = rect;
  const bool is_clipped = false;
  const float opacity = 1.0f;
  const SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(content_to_target_transform,
                       content_bounds,
                       visible_content_rect,
                       clip_rect,
                       is_clipped,
                       opacity,
                       blend_mode,
                       sorting_context_id);
  return shared_state;
}

SharedQuadState* CreateTestSharedQuadStateClipped(
    RenderPass* render_pass,
    gfx::Transform content_to_target_transform,
    const gfx::Rect& rect,
    const gfx::Rect& clip_rect) {
  const gfx::Size content_bounds = rect.size();
  const gfx::Rect visible_content_rect = clip_rect;
  const bool is_clipped = true;
  const float opacity = 1.0f;
  const SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(content_to_target_transform,
                       content_bounds,
                       visible_content_rect,
                       clip_rect,
                       is_clipped,
                       opacity,
                       blend_mode,
                       sorting_context_id);
  return shared_state;
}

scoped_ptr<DrawQuad> CreateTestRenderPassDrawQuad(
    SharedQuadState* shared_state,
    const gfx::Rect& rect,
    RenderPass::Id pass_id) {
  scoped_ptr<RenderPassDrawQuad> quad = RenderPassDrawQuad::Create();
  quad->SetNew(shared_state,
               rect,
               rect,
               pass_id,
               false,                 // is_replica
               0,                     // mask_resource_id
               rect,                  // contents_changed_since_last_frame
               gfx::RectF(1.f, 1.f),  // mask_uv_rect
               FilterOperations(),    // foreground filters
               FilterOperations());   // background filters

  return quad.PassAs<DrawQuad>();
}

scoped_ptr<TextureDrawQuad> CreateTestTextureDrawQuad(
    const gfx::Rect& rect,
    SkColor texel_color,
    SkColor background_color,
    bool premultiplied_alpha,
    SharedQuadState* shared_state,
    ResourceProvider* resource_provider) {
  SkPMColor pixel_color = premultiplied_alpha ?
      SkPreMultiplyColor(texel_color) :
      SkPackARGB32NoCheck(SkColorGetA(texel_color),
                          SkColorGetR(texel_color),
                          SkColorGetG(texel_color),
                          SkColorGetB(texel_color));
  std::vector<uint32_t> pixels(rect.size().GetArea(), pixel_color);

  ResourceProvider::ResourceId resource =
      resource_provider->CreateResource(rect.size(),
                                        GL_CLAMP_TO_EDGE,
                                        ResourceProvider::TextureUsageAny,
                                        RGBA_8888);
  resource_provider->SetPixels(
      resource,
      reinterpret_cast<uint8_t*>(&pixels.front()),
      rect,
      rect,
      gfx::Vector2d());

  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};

  scoped_ptr<TextureDrawQuad> quad = TextureDrawQuad::Create();
  quad->SetNew(shared_state,
               rect,
               gfx::Rect(),
               rect,
               resource,
               premultiplied_alpha,
               gfx::PointF(0.0f, 0.0f),  // uv_top_left
               gfx::PointF(1.0f, 1.0f),  // uv_bottom_right
               background_color,
               vertex_opacity,
               false);  // flipped
  return quad.Pass();
}

typedef ::testing::Types<GLRenderer,
                         SoftwareRenderer,
                         GLRendererWithExpandedViewport,
                         SoftwareRendererWithExpandedViewport> RendererTypes;
TYPED_TEST_CASE(RendererPixelTest, RendererTypes);

// All pixels can be off by one, but any more than that is an error.
class FuzzyPixelOffByOneComparator : public FuzzyPixelComparator {
 public:
  explicit FuzzyPixelOffByOneComparator(bool discard_alpha)
    : FuzzyPixelComparator(discard_alpha, 100.f, 0.f, 1.f, 1, 0) {}
};

template <typename RendererType>
class FuzzyForSoftwareOnlyPixelComparator : public PixelComparator {
 public:
  explicit FuzzyForSoftwareOnlyPixelComparator(bool discard_alpha)
      : fuzzy_(discard_alpha), exact_(discard_alpha) {}

  virtual bool Compare(const SkBitmap& actual_bmp,
                       const SkBitmap& expected_bmp) const;

 private:
  FuzzyPixelOffByOneComparator fuzzy_;
  ExactPixelComparator exact_;
};

template<>
bool FuzzyForSoftwareOnlyPixelComparator<SoftwareRenderer>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return fuzzy_.Compare(actual_bmp, expected_bmp);
}

template <>
bool FuzzyForSoftwareOnlyPixelComparator<
    SoftwareRendererWithExpandedViewport>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return fuzzy_.Compare(actual_bmp, expected_bmp);
}

template<typename RendererType>
bool FuzzyForSoftwareOnlyPixelComparator<RendererType>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return exact_.Compare(actual_bmp, expected_bmp);
}

TYPED_TEST(RendererPixelTest, SimpleGreenRect) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorGREEN, false);

  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, SimpleGreenRect_NonRootRenderPass) {
  gfx::Rect rect(this->device_viewport_size_);
  gfx::Rect small_rect(100, 100);

  RenderPass::Id child_id(2, 1);
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_id, small_rect, gfx::Transform());

  SharedQuadState* child_shared_state =
      CreateTestSharedQuadState(child_pass.get(), gfx::Transform(), small_rect);

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(child_shared_state, rect, rect, SK_ColorGREEN, false);
  child_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPass::Id root_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRenderPass(root_id, rect, gfx::Transform());

  SharedQuadState* root_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), rect);

  scoped_ptr<DrawQuad> render_pass_quad =
      CreateTestRenderPassDrawQuad(root_shared_state, small_rect, child_id);
  root_pass->quad_list.push_back(render_pass_quad.PassAs<DrawQuad>());

  RenderPass* child_pass_ptr = child_pass.get();

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  EXPECT_TRUE(this->RunPixelTestWithReadbackTarget(
      &pass_list,
      child_pass_ptr,
      base::FilePath(FILE_PATH_LITERAL("green_small.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, PremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  scoped_ptr<TextureDrawQuad> texture_quad =
      CreateTestTextureDrawQuad(gfx::Rect(this->device_viewport_size_),
                                SkColorSetARGB(128, 0, 255, 0),  // Texel color.
                                SK_ColorTRANSPARENT,  // Background color.
                                true,                 // Premultiplied alpha.
                                shared_state,
                                this->resource_provider_.get());
  pass->quad_list.push_back(texture_quad.PassAs<DrawQuad>());

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorWHITE, false);
  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      FuzzyPixelOffByOneComparator(true)));
}

TYPED_TEST(RendererPixelTest, PremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  texture_quad_state->opacity = 0.8f;

  scoped_ptr<TextureDrawQuad> texture_quad = CreateTestTextureDrawQuad(
      gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(204, 120, 255, 120),  // Texel color.
      SK_ColorGREEN,                       // Background color.
      true,                                // Premultiplied alpha.
      texture_quad_state,
      this->resource_provider_.get());
  pass->quad_list.push_back(texture_quad.PassAs<DrawQuad>());

  SharedQuadState* color_quad_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(color_quad_state, rect, rect, SK_ColorWHITE, false);
  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      FuzzyPixelOffByOneComparator(true)));
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_F(GLRendererPixelTest, NonPremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  scoped_ptr<TextureDrawQuad> texture_quad =
      CreateTestTextureDrawQuad(gfx::Rect(this->device_viewport_size_),
                                SkColorSetARGB(128, 0, 255, 0),  // Texel color.
                                SK_ColorTRANSPARENT,  // Background color.
                                false,                // Premultiplied alpha.
                                shared_state,
                                this->resource_provider_.get());
  pass->quad_list.push_back(texture_quad.PassAs<DrawQuad>());

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorWHITE, false);
  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      FuzzyPixelOffByOneComparator(true)));
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_F(GLRendererPixelTest, NonPremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  texture_quad_state->opacity = 0.8f;

  scoped_ptr<TextureDrawQuad> texture_quad = CreateTestTextureDrawQuad(
      gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(204, 120, 255, 120),  // Texel color.
      SK_ColorGREEN,                       // Background color.
      false,                               // Premultiplied alpha.
      texture_quad_state,
      this->resource_provider_.get());
  pass->quad_list.push_back(texture_quad.PassAs<DrawQuad>());

  SharedQuadState* color_quad_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(color_quad_state, rect, rect, SK_ColorWHITE, false);
  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      FuzzyPixelOffByOneComparator(true)));
}

class VideoGLRendererPixelTest : public GLRendererPixelTest {
 protected:
  scoped_ptr<YUVVideoDrawQuad> CreateTestYUVVideoDrawQuad_Striped(
      SharedQuadState* shared_state,
      media::VideoFrame::Format format,
      bool is_transparent,
      const gfx::RectF& tex_coord_rect) {
    const gfx::Rect rect(this->device_viewport_size_);

    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            format, rect.size(), rect, rect.size(), base::TimeDelta());

    // YUV values representing a striped pattern, for validating texture
    // coordinates for sampling.
    uint8_t y_value = 0;
    uint8_t u_value = 0;
    uint8_t v_value = 0;
    for (int i = 0; i < video_frame->rows(media::VideoFrame::kYPlane); ++i) {
      uint8_t* y_row = video_frame->data(media::VideoFrame::kYPlane) +
                       video_frame->stride(media::VideoFrame::kYPlane) * i;
      for (int j = 0; j < video_frame->row_bytes(media::VideoFrame::kYPlane);
           ++j) {
        y_row[j] = (y_value += 1);
      }
    }
    for (int i = 0; i < video_frame->rows(media::VideoFrame::kUPlane); ++i) {
      uint8_t* u_row = video_frame->data(media::VideoFrame::kUPlane) +
                       video_frame->stride(media::VideoFrame::kUPlane) * i;
      uint8_t* v_row = video_frame->data(media::VideoFrame::kVPlane) +
                       video_frame->stride(media::VideoFrame::kVPlane) * i;
      for (int j = 0; j < video_frame->row_bytes(media::VideoFrame::kUPlane);
           ++j) {
        u_row[j] = (u_value += 3);
        v_row[j] = (v_value += 5);
      }
    }
    return CreateTestYUVVideoDrawQuad_FromVideoFrame(
        shared_state, video_frame, is_transparent, tex_coord_rect);
  }

  scoped_ptr<YUVVideoDrawQuad> CreateTestYUVVideoDrawQuad_Solid(
      SharedQuadState* shared_state,
      media::VideoFrame::Format format,
      bool is_transparent,
      const gfx::RectF& tex_coord_rect,
      uint8 y,
      uint8 u,
      uint8 v) {
    const gfx::Rect rect(this->device_viewport_size_);

    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateFrame(
            format, rect.size(), rect, rect.size(), base::TimeDelta());

    // YUV values of a solid, constant, color. Useful for testing that color
    // space/color range are being handled properly.
    memset(video_frame->data(media::VideoFrame::kYPlane),
           y,
           video_frame->stride(media::VideoFrame::kYPlane) *
               video_frame->rows(media::VideoFrame::kYPlane));
    memset(video_frame->data(media::VideoFrame::kUPlane),
           u,
           video_frame->stride(media::VideoFrame::kUPlane) *
               video_frame->rows(media::VideoFrame::kUPlane));
    memset(video_frame->data(media::VideoFrame::kVPlane),
           v,
           video_frame->stride(media::VideoFrame::kVPlane) *
               video_frame->rows(media::VideoFrame::kVPlane));

    return CreateTestYUVVideoDrawQuad_FromVideoFrame(
        shared_state, video_frame, is_transparent, tex_coord_rect);
  }

  scoped_ptr<YUVVideoDrawQuad> CreateTestYUVVideoDrawQuad_FromVideoFrame(
      SharedQuadState* shared_state,
      scoped_refptr<media::VideoFrame> video_frame,
      bool is_transparent,
      const gfx::RectF& tex_coord_rect) {
    const bool with_alpha = (video_frame->format() == media::VideoFrame::YV12A);
    const YUVVideoDrawQuad::ColorSpace color_space =
        (video_frame->format() == media::VideoFrame::YV12J
             ? YUVVideoDrawQuad::REC_601_JPEG
             : YUVVideoDrawQuad::REC_601);
    const gfx::Rect rect(this->device_viewport_size_);
    const gfx::Rect opaque_rect(0, 0, 0, 0);

    if (with_alpha)
      memset(video_frame->data(media::VideoFrame::kAPlane),
             is_transparent ? 0 : 128,
             video_frame->stride(media::VideoFrame::kAPlane) *
                 video_frame->rows(media::VideoFrame::kAPlane));

    VideoFrameExternalResources resources =
        video_resource_updater_->CreateExternalResourcesFromVideoFrame(
            video_frame);

    EXPECT_EQ(VideoFrameExternalResources::YUV_RESOURCE, resources.type);
    EXPECT_EQ(media::VideoFrame::NumPlanes(video_frame->format()),
              resources.mailboxes.size());
    EXPECT_EQ(media::VideoFrame::NumPlanes(video_frame->format()),
              resources.release_callbacks.size());

    ResourceProvider::ResourceId y_resource =
        resource_provider_->CreateResourceFromTextureMailbox(
            resources.mailboxes[media::VideoFrame::kYPlane],
            SingleReleaseCallback::Create(
                resources.release_callbacks[media::VideoFrame::kYPlane]));
    ResourceProvider::ResourceId u_resource =
        resource_provider_->CreateResourceFromTextureMailbox(
            resources.mailboxes[media::VideoFrame::kUPlane],
            SingleReleaseCallback::Create(
                resources.release_callbacks[media::VideoFrame::kUPlane]));
    ResourceProvider::ResourceId v_resource =
        resource_provider_->CreateResourceFromTextureMailbox(
            resources.mailboxes[media::VideoFrame::kVPlane],
            SingleReleaseCallback::Create(
                resources.release_callbacks[media::VideoFrame::kVPlane]));
    ResourceProvider::ResourceId a_resource = 0;
    if (with_alpha) {
      a_resource = resource_provider_->CreateResourceFromTextureMailbox(
          resources.mailboxes[media::VideoFrame::kAPlane],
          SingleReleaseCallback::Create(
              resources.release_callbacks[media::VideoFrame::kAPlane]));
    }

    scoped_ptr<YUVVideoDrawQuad> yuv_quad = YUVVideoDrawQuad::Create();
    yuv_quad->SetNew(shared_state,
                     rect,
                     opaque_rect,
                     rect,
                     tex_coord_rect,
                     y_resource,
                     u_resource,
                     v_resource,
                     a_resource,
                     color_space);
    return yuv_quad.Pass();
  }

  virtual void SetUp() OVERRIDE {
    GLRendererPixelTest::SetUp();
    video_resource_updater_.reset(new VideoResourceUpdater(
        output_surface_->context_provider().get(), resource_provider_.get()));
  }

 private:
  scoped_ptr<VideoResourceUpdater> video_resource_updater_;
};

TEST_F(VideoGLRendererPixelTest, SimpleYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  scoped_ptr<YUVVideoDrawQuad> yuv_quad =
      CreateTestYUVVideoDrawQuad_Striped(shared_state,
                                         media::VideoFrame::YV12,
                                         false,
                                         gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f));

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(
      this->RunPixelTest(&pass_list,
                         base::FilePath(FILE_PATH_LITERAL("yuv_stripes.png")),
                         FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, OffsetYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  // Intentionally sets frame format to I420 for testing coverage.
  scoped_ptr<YUVVideoDrawQuad> yuv_quad = CreateTestYUVVideoDrawQuad_Striped(
      shared_state,
      media::VideoFrame::I420,
      false,
      gfx::RectF(0.125f, 0.25f, 0.75f, 0.5f));

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("yuv_stripes_offset.png")),
      FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, SimpleYUVRectBlack) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  // In MPEG color range YUV values of (15,128,128) should produce black.
  scoped_ptr<YUVVideoDrawQuad> yuv_quad =
      CreateTestYUVVideoDrawQuad_Solid(shared_state,
                                       media::VideoFrame::YV12,
                                       false,
                                       gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f),
                                       15,
                                       128,
                                       128);

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  // If we didn't get black out of the YUV values above, then we probably have a
  // color range issue.
  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("black.png")),
                                 FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, SimpleYUVJRect) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  // YUV of (149,43,21) should be green (0,255,0) in RGB.
  scoped_ptr<YUVVideoDrawQuad> yuv_quad =
      CreateTestYUVVideoDrawQuad_Solid(shared_state,
                                       media::VideoFrame::YV12J,
                                       false,
                                       gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f),
                                       149,
                                       43,
                                       21);

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, SimpleYUVJRectGrey) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  // Dark grey in JPEG color range (in MPEG, this is black).
  scoped_ptr<YUVVideoDrawQuad> yuv_quad =
      CreateTestYUVVideoDrawQuad_Solid(shared_state,
                                       media::VideoFrame::YV12J,
                                       false,
                                       gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f),
                                       15,
                                       128,
                                       128);

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(
      this->RunPixelTest(&pass_list,
                         base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
                         FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, SimpleYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  scoped_ptr<YUVVideoDrawQuad> yuv_quad =
      CreateTestYUVVideoDrawQuad_Striped(shared_state,
                                         media::VideoFrame::YV12A,
                                         false,
                                         gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f));

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorWHITE, false);

  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("yuv_stripes_alpha.png")),
      FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, FullyTransparentYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  scoped_ptr<YUVVideoDrawQuad> yuv_quad =
      CreateTestYUVVideoDrawQuad_Striped(shared_state,
                                         media::VideoFrame::YV12A,
                                         true,
                                         gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f));

  pass->quad_list.push_back(yuv_quad.PassAs<DrawQuad>());

  scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorBLACK, false);

  pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("black.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, FastPassColorFilterAlpha) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0,
                        this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  scoped_ptr<SolidColorDrawQuad> white = SolidColorDrawQuad::Create();
  white->SetNew(
      blank_state, viewport_rect, viewport_rect, SK_ColorWHITE, false);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(white.PassAs<DrawQuad>());

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), pass_rect);

  SkScalar matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  skia::RefPtr<SkColorFilter> colorFilter(
      skia::AdoptRef(SkColorMatrixFilter::Create(matrix)));
  skia::RefPtr<SkImageFilter> filter =
      skia::AdoptRef(SkColorFilterImageFilter::Create(colorFilter.get(), NULL));
  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(filter));

  scoped_ptr<RenderPassDrawQuad> render_pass_quad =
      RenderPassDrawQuad::Create();
  render_pass_quad->SetNew(pass_shared_state,
                           pass_rect,
                           pass_rect,
                           child_pass_id,
                           false,
                           0,
                           pass_rect,
                           gfx::RectF(),
                           filters,
                           FilterOperations());

  root_pass->quad_list.push_back(render_pass_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      FuzzyForSoftwareOnlyPixelComparator<TypeParam>(false)));
}

TYPED_TEST(RendererPixelTest, FastPassSaturateFilter) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0,
                        this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  scoped_ptr<SolidColorDrawQuad> white = SolidColorDrawQuad::Create();
  white->SetNew(
      blank_state, viewport_rect, viewport_rect, SK_ColorWHITE, false);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(white.PassAs<DrawQuad>());

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), pass_rect);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateSaturateFilter(0.5f));

  scoped_ptr<RenderPassDrawQuad> render_pass_quad =
      RenderPassDrawQuad::Create();
  render_pass_quad->SetNew(pass_shared_state,
                           pass_rect,
                           pass_rect,
                           child_pass_id,
                           false,
                           0,
                           pass_rect,
                           gfx::RectF(),
                           filters,
                           FilterOperations());

  root_pass->quad_list.push_back(render_pass_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, FastPassFilterChain) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0,
                        this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  scoped_ptr<SolidColorDrawQuad> white = SolidColorDrawQuad::Create();
  white->SetNew(
      blank_state, viewport_rect, viewport_rect, SK_ColorWHITE, false);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(white.PassAs<DrawQuad>());

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), pass_rect);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.f));
  filters.Append(FilterOperation::CreateBrightnessFilter(0.5f));

  scoped_ptr<RenderPassDrawQuad> render_pass_quad =
      RenderPassDrawQuad::Create();
  render_pass_quad->SetNew(pass_shared_state,
                           pass_rect,
                           pass_rect,
                           child_pass_id,
                           false,
                           0,
                           pass_rect,
                           gfx::RectF(),
                           filters,
                           FilterOperations());

  root_pass->quad_list.push_back(render_pass_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, FastPassColorFilterAlphaTranslation) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0,
                        this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  scoped_ptr<SolidColorDrawQuad> white = SolidColorDrawQuad::Create();
  white->SetNew(
      blank_state, viewport_rect, viewport_rect, SK_ColorWHITE, false);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(white.PassAs<DrawQuad>());

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), pass_rect);

  SkScalar matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = 0;
  matrix[4] = 20.f;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = 0;
  matrix[9] = 200.f;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = 0;
  matrix[14] = 1.5f;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  skia::RefPtr<SkColorFilter> colorFilter(
      skia::AdoptRef(SkColorMatrixFilter::Create(matrix)));
  skia::RefPtr<SkImageFilter> filter =
      skia::AdoptRef(SkColorFilterImageFilter::Create(colorFilter.get(), NULL));
  FilterOperations filters;
  filters.Append(FilterOperation::CreateReferenceFilter(filter));

  scoped_ptr<RenderPassDrawQuad> render_pass_quad =
      RenderPassDrawQuad::Create();
  render_pass_quad->SetNew(pass_shared_state,
                           pass_rect,
                           pass_rect,
                           child_pass_id,
                           false,
                           0,
                           pass_rect,
                           gfx::RectF(),
                           filters,
                           FilterOperations());

  root_pass->quad_list.push_back(render_pass_quad.PassAs<DrawQuad>());
  RenderPassList pass_list;

  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha_translate.png")),
      FuzzyForSoftwareOnlyPixelComparator<TypeParam>(false)));
}

TYPED_TEST(RendererPixelTest, EnlargedRenderPassTexture) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0,
                        this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), pass_rect);
  root_pass->quad_list.push_back(CreateTestRenderPassDrawQuad(
      pass_shared_state, pass_rect, child_pass_id));

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Vector2d(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, EnlargedRenderPassTextureWithAntiAliasing) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0,
                        this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  child_pass->quad_list.push_back(yellow.PassAs<DrawQuad>());

  gfx::Transform aa_transform;
  aa_transform.Translate(0.5, 0.0);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), aa_transform, pass_rect);
  root_pass->quad_list.push_back(CreateTestRenderPassDrawQuad(
      pass_shared_state, pass_rect, child_pass_id));

  SharedQuadState* root_shared_state = CreateTestSharedQuadState(
      root_pass.get(), gfx::Transform(), viewport_rect);
  scoped_ptr<SolidColorDrawQuad> background = SolidColorDrawQuad::Create();
  background->SetNew(root_shared_state,
                     gfx::Rect(this->device_viewport_size_),
                     gfx::Rect(this->device_viewport_size_),
                     SK_ColorWHITE,
                     false);
  root_pass->quad_list.push_back(background.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Vector2d(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_anti_aliasing.png")),
      FuzzyPixelOffByOneComparator(true)));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TYPED_TEST(RendererPixelTest, RenderPassAndMaskWithPartialQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      root_pass.get(), gfx::Transform(), viewport_rect);

  RenderPass::Id child_pass_id(2, 2);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      child_pass.get(), gfx::Transform(), viewport_rect);

  // The child render pass is just a green box.
  static const SkColor kCSSGreen = 0xff008000;
  scoped_ptr<SolidColorDrawQuad> green = SolidColorDrawQuad::Create();
  green->SetNew(
      child_pass_shared_state, viewport_rect, viewport_rect, kCSSGreen, false);
  child_pass->quad_list.push_back(green.PassAs<DrawQuad>());

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(SkIntToScalar(4));
  paint.setColor(SK_ColorWHITE);
  canvas.clear(SK_ColorTRANSPARENT);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(6, 6, 4, 4);
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        paint);
    rect.Inset(6, 6, 4, 4);
  }

  ResourceProvider::ResourceId mask_resource_id =
      this->resource_provider_->CreateResource(
          mask_rect.size(),
          GL_CLAMP_TO_EDGE,
          ResourceProvider::TextureUsageAny,
          RGBA_8888);
  {
    SkAutoLockPixels lock(bitmap);
    this->resource_provider_->SetPixels(
        mask_resource_id,
        reinterpret_cast<uint8_t*>(bitmap.getPixels()),
        mask_rect,
        mask_rect,
        gfx::Vector2d());
  }

  // This RenderPassDrawQuad does not include the full |viewport_rect| which is
  // the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 50, 100, 100);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());
  EXPECT_TRUE(child_pass->output_rect.Contains(sub_rect));

  // Set up a mask on the RenderPassDrawQuad.
  scoped_ptr<RenderPassDrawQuad> mask_quad = RenderPassDrawQuad::Create();
  mask_quad->SetNew(root_pass_shared_state,
                    sub_rect,
                    sub_rect,
                    child_pass_id,
                    false,  // is_replica
                    mask_resource_id,
                    sub_rect,              // contents_changed_since_last_frame
                    gfx::RectF(1.f, 1.f),  // mask_uv_rect
                    FilterOperations(),    // foreground filters
                    FilterOperations());   // background filters
  root_pass->quad_list.push_back(mask_quad.PassAs<DrawQuad>());

  // White background behind the masked render pass.
  scoped_ptr<SolidColorDrawQuad> white = SolidColorDrawQuad::Create();
  white->SetNew(root_pass_shared_state,
                viewport_rect,
                viewport_rect,
                SK_ColorWHITE,
                false);
  root_pass->quad_list.push_back(white.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("image_mask_of_layer.png")),
      ExactPixelComparator(true)));
}

template <typename RendererType>
class RendererPixelTestWithBackgroundFilter
    : public RendererPixelTest<RendererType> {
 protected:
  void SetUpRenderPassList() {
    gfx::Rect device_viewport_rect(this->device_viewport_size_);

    RenderPass::Id root_id(1, 1);
    scoped_ptr<RenderPass> root_pass =
        CreateTestRootRenderPass(root_id, device_viewport_rect);
    root_pass->has_transparent_background = false;

    gfx::Transform identity_content_to_target_transform;

    RenderPass::Id filter_pass_id(2, 1);
    gfx::Transform transform_to_root;
    scoped_ptr<RenderPass> filter_pass =
        CreateTestRenderPass(filter_pass_id,
                             filter_pass_content_rect_,
                             transform_to_root);

    // A non-visible quad in the filtering render pass.
    {
      SharedQuadState* shared_state =
          CreateTestSharedQuadState(filter_pass.get(),
                                    identity_content_to_target_transform,
                                    filter_pass_content_rect_);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(shared_state,
                         filter_pass_content_rect_,
                         filter_pass_content_rect_,
                         SK_ColorTRANSPARENT,
                         false);
      filter_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
    }

    {
      SharedQuadState* shared_state =
          CreateTestSharedQuadState(filter_pass.get(),
                                    filter_pass_to_target_transform_,
                                    filter_pass_content_rect_);
      scoped_ptr<RenderPassDrawQuad> filter_pass_quad =
          RenderPassDrawQuad::Create();
      filter_pass_quad->SetNew(
          shared_state,
          filter_pass_content_rect_,
          filter_pass_content_rect_,
          filter_pass_id,
          false,                      // is_replica
          0,                          // mask_resource_id
          filter_pass_content_rect_,  // contents_changed_since_last_frame
          gfx::RectF(),               // mask_uv_rect
          FilterOperations(),         // filters
          this->background_filters_);
      root_pass->quad_list.push_back(filter_pass_quad.PassAs<DrawQuad>());
    }

    const int kColumnWidth = device_viewport_rect.width() / 3;

    gfx::Rect left_rect = gfx::Rect(0, 0, kColumnWidth, 20);
    for (int i = 0; left_rect.y() < device_viewport_rect.height(); ++i) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          root_pass.get(), identity_content_to_target_transform, left_rect);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(
          shared_state, left_rect, left_rect, SK_ColorGREEN, false);
      root_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      left_rect += gfx::Vector2d(0, left_rect.height() + 1);
    }

    gfx::Rect middle_rect = gfx::Rect(kColumnWidth+1, 0, kColumnWidth, 20);
    for (int i = 0; middle_rect.y() < device_viewport_rect.height(); ++i) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          root_pass.get(), identity_content_to_target_transform, middle_rect);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(
          shared_state, middle_rect, middle_rect, SK_ColorRED, false);
      root_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      middle_rect += gfx::Vector2d(0, middle_rect.height() + 1);
    }

    gfx::Rect right_rect = gfx::Rect((kColumnWidth+1)*2, 0, kColumnWidth, 20);
    for (int i = 0; right_rect.y() < device_viewport_rect.height(); ++i) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          root_pass.get(), identity_content_to_target_transform, right_rect);
      scoped_ptr<SolidColorDrawQuad> color_quad = SolidColorDrawQuad::Create();
      color_quad->SetNew(
          shared_state, right_rect, right_rect, SK_ColorBLUE, false);
      root_pass->quad_list.push_back(color_quad.PassAs<DrawQuad>());
      right_rect += gfx::Vector2d(0, right_rect.height() + 1);
    }

    SharedQuadState* shared_state =
        CreateTestSharedQuadState(root_pass.get(),
                                  identity_content_to_target_transform,
                                  device_viewport_rect);
    scoped_ptr<SolidColorDrawQuad> background_quad =
        SolidColorDrawQuad::Create();
    background_quad->SetNew(shared_state,
                            device_viewport_rect,
                            device_viewport_rect,
                            SK_ColorWHITE,
                            false);
    root_pass->quad_list.push_back(background_quad.PassAs<DrawQuad>());

    pass_list_.push_back(filter_pass.Pass());
    pass_list_.push_back(root_pass.Pass());
  }

  RenderPassList pass_list_;
  FilterOperations background_filters_;
  gfx::Transform filter_pass_to_target_transform_;
  gfx::Rect filter_pass_content_rect_;
};

typedef ::testing::Types<GLRenderer, SoftwareRenderer>
    BackgroundFilterRendererTypes;
TYPED_TEST_CASE(RendererPixelTestWithBackgroundFilter,
                BackgroundFilterRendererTypes);

typedef RendererPixelTestWithBackgroundFilter<GLRenderer>
GLRendererPixelTestWithBackgroundFilter;

// TODO(skaslev): The software renderer does not support filters yet.
TEST_F(GLRendererPixelTestWithBackgroundFilter, InvertFilter) {
  this->background_filters_.Append(
      FilterOperation::CreateInvertFilter(1.f));

  this->filter_pass_content_rect_ = gfx::Rect(this->device_viewport_size_);
  this->filter_pass_content_rect_.Inset(12, 14, 16, 18);

  this->SetUpRenderPassList();
  EXPECT_TRUE(this->RunPixelTest(
      &this->pass_list_,
      base::FilePath(FILE_PATH_LITERAL("background_filter.png")),
      ExactPixelComparator(true)));
}

class ExternalStencilPixelTest : public GLRendererPixelTest {
 protected:
  void ClearBackgroundToGreen() {
    GLES2Interface* gl = output_surface_->context_provider()->ContextGL();
    output_surface_->EnsureBackbuffer();
    output_surface_->Reshape(device_viewport_size_, 1);
    gl->ClearColor(0.f, 1.f, 0.f, 1.f);
    gl->Clear(GL_COLOR_BUFFER_BIT);
  }

  void PopulateStencilBuffer() {
    // Set two quadrants of the stencil buffer to 1.
    GLES2Interface* gl = output_surface_->context_provider()->ContextGL();
    output_surface_->EnsureBackbuffer();
    output_surface_->Reshape(device_viewport_size_, 1);
    gl->ClearStencil(0);
    gl->Clear(GL_STENCIL_BUFFER_BIT);
    gl->Enable(GL_SCISSOR_TEST);
    gl->ClearStencil(1);
    gl->Scissor(0,
                0,
                device_viewport_size_.width() / 2,
                device_viewport_size_.height() / 2);
    gl->Clear(GL_STENCIL_BUFFER_BIT);
    gl->Scissor(device_viewport_size_.width() / 2,
                device_viewport_size_.height() / 2,
                device_viewport_size_.width(),
                device_viewport_size_.height());
    gl->Clear(GL_STENCIL_BUFFER_BIT);
  }
};

TEST_F(ExternalStencilPixelTest, StencilTestEnabled) {
  ClearBackgroundToGreen();
  PopulateStencilBuffer();
  this->EnableExternalStencilTest();

  // Draw a blue quad that covers the entire device viewport. It should be
  // clipped to the bottom left and top right corners by the external stencil.
  gfx::Rect rect(this->device_viewport_size_);
  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);
  pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  pass->has_transparent_background = false;
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      ExactPixelComparator(true)));
}

TEST_F(ExternalStencilPixelTest, StencilTestDisabled) {
  PopulateStencilBuffer();

  // Draw a green quad that covers the entire device viewport. The stencil
  // buffer should be ignored.
  gfx::Rect rect(this->device_viewport_size_);
  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  scoped_ptr<SolidColorDrawQuad> green = SolidColorDrawQuad::Create();
  green->SetNew(green_shared_state, rect, rect, SK_ColorGREEN, false);
  pass->quad_list.push_back(green.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green.png")),
      ExactPixelComparator(true)));
}

TEST_F(ExternalStencilPixelTest, RenderSurfacesIgnoreStencil) {
  // The stencil test should apply only to the final render pass.
  ClearBackgroundToGreen();
  PopulateStencilBuffer();
  this->EnableExternalStencilTest();

  gfx::Rect viewport_rect(this->device_viewport_size_);

  RenderPass::Id root_pass_id(1, 1);
  scoped_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);
  root_pass->has_transparent_background = false;

  RenderPass::Id child_pass_id(2, 2);
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      child_pass.get(), content_to_target_transform, viewport_rect);

  gfx::Rect blue_rect(0,
                      0,
                      this->device_viewport_size_.width(),
                      this->device_viewport_size_.height());
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  child_pass->quad_list.push_back(blue.PassAs<DrawQuad>());

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(root_pass.get(), gfx::Transform(), pass_rect);
  root_pass->quad_list.push_back(CreateTestRenderPassDrawQuad(
      pass_shared_state, pass_rect, child_pass_id));
  RenderPassList pass_list;
  pass_list.push_back(child_pass.Pass());
  pass_list.push_back(root_pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      ExactPixelComparator(true)));
}

TEST_F(ExternalStencilPixelTest, DeviceClip) {
  ClearBackgroundToGreen();
  gfx::Rect clip_rect(gfx::Point(150, 150), gfx::Size(50, 50));
  this->ForceDeviceClip(clip_rect);

  // Draw a blue quad that covers the entire device viewport. It should be
  // clipped to the bottom right corner by the device clip.
  gfx::Rect rect(this->device_viewport_size_);
  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);
  pass->quad_list.push_back(blue.PassAs<DrawQuad>());
  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")),
      ExactPixelComparator(true)));
}

// Software renderer does not support anti-aliased edges.
TEST_F(GLRendererPixelTest, AntiAliasing) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  gfx::Transform red_content_to_target_transform;
  red_content_to_target_transform.Rotate(10);
  SharedQuadState* red_shared_state = CreateTestSharedQuadState(
      pass.get(), red_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> red = SolidColorDrawQuad::Create();
  red->SetNew(red_shared_state, rect, rect, SK_ColorRED, false);

  pass->quad_list.push_back(red.PassAs<DrawQuad>());

  gfx::Transform yellow_content_to_target_transform;
  yellow_content_to_target_transform.Rotate(5);
  SharedQuadState* yellow_shared_state = CreateTestSharedQuadState(
      pass.get(), yellow_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(yellow_shared_state, rect, rect, SK_ColorYELLOW, false);

  pass->quad_list.push_back(yellow.PassAs<DrawQuad>());

  gfx::Transform blue_content_to_target_transform;
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      pass.get(), blue_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);

  pass->quad_list.push_back(blue.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing.png")),
      FuzzyPixelOffByOneComparator(true)));
}

// This test tests that anti-aliasing works for axis aligned quads.
// Anti-aliasing is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, AxisAligned) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform red_content_to_target_transform;
  red_content_to_target_transform.Translate(50, 50);
  red_content_to_target_transform.Scale(
      0.5f + 1.0f / (rect.width() * 2.0f),
      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* red_shared_state = CreateTestSharedQuadState(
      pass.get(), red_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> red = SolidColorDrawQuad::Create();
  red->SetNew(red_shared_state, rect, rect, SK_ColorRED, false);

  pass->quad_list.push_back(red.PassAs<DrawQuad>());

  gfx::Transform yellow_content_to_target_transform;
  yellow_content_to_target_transform.Translate(25.5f, 25.5f);
  yellow_content_to_target_transform.Scale(0.5f, 0.5f);
  SharedQuadState* yellow_shared_state = CreateTestSharedQuadState(
      pass.get(), yellow_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> yellow = SolidColorDrawQuad::Create();
  yellow->SetNew(yellow_shared_state, rect, rect, SK_ColorYELLOW, false);

  pass->quad_list.push_back(yellow.PassAs<DrawQuad>());

  gfx::Transform blue_content_to_target_transform;
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      pass.get(), blue_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);

  pass->quad_list.push_back(blue.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("axis_aligned.png")),
      ExactPixelComparator(true)));
}

// This test tests that forcing anti-aliasing off works as expected.
// Anti-aliasing is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, ForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform hole_content_to_target_transform;
  hole_content_to_target_transform.Translate(50, 50);
  hole_content_to_target_transform.Scale(
      0.5f + 1.0f / (rect.width() * 2.0f),
      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state = CreateTestSharedQuadState(
      pass.get(), hole_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> hole = SolidColorDrawQuad::Create();
  hole->SetAll(
      hole_shared_state, rect, rect, rect, false, SK_ColorTRANSPARENT, true);
  pass->quad_list.push_back(hole.PassAs<DrawQuad>());

  gfx::Transform green_content_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      pass.get(), green_content_to_target_transform, rect);

  scoped_ptr<SolidColorDrawQuad> green = SolidColorDrawQuad::Create();
  green->SetNew(green_shared_state, rect, rect, SK_ColorGREEN, false);

  pass->quad_list.push_back(green.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      ExactPixelComparator(false)));
}

TEST_F(GLRendererPixelTest, AntiAliasingPerspective) {
  gfx::Rect rect(this->device_viewport_size_);

  scoped_ptr<RenderPass> pass =
      CreateTestRootRenderPass(RenderPass::Id(1, 1), rect);

  gfx::Rect red_rect(0, 0, 180, 500);
  gfx::Transform red_content_to_target_transform(
      1.0f,  2.4520f,  10.6206f, 19.0f,
      0.0f,  0.3528f,  5.9737f,  9.5f,
      0.0f, -0.2250f, -0.9744f,  0.0f,
      0.0f,  0.0225f,  0.0974f,  1.0f);
  SharedQuadState* red_shared_state = CreateTestSharedQuadState(
      pass.get(), red_content_to_target_transform, red_rect);
  scoped_ptr<SolidColorDrawQuad> red = SolidColorDrawQuad::Create();
  red->SetNew(red_shared_state, red_rect, red_rect, SK_ColorRED, false);
  pass->quad_list.push_back(red.PassAs<DrawQuad>());

  gfx::Rect green_rect(19, 7, 180, 10);
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), green_rect);
  scoped_ptr<SolidColorDrawQuad> green = SolidColorDrawQuad::Create();
  green->SetNew(
      green_shared_state, green_rect, green_rect, SK_ColorGREEN, false);
  pass->quad_list.push_back(green.PassAs<DrawQuad>());

  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);
  scoped_ptr<SolidColorDrawQuad> blue = SolidColorDrawQuad::Create();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);
  pass->quad_list.push_back(blue.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing_perspective.png")),
      FuzzyPixelOffByOneComparator(true)));
}

TYPED_TEST(RendererPixelTest, PictureDrawQuadIdentityScale) {
  gfx::Size pile_tile_size(1000, 1000);
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  ResourceFormat texture_format = RGBA_8888;

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  // One clipped blue quad in the lower right corner.  Outside the clip
  // is red, which should not appear.
  gfx::Rect blue_rect(gfx::Size(100, 100));
  gfx::Rect blue_clip_rect(gfx::Point(50, 50), gfx::Size(50, 50));
  scoped_refptr<FakePicturePileImpl> blue_pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, blue_rect.size());
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  blue_pile->add_draw_rect_with_paint(blue_rect, red_paint);
  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  blue_pile->add_draw_rect_with_paint(blue_clip_rect, blue_paint);
  blue_pile->RerecordPile();

  gfx::Transform blue_content_to_target_transform;
  gfx::Vector2d offset(viewport.bottom_right() - blue_rect.bottom_right());
  blue_content_to_target_transform.Translate(offset.x(), offset.y());
  gfx::RectF blue_scissor_rect = blue_clip_rect;
  blue_content_to_target_transform.TransformRect(&blue_scissor_rect);
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadStateClipped(pass.get(),
                                       blue_content_to_target_transform,
                                       blue_rect,
                                       gfx::ToEnclosingRect(blue_scissor_rect));

  scoped_ptr<PictureDrawQuad> blue_quad = PictureDrawQuad::Create();

  blue_quad->SetNew(blue_shared_state,
                    viewport,  // Intentionally bigger than clip.
                    gfx::Rect(),
                    viewport,
                    gfx::RectF(viewport),
                    viewport.size(),
                    texture_format,
                    viewport,
                    1.f,
                    PicturePileImpl::CreateFromOther(blue_pile));
  pass->quad_list.push_back(blue_quad.PassAs<DrawQuad>());

  // One viewport-filling green quad.
  scoped_refptr<FakePicturePileImpl> green_pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, viewport.size());
  SkPaint green_paint;
  green_paint.setColor(SK_ColorGREEN);
  green_pile->add_draw_rect_with_paint(viewport, green_paint);
  green_pile->RerecordPile();

  gfx::Transform green_content_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      pass.get(), green_content_to_target_transform, viewport);

  scoped_ptr<PictureDrawQuad> green_quad = PictureDrawQuad::Create();
  green_quad->SetNew(green_shared_state,
                     viewport,
                     gfx::Rect(),
                     viewport,
                     gfx::RectF(0.f, 0.f, 1.f, 1.f),
                     viewport.size(),
                     texture_format,
                     viewport,
                     1.f,
                     PicturePileImpl::CreateFromOther(green_pile));
  pass->quad_list.push_back(green_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")),
      ExactPixelComparator(true)));
}

// Not WithSkiaGPUBackend since that path currently requires tiles for opacity.
TYPED_TEST(RendererPixelTest, PictureDrawQuadOpacity) {
  gfx::Size pile_tile_size(1000, 1000);
  gfx::Rect viewport(this->device_viewport_size_);
  ResourceFormat texture_format = RGBA_8888;

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  // One viewport-filling 0.5-opacity green quad.
  scoped_refptr<FakePicturePileImpl> green_pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, viewport.size());
  SkPaint green_paint;
  green_paint.setColor(SK_ColorGREEN);
  green_pile->add_draw_rect_with_paint(viewport, green_paint);
  green_pile->RerecordPile();

  gfx::Transform green_content_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      pass.get(), green_content_to_target_transform, viewport);
  green_shared_state->opacity = 0.5f;

  scoped_ptr<PictureDrawQuad> green_quad = PictureDrawQuad::Create();
  green_quad->SetNew(green_shared_state,
                     viewport,
                     gfx::Rect(),
                     viewport,
                     gfx::RectF(0, 0, 1, 1),
                     viewport.size(),
                     texture_format,
                     viewport,
                     1.f,
                     PicturePileImpl::CreateFromOther(green_pile));
  pass->quad_list.push_back(green_quad.PassAs<DrawQuad>());

  // One viewport-filling white quad.
  scoped_refptr<FakePicturePileImpl> white_pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, viewport.size());
  SkPaint white_paint;
  white_paint.setColor(SK_ColorWHITE);
  white_pile->add_draw_rect_with_paint(viewport, white_paint);
  white_pile->RerecordPile();

  gfx::Transform white_content_to_target_transform;
  SharedQuadState* white_shared_state = CreateTestSharedQuadState(
      pass.get(), white_content_to_target_transform, viewport);

  scoped_ptr<PictureDrawQuad> white_quad = PictureDrawQuad::Create();
  white_quad->SetNew(white_shared_state,
                     viewport,
                     gfx::Rect(),
                     viewport,
                     gfx::RectF(0, 0, 1, 1),
                     viewport.size(),
                     texture_format,
                     viewport,
                     1.f,
                     PicturePileImpl::CreateFromOther(white_pile));
  pass->quad_list.push_back(white_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      FuzzyPixelOffByOneComparator(true)));
}

template<typename TypeParam> bool IsSoftwareRenderer() {
  return false;
}

template<>
bool IsSoftwareRenderer<SoftwareRenderer>() {
  return true;
}

template<>
bool IsSoftwareRenderer<SoftwareRendererWithExpandedViewport>() {
  return true;
}

// If we disable image filtering, then a 2x2 bitmap should appear as four
// huge sharp squares.
TYPED_TEST(RendererPixelTest, PictureDrawQuadDisableImageFiltering) {
  // We only care about this in software mode since bilinear filtering is
  // cheap in hardware.
  if (!IsSoftwareRenderer<TypeParam>())
    return;

  gfx::Size pile_tile_size(1000, 1000);
  gfx::Rect viewport(this->device_viewport_size_);
  ResourceFormat texture_format = RGBA_8888;

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  {
    SkAutoLockPixels lock(bitmap);
    SkCanvas canvas(bitmap);
    canvas.drawPoint(0, 0, SK_ColorGREEN);
    canvas.drawPoint(0, 1, SK_ColorBLUE);
    canvas.drawPoint(1, 0, SK_ColorBLUE);
    canvas.drawPoint(1, 1, SK_ColorGREEN);
  }

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, viewport.size());
  SkPaint paint;
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);
  pile->add_draw_bitmap_with_paint(bitmap, gfx::Point(), paint);
  pile->RerecordPile();

  gfx::Transform content_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      pass.get(), content_to_target_transform, viewport);

  scoped_ptr<PictureDrawQuad> quad = PictureDrawQuad::Create();
  quad->SetNew(shared_state,
               viewport,
               gfx::Rect(),
               viewport,
               gfx::RectF(0, 0, 2, 2),
               viewport.size(),
               texture_format,
               viewport,
               1.f,
               PicturePileImpl::CreateFromOther(pile));
  pass->quad_list.push_back(quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  this->disable_picture_quad_image_filtering_ = true;

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, PictureDrawQuadNonIdentityScale) {
  gfx::Size pile_tile_size(1000, 1000);
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  ResourceFormat texture_format = RGBA_8888;

  RenderPass::Id id(1, 1);
  gfx::Transform transform_to_root;
  scoped_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  // As scaling up the blue checkerboards will cause sampling on the GPU,
  // a few extra "cleanup rects" need to be added to clobber the blending
  // to make the output image more clean.  This will also test subrects
  // of the layer.
  gfx::Transform green_content_to_target_transform;
  gfx::Rect green_rect1(gfx::Point(80, 0), gfx::Size(20, 100));
  gfx::Rect green_rect2(gfx::Point(0, 80), gfx::Size(100, 20));
  scoped_refptr<FakePicturePileImpl> green_pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, viewport.size());
  SkPaint red_paint;
  red_paint.setColor(SK_ColorRED);
  green_pile->add_draw_rect_with_paint(viewport, red_paint);
  SkPaint green_paint;
  green_paint.setColor(SK_ColorGREEN);
  green_pile->add_draw_rect_with_paint(green_rect1, green_paint);
  green_pile->add_draw_rect_with_paint(green_rect2, green_paint);
  green_pile->RerecordPile();

  SharedQuadState* top_right_green_shared_quad_state =
      CreateTestSharedQuadState(
          pass.get(), green_content_to_target_transform, viewport);

  scoped_ptr<PictureDrawQuad> green_quad1 = PictureDrawQuad::Create();
  green_quad1->SetNew(top_right_green_shared_quad_state,
                      green_rect1,
                      gfx::Rect(),
                      green_rect1,
                      gfx::RectF(green_rect1.size()),
                      green_rect1.size(),
                      texture_format,
                      green_rect1,
                      1.f,
                      PicturePileImpl::CreateFromOther(green_pile));
  pass->quad_list.push_back(green_quad1.PassAs<DrawQuad>());

  scoped_ptr<PictureDrawQuad> green_quad2 = PictureDrawQuad::Create();
  green_quad2->SetNew(top_right_green_shared_quad_state,
                      green_rect2,
                      gfx::Rect(),
                      green_rect2,
                      gfx::RectF(green_rect2.size()),
                      green_rect2.size(),
                      texture_format,
                      green_rect2,
                      1.f,
                      PicturePileImpl::CreateFromOther(green_pile));
  pass->quad_list.push_back(green_quad2.PassAs<DrawQuad>());

  // Add a green clipped checkerboard in the bottom right to help test
  // interleaving picture quad content and solid color content.
  gfx::Rect bottom_right_rect(
      gfx::Point(viewport.width() / 2, viewport.height() / 2),
      gfx::Size(viewport.width() / 2, viewport.height() / 2));
  SharedQuadState* bottom_right_green_shared_state =
      CreateTestSharedQuadStateClipped(pass.get(),
                                       green_content_to_target_transform,
                                       viewport,
                                       bottom_right_rect);
  scoped_ptr<SolidColorDrawQuad> bottom_right_color_quad =
      SolidColorDrawQuad::Create();
  bottom_right_color_quad->SetNew(bottom_right_green_shared_state,
                                  viewport,
                                  viewport,
                                  SK_ColorGREEN,
                                  false);
  pass->quad_list.push_back(bottom_right_color_quad.PassAs<DrawQuad>());

  // Add two blue checkerboards taking up the bottom left and top right,
  // but use content scales as content rects to make this happen.
  // The content is at a 4x content scale.
  gfx::Rect layer_rect(gfx::Size(20, 30));
  float contents_scale = 4.f;
  // Two rects that touch at their corners, arbitrarily placed in the layer.
  gfx::RectF blue_layer_rect1(gfx::PointF(5.5f, 9.0f), gfx::SizeF(2.5f, 2.5f));
  gfx::RectF blue_layer_rect2(gfx::PointF(8.0f, 6.5f), gfx::SizeF(2.5f, 2.5f));
  gfx::RectF union_layer_rect = blue_layer_rect1;
  union_layer_rect.Union(blue_layer_rect2);

  // Because scaling up will cause sampling outside the rects, add one extra
  // pixel of buffer at the final content scale.
  float inset = -1.f / contents_scale;
  blue_layer_rect1.Inset(inset, inset, inset, inset);
  blue_layer_rect2.Inset(inset, inset, inset, inset);

  scoped_refptr<FakePicturePileImpl> pile =
      FakePicturePileImpl::CreateFilledPile(pile_tile_size, layer_rect.size());

  Region outside(layer_rect);
  outside.Subtract(gfx::ToEnclosingRect(union_layer_rect));
  for (Region::Iterator iter(outside); iter.has_rect(); iter.next()) {
    pile->add_draw_rect_with_paint(iter.rect(), red_paint);
  }

  SkPaint blue_paint;
  blue_paint.setColor(SK_ColorBLUE);
  pile->add_draw_rect_with_paint(blue_layer_rect1, blue_paint);
  pile->add_draw_rect_with_paint(blue_layer_rect2, blue_paint);
  pile->RerecordPile();

  gfx::Rect content_rect(
      gfx::ScaleToEnclosingRect(layer_rect, contents_scale));
  gfx::Rect content_union_rect(
      gfx::ToEnclosingRect(gfx::ScaleRect(union_layer_rect, contents_scale)));

  // At a scale of 4x the rectangles with a width of 2.5 will take up 10 pixels,
  // so scale an additional 10x to make them 100x100.
  gfx::Transform content_to_target_transform;
  content_to_target_transform.Scale(10.0, 10.0);
  gfx::Rect quad_content_rect(gfx::Size(20, 20));
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      pass.get(), content_to_target_transform, quad_content_rect);

  scoped_ptr<PictureDrawQuad> blue_quad = PictureDrawQuad::Create();
  blue_quad->SetNew(blue_shared_state,
                    quad_content_rect,
                    gfx::Rect(),
                    quad_content_rect,
                    gfx::RectF(quad_content_rect),
                    content_union_rect.size(),
                    texture_format,
                    content_union_rect,
                    contents_scale,
                    PicturePileImpl::CreateFromOther(pile));
  pass->quad_list.push_back(blue_quad.PassAs<DrawQuad>());

  // Fill left half of viewport with green.
  gfx::Transform half_green_content_to_target_transform;
  gfx::Rect half_green_rect(gfx::Size(viewport.width() / 2, viewport.height()));
  SharedQuadState* half_green_shared_state = CreateTestSharedQuadState(
      pass.get(), half_green_content_to_target_transform, half_green_rect);
  scoped_ptr<SolidColorDrawQuad> half_color_quad = SolidColorDrawQuad::Create();
  half_color_quad->SetNew(half_green_shared_state,
                          half_green_rect,
                          half_green_rect,
                          SK_ColorGREEN,
                          false);
  pass->quad_list.push_back(half_color_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, WrapModeRepeat) {
  gfx::Rect rect(this->device_viewport_size_);

  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(pass.get(), gfx::Transform(), rect);

  gfx::Rect texture_rect(4, 4);
  SkPMColor colors[4] = {
    SkPreMultiplyColor(SkColorSetARGB(255, 0, 255, 0)),
    SkPreMultiplyColor(SkColorSetARGB(255, 0, 128, 0)),
    SkPreMultiplyColor(SkColorSetARGB(255, 0,  64, 0)),
    SkPreMultiplyColor(SkColorSetARGB(255, 0,   0, 0)),
  };
  uint32_t pixels[16] = {
    colors[0], colors[0], colors[1], colors[1],
    colors[0], colors[0], colors[1], colors[1],
    colors[2], colors[2], colors[3], colors[3],
    colors[2], colors[2], colors[3], colors[3],
  };
  ResourceProvider::ResourceId resource =
      this->resource_provider_->CreateResource(
          texture_rect.size(),
          GL_REPEAT,
          ResourceProvider::TextureUsageAny,
          RGBA_8888);
  this->resource_provider_->SetPixels(
      resource,
      reinterpret_cast<uint8_t*>(pixels),
      texture_rect,
      texture_rect,
      gfx::Vector2d());

  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  scoped_ptr<TextureDrawQuad> texture_quad = TextureDrawQuad::Create();
  texture_quad->SetNew(
      shared_state,
      gfx::Rect(this->device_viewport_size_),
      gfx::Rect(),
      gfx::Rect(this->device_viewport_size_),
      resource,
      true,                     // premultiplied_alpha
      gfx::PointF(0.0f, 0.0f),  // uv_top_left
      gfx::PointF(              // uv_bottom_right
          this->device_viewport_size_.width() / texture_rect.width(),
          this->device_viewport_size_.height() / texture_rect.height()),
      SK_ColorWHITE,
      vertex_opacity,
      false);  // flipped
  pass->quad_list.push_back(texture_quad.PassAs<DrawQuad>());

  RenderPassList pass_list;
  pass_list.push_back(pass.Pass());

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("wrap_mode_repeat.png")),
      FuzzyPixelOffByOneComparator(true)));
}

#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace cc
