// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositing_iosurface_transformer_mac.h"

#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/compositing_iosurface_shader_programs_mac.h"
#include "media/base/yuv_convert.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/rect.h"

namespace content {

#define EXPECT_NO_GL_ERROR(stmt)  \
  do {  \
    stmt;  \
    const GLenum error_code = glGetError();  \
    EXPECT_TRUE(GL_NO_ERROR == error_code)  \
        << "for error code " << error_code  \
        << ": " << gluErrorString(error_code);  \
  } while(0)

namespace {

const GLenum kGLTextureTarget = GL_TEXTURE_RECTANGLE_ARB;

enum RendererRestriction {
  RESTRICTION_NONE,
  RESTRICTION_SOFTWARE_ONLY,
  RESTRICTION_HARDWARE_ONLY
};

bool InitializeGLContext(CGLContextObj* context,
                         RendererRestriction restriction) {
  std::vector<CGLPixelFormatAttribute> attribs;
  // Select off-screen renderers only.
  attribs.push_back(kCGLPFAOffScreen);
  // By default, the library will prefer hardware-accelerated renderers, but
  // falls back on the software ones if necessary.  However, there are use cases
  // where we want to force a restriction (e.g., benchmarking performance).
  if (restriction == RESTRICTION_SOFTWARE_ONLY) {
    attribs.push_back(kCGLPFARendererID);
    attribs.push_back(static_cast<CGLPixelFormatAttribute>(
        kCGLRendererGenericFloatID));
  } else if (restriction == RESTRICTION_HARDWARE_ONLY) {
    attribs.push_back(kCGLPFAAccelerated);
  }
  attribs.push_back(static_cast<CGLPixelFormatAttribute>(0));

  CGLPixelFormatObj format;
  GLint num_pixel_formats = 0;
  bool success = true;
  if (CGLChoosePixelFormat(&attribs.front(), &format, &num_pixel_formats) !=
          kCGLNoError) {
    LOG(ERROR) << "Error choosing pixel format.";
    success = false;
  }
  if (success && num_pixel_formats <= 0) {
    LOG(ERROR) << "num_pixel_formats <= 0; actual value is "
               << num_pixel_formats;
    success = false;
  }
  if (success && CGLCreateContext(format, NULL, context) != kCGLNoError) {
    LOG(ERROR) << "Error creating context.";
    success = false;
  }
  CGLDestroyPixelFormat(format);
  return success;
}

// Returns a decent test pattern for testing all of: 1) orientation, 2) scaling,
// 3) color space conversion (e.g., 4 pixels --> one U or V pixel), and 4)
// texture alignment/processing.  Example 32x32 bitmap:
//
// GGGGGGGGGGGGGGGGRRBBRRBBRRBBRRBB
// GGGGGGGGGGGGGGGGRRBBRRBBRRBBRRBB
// GGGGGGGGGGGGGGGGYYCCYYCCYYCCYYCC
// GGGGGGGGGGGGGGGGYYCCYYCCYYCCYYCC
// GGGGGGGGGGGGGGGGRRBBRRBBRRBBRRBB
// GGGGGGGGGGGGGGGGRRBBRRBBRRBBRRBB
// GGGGGGGGGGGGGGGGYYCCYYCCYYCCYYCC
// GGGGGGGGGGGGGGGGYYCCYYCCYYCCYYCC
// RRBBRRBBRRBBRRBBRRBBRRBBRRBBRRBB
// RRBBRRBBRRBBRRBBRRBBRRBBRRBBRRBB
// YYCCYYCCYYCCYYCCYYCCYYCCYYCCYYCC
// YYCCYYCCYYCCYYCCYYCCYYCCYYCCYYCC
// RRBBRRBBRRBBRRBBRRBBRRBBRRBBRRBB
// RRBBRRBBRRBBRRBBRRBBRRBBRRBBRRBB
// YYCCYYCCYYCCYYCCYYCCYYCCYYCCYYCC
// YYCCYYCCYYCCYYCCYYCCYYCCYYCCYYCC
//
// Key: G = Gray, R = Red, B = Blue, Y = Yellow, C = Cyan
SkBitmap GenerateTestPatternBitmap(const gfx::Size& size) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  CHECK(bitmap.allocPixels());
  SkAutoLockPixels lock_bitmap(bitmap);
  bitmap.eraseColor(SK_ColorGRAY);
  for (int y = 0; y < size.height(); ++y) {
    uint32_t* p = bitmap.getAddr32(0, y);
    for (int x = 0; x < size.width(); ++x, ++p) {
      if ((x < (size.width() / 2)) && (y < (size.height() / 2)))
        continue;  // Leave upper-left quadrant gray.
      *p = SkColorSetARGB(255,
                          x % 4 < 2 ? 255 : 0,
                          y % 4 < 2 ? 255 : 0,
                          x % 4 < 2 ? 0 : 255);
    }
  }
  return bitmap;
}

// Creates a new texture consisting of the given |bitmap|.
GLuint CreateTextureWithImage(const SkBitmap& bitmap) {
  GLuint texture;
  EXPECT_NO_GL_ERROR(glGenTextures(1, &texture));
  EXPECT_NO_GL_ERROR(glBindTexture(kGLTextureTarget, texture));
  {
    SkAutoLockPixels lock_bitmap(bitmap);
    EXPECT_NO_GL_ERROR(glTexImage2D(
        kGLTextureTarget, 0, GL_RGBA, bitmap.width(), bitmap.height(), 0,
        GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, bitmap.getPixels()));
  }
  glBindTexture(kGLTextureTarget, 0);
  return texture;
}

// Read back a texture from the GPU, returning the image data as an SkBitmap.
SkBitmap ReadBackTexture(GLuint texture, const gfx::Size& size, GLenum format) {
  SkBitmap result;
  result.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  CHECK(result.allocPixels());

  GLuint frame_buffer;
  EXPECT_NO_GL_ERROR(glGenFramebuffersEXT(1, &frame_buffer));
  EXPECT_NO_GL_ERROR(
      glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, frame_buffer));
  EXPECT_NO_GL_ERROR(glFramebufferTexture2DEXT(
      GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, kGLTextureTarget,
      texture, 0));
  DCHECK(glCheckFramebufferStatusEXT(GL_READ_FRAMEBUFFER_EXT) ==
             GL_FRAMEBUFFER_COMPLETE_EXT);

  {
    SkAutoLockPixels lock_result(result);
    EXPECT_NO_GL_ERROR(glReadPixels(
        0, 0, size.width(), size.height(), format, GL_UNSIGNED_INT_8_8_8_8_REV,
        result.getPixels()));
  }

  EXPECT_NO_GL_ERROR(glDeleteFramebuffersEXT(1, &frame_buffer));

  return result;
}

// Returns the |src_rect| region of |src| scaled to |to_size| by drawing on a
// Skia canvas, and using bilinear filtering (just like a GPU would).
SkBitmap ScaleBitmapWithSkia(const SkBitmap& src,
                             const gfx::Rect& src_rect,
                             const gfx::Size& to_size) {
  SkBitmap cropped_src;
  if (src_rect == gfx::Rect(0, 0, src.width(), src.height())) {
    cropped_src = src;
  } else {
    CHECK(src.extractSubset(
        &cropped_src,
        SkIRect::MakeXYWH(src_rect.x(), src_rect.y(),
                          src_rect.width(), src_rect.height())));
  }

  SkBitmap result;
  result.setConfig(cropped_src.config(), to_size.width(), to_size.height());
  CHECK(result.allocPixels());

  SkCanvas canvas(result);
  canvas.scale(static_cast<double>(result.width()) / cropped_src.width(),
               static_cast<double>(result.height()) / cropped_src.height());
  SkPaint paint;
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);  // Use bilinear filtering.
  canvas.drawBitmap(cropped_src, 0, 0, &paint);

  return result;
}

// The maximum value by which a pixel value may deviate from the expected value
// before considering it "significantly different."  This is meant to account
// for the slight differences in filtering techniques used between the various
// GPUs and software implementations.
const int kDifferenceThreshold = 16;

// Returns the number of pixels significantly different between |expected| and
// |actual|.
int ImageDifference(const SkBitmap& expected, const SkBitmap& actual) {
  SkAutoLockPixels lock_expected(expected);
  SkAutoLockPixels lock_actual(actual);

  // Sanity-check assumed image properties.
  DCHECK_EQ(expected.width(), actual.width());
  DCHECK_EQ(expected.height(), actual.height());
  DCHECK_EQ(SkBitmap::kARGB_8888_Config, expected.config());
  DCHECK_EQ(SkBitmap::kARGB_8888_Config, actual.config());

  // Compare both images.
  int num_pixels_different = 0;
  for (int y = 0; y < expected.height(); ++y) {
    const uint32_t* p = expected.getAddr32(0, y);
    const uint32_t* q = actual.getAddr32(0, y);
    for (int x = 0; x < expected.width(); ++x, ++p, ++q) {
      if (abs(static_cast<int>(SkColorGetR(*p)) -
              static_cast<int>(SkColorGetR(*q))) > kDifferenceThreshold ||
          abs(static_cast<int>(SkColorGetG(*p)) -
              static_cast<int>(SkColorGetG(*q))) > kDifferenceThreshold ||
          abs(static_cast<int>(SkColorGetB(*p)) -
              static_cast<int>(SkColorGetB(*q))) > kDifferenceThreshold) {
        ++num_pixels_different;
      }
    }
  }

  return num_pixels_different;
}

// Returns the number of pixels significantly different between |expected| and
// |actual|.  It is understood that |actual| contains 4-byte quads, and so we
// may need to be ignoring a mod-4 number of pixels at the end of each of its
// rows.
int ImagePlaneDifference(const uint8* expected, const SkBitmap& actual,
                         const gfx::Size& dst_size) {
  SkAutoLockPixels actual_lock(actual);

  int num_pixels_different = 0;
  for (int y = 0; y < dst_size.height(); ++y) {
    const uint8* p = expected + y * dst_size.width();
    const uint8* const p_end = p + dst_size.width();
    const uint8* q =
        reinterpret_cast<uint8*>(actual.getPixels()) + y * actual.rowBytes();
    for (; p < p_end; ++p, ++q) {
      if (abs(static_cast<int>(*p) - static_cast<int>(*q)) >
              kDifferenceThreshold) {
        ++num_pixels_different;
      }
    }
  }

  return num_pixels_different;
}

}  // namespace

// Note: All tests fixtures operate within an off-screen OpenGL context.
class CompositingIOSurfaceTransformerTest : public testing::Test {
 public:
  CompositingIOSurfaceTransformerTest() {
    // TODO(miu): Try to use RESTRICTION_NONE to speed up the execution time of
    // unit tests, once it's established that the trybots and buildbots behave
    // well when using the GPU.
    CHECK(InitializeGLContext(&context_, RESTRICTION_SOFTWARE_ONLY));
    CGLSetCurrentContext(context_);
    shader_program_cache_.reset(new CompositingIOSurfaceShaderPrograms());
    transformer_.reset(new CompositingIOSurfaceTransformer(
        kGLTextureTarget, false, shader_program_cache_.get()));
  }

  virtual ~CompositingIOSurfaceTransformerTest() {
    transformer_->ReleaseCachedGLObjects();
    shader_program_cache_->Reset();
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(context_);
  }

 protected:
  void RunResizeTest(const SkBitmap& src_bitmap, const gfx::Rect& src_rect,
                     const gfx::Size& dst_size) {
    SCOPED_TRACE(::testing::Message()
                 << "src_rect=(" << src_rect.x() << ',' << src_rect.y()
                 << ")x[" << src_rect.width() << 'x' << src_rect.height()
                 << "]; dst_size=[" << dst_size.width() << 'x'
                 << dst_size.height() << ']');

    // Do the scale operation on the GPU.
    const GLuint original_texture = CreateTextureWithImage(src_bitmap);
    ASSERT_NE(0u, original_texture);
    GLuint scaled_texture = 0u;
    ASSERT_TRUE(transformer_->ResizeBilinear(
        original_texture, src_rect, dst_size, &scaled_texture));
    EXPECT_NE(0u, scaled_texture);
    CGLFlushDrawable(context_);  // Account for some buggy driver impls.
    const SkBitmap result_bitmap =
        ReadBackTexture(scaled_texture, dst_size, GL_BGRA);
    EXPECT_NO_GL_ERROR(glDeleteTextures(1, &original_texture));

    // Compare the image read back to the version produced by a known-working
    // software implementation.  Allow up to 2 lines of mismatch due to how
    // implementations disagree on resolving the processing of edges.
    const SkBitmap expected_bitmap =
        ScaleBitmapWithSkia(src_bitmap, src_rect, dst_size);
    EXPECT_GE(std::max(expected_bitmap.width(), expected_bitmap.height()) * 2,
              ImageDifference(expected_bitmap, result_bitmap));
  }

  void RunTransformRGBToYV12Test(
      const SkBitmap& src_bitmap, const gfx::Rect& src_rect,
      const gfx::Size& dst_size) {
    SCOPED_TRACE(::testing::Message()
                 << "src_rect=(" << src_rect.x() << ',' << src_rect.y()
                 << ")x[" << src_rect.width() << 'x' << src_rect.height()
                 << "]; dst_size=[" << dst_size.width() << 'x'
                 << dst_size.height() << ']');

    // Perform the RGB to YV12 conversion.
    const GLuint original_texture = CreateTextureWithImage(src_bitmap);
    ASSERT_NE(0u, original_texture);
    GLuint texture_y = 0u;
    GLuint texture_u = 0u;
    GLuint texture_v = 0u;
    gfx::Size packed_y_size;
    gfx::Size packed_uv_size;
    ASSERT_TRUE(transformer_->TransformRGBToYV12(
        original_texture, src_rect, dst_size,
        &texture_y, &texture_u, &texture_v, &packed_y_size, &packed_uv_size));
    EXPECT_NE(0u, texture_y);
    EXPECT_NE(0u, texture_u);
    EXPECT_NE(0u, texture_v);
    EXPECT_FALSE(packed_y_size.IsEmpty());
    EXPECT_FALSE(packed_uv_size.IsEmpty());
    EXPECT_NO_GL_ERROR(glDeleteTextures(1, &original_texture));

    // Read-back the texture for each plane.
    CGLFlushDrawable(context_);  // Account for some buggy driver impls.
    const GLenum format = shader_program_cache_->rgb_to_yv12_output_format();
    const SkBitmap result_y_bitmap =
        ReadBackTexture(texture_y, packed_y_size, format);
    const SkBitmap result_u_bitmap =
        ReadBackTexture(texture_u, packed_uv_size, format);
    const SkBitmap result_v_bitmap =
        ReadBackTexture(texture_v, packed_uv_size, format);

    // Compare the Y, U, and V planes read-back to the version produced by a
    // known-working software implementation.  Allow up to 2 lines of mismatch
    // due to how implementations disagree on resolving the processing of edges.
    const SkBitmap expected_bitmap =
        ScaleBitmapWithSkia(src_bitmap, src_rect, dst_size);
    const gfx::Size dst_uv_size(
        (dst_size.width() + 1) / 2, (dst_size.height() + 1) / 2);
    scoped_ptr<uint8[]> expected_y_plane(
        new uint8[dst_size.width() * dst_size.height()]);
    scoped_ptr<uint8[]> expected_u_plane(
        new uint8[dst_uv_size.width() * dst_uv_size.height()]);
    scoped_ptr<uint8[]> expected_v_plane(
        new uint8[dst_uv_size.width() * dst_uv_size.height()]);
    {
      SkAutoLockPixels src_bitmap_lock(expected_bitmap);
      media::ConvertRGB32ToYUV(
          reinterpret_cast<const uint8*>(expected_bitmap.getPixels()),
          expected_y_plane.get(), expected_u_plane.get(),
          expected_v_plane.get(),
          expected_bitmap.width(), expected_bitmap.height(),
          expected_bitmap.rowBytes(),
          dst_size.width(), (dst_size.width() + 1) / 2);
    }
    EXPECT_GE(
        std::max(expected_bitmap.width(), expected_bitmap.height()) * 2,
        ImagePlaneDifference(expected_y_plane.get(), result_y_bitmap, dst_size))
        << " for RGB --> Y Plane";
    EXPECT_GE(
        std::max(expected_bitmap.width(), expected_bitmap.height()),
        ImagePlaneDifference(expected_u_plane.get(), result_u_bitmap,
                             dst_uv_size))
        << " for RGB --> U Plane";
    EXPECT_GE(
        std::max(expected_bitmap.width(), expected_bitmap.height()),
        ImagePlaneDifference(expected_v_plane.get(), result_v_bitmap,
                             dst_uv_size))
        << " for RGB --> V Plane";
  }

  CompositingIOSurfaceShaderPrograms* shader_program_cache() const {
    return shader_program_cache_.get();
  }

 private:
  CGLContextObj context_;
  scoped_ptr<CompositingIOSurfaceShaderPrograms> shader_program_cache_;
  scoped_ptr<CompositingIOSurfaceTransformer> transformer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositingIOSurfaceTransformerTest);
};

TEST_F(CompositingIOSurfaceTransformerTest, ShaderProgramsCompileAndLink) {
  // Attempt to use each program, binding its required uniform variables.
  EXPECT_NO_GL_ERROR(shader_program_cache()->UseBlitProgram());
  EXPECT_NO_GL_ERROR(shader_program_cache()->UseSolidWhiteProgram());
  EXPECT_NO_GL_ERROR(shader_program_cache()->UseRGBToYV12Program(1, 1.0f));
  EXPECT_NO_GL_ERROR(shader_program_cache()->UseRGBToYV12Program(2, 1.0f));

  EXPECT_NO_GL_ERROR(glUseProgram(0));
}

namespace {

const struct TestParameters {
  int src_width;
  int src_height;
  int scaled_width;
  int scaled_height;
} kTestParameters[] = {
  // Test 1:1 copies, but exposing varying pixel packing configurations.
  { 64, 64, 64, 64 },
  { 63, 63, 63, 63 },
  { 62, 62, 62, 62 },
  { 61, 61, 61, 61 },
  { 60, 60, 60, 60 },
  { 59, 59, 59, 59 },
  { 58, 58, 58, 58 },
  { 57, 57, 57, 57 },
  { 56, 56, 56, 56 },

  // Even-size, one or both dimensions upscaled.
  { 32, 32, 64, 32 }, { 32, 32, 32, 64 }, { 32, 32, 64, 64 },
  // Even-size, one or both dimensions downscaled by 2X.
  { 32, 32, 16, 32 }, { 32, 32, 32, 16 }, { 32, 32, 16, 16 },
  // Even-size, one or both dimensions downscaled by 1 pixel.
  { 32, 32, 31, 32 }, { 32, 32, 32, 31 }, { 32, 32, 31, 31 },
  // Even-size, one or both dimensions downscaled by 2 pixels.
  { 32, 32, 30, 32 }, { 32, 32, 32, 30 }, { 32, 32, 30, 30 },
  // Even-size, one or both dimensions downscaled by 3 pixels.
  { 32, 32, 29, 32 }, { 32, 32, 32, 29 }, { 32, 32, 29, 29 },

  // Odd-size, one or both dimensions upscaled.
  { 33, 33, 66, 33 }, { 33, 33, 33, 66 }, { 33, 33, 66, 66 },
  // Odd-size, one or both dimensions downscaled by 2X.
  { 33, 33, 16, 33 }, { 33, 33, 33, 16 }, { 33, 33, 16, 16 },
  // Odd-size, one or both dimensions downscaled by 1 pixel.
  { 33, 33, 32, 33 }, { 33, 33, 33, 32 }, { 33, 33, 32, 32 },
  // Odd-size, one or both dimensions downscaled by 2 pixels.
  { 33, 33, 31, 33 }, { 33, 33, 33, 31 }, { 33, 33, 31, 31 },
  // Odd-size, one or both dimensions downscaled by 3 pixels.
  { 33, 33, 30, 33 }, { 33, 33, 33, 30 }, { 33, 33, 30, 30 },
};

}  // namespace

TEST_F(CompositingIOSurfaceTransformerTest, ResizesTexturesCorrectly) {
  for (size_t i = 0; i < arraysize(kTestParameters); ++i) {
    SCOPED_TRACE(::testing::Message() << "kTestParameters[" << i << ']');

    const TestParameters& params = kTestParameters[i];
    const gfx::Size src_size(params.src_width, params.src_height);
    const gfx::Size dst_size(params.scaled_width, params.scaled_height);
    const SkBitmap src_bitmap = GenerateTestPatternBitmap(src_size);

    // Full texture resize test.
    RunResizeTest(src_bitmap, gfx::Rect(src_size), dst_size);
    // Subrect resize test: missing top row in source.
    RunResizeTest(src_bitmap,
                  gfx::Rect(0, 1, params.src_width, params.src_height - 1),
                  dst_size);
    // Subrect resize test: missing left column in source.
    RunResizeTest(src_bitmap,
                  gfx::Rect(1, 0, params.src_width - 1, params.src_height),
                  dst_size);
    // Subrect resize test: missing top+bottom rows, and left column in source.
    RunResizeTest(src_bitmap,
                  gfx::Rect(1, 1, params.src_width - 1, params.src_height - 2),
                  dst_size);
    // Subrect resize test: missing top row, and left+right columns in source.
    RunResizeTest(src_bitmap,
                  gfx::Rect(1, 1, params.src_width - 2, params.src_height - 1),
                  dst_size);
  }
}

TEST_F(CompositingIOSurfaceTransformerTest, TransformsRGBToYV12) {
  static const GLenum kOutputFormats[] = { GL_BGRA, GL_RGBA };

  for (size_t i = 0; i < arraysize(kOutputFormats); ++i) {
    SCOPED_TRACE(::testing::Message() << "kOutputFormats[" << i << ']');

    shader_program_cache()->SetOutputFormatForTesting(kOutputFormats[i]);

    for (size_t j = 0; j < arraysize(kTestParameters); ++j) {
      SCOPED_TRACE(::testing::Message() << "kTestParameters[" << j << ']');

      const TestParameters& params = kTestParameters[j];
      const gfx::Size src_size(params.src_width, params.src_height);
      const gfx::Size dst_size(params.scaled_width, params.scaled_height);
      const SkBitmap src_bitmap = GenerateTestPatternBitmap(src_size);

      // Full texture resize test.
      RunTransformRGBToYV12Test(src_bitmap, gfx::Rect(src_size), dst_size);
      // Subrect resize test: missing top row in source.
      RunTransformRGBToYV12Test(
          src_bitmap, gfx::Rect(0, 1, params.src_width, params.src_height - 1),
          dst_size);
      // Subrect resize test: missing left column in source.
      RunTransformRGBToYV12Test(
          src_bitmap, gfx::Rect(1, 0, params.src_width - 1, params.src_height),
          dst_size);
      // Subrect resize test: missing top+bottom rows, and left column in
      // source.
      RunTransformRGBToYV12Test(
          src_bitmap,
          gfx::Rect(1, 1, params.src_width - 1, params.src_height - 2),
          dst_size);
      // Subrect resize test: missing top row, and left+right columns in source.
      RunTransformRGBToYV12Test(
          src_bitmap,
          gfx::Rect(1, 1, params.src_width - 2, params.src_height - 1),
          dst_size);
    }
  }
}

}  // namespace content
