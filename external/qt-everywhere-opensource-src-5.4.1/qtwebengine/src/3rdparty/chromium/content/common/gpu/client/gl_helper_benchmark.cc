// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file looks like a unit test, but it contains benchmarks and test
// utilities intended for manual evaluation of the scalers in
// gl_helper*. These tests produce output in the form of files and printouts,
// but cannot really "fail". There is no point in making these tests part
// of any test automation run.

#include <stdio.h>
#include <cmath>
#include <string>
#include <vector>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/common/gpu/client/gl_helper.h"
#include "content/common/gpu/client/gl_helper_scaling.h"
#include "content/public/test/unittest_test_suite.h"
#include "content/test/content_test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gl/gl_surface.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace content {

using blink::WebGLId;
using blink::WebGraphicsContext3D;

content::GLHelper::ScalerQuality kQualities[] = {
  content::GLHelper::SCALER_QUALITY_BEST,
  content::GLHelper::SCALER_QUALITY_GOOD,
  content::GLHelper::SCALER_QUALITY_FAST,
};

const char *kQualityNames[] = {
  "best",
  "good",
  "fast",
};

class GLHelperTest : public testing::Test {
 protected:
  virtual void SetUp() {
    WebGraphicsContext3D::Attributes attributes;
    bool lose_context_when_out_of_memory = false;
    context_ = webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl::
        CreateOffscreenContext(attributes, lose_context_when_out_of_memory);
    context_->makeContextCurrent();

    helper_.reset(
        new content::GLHelper(context_->GetGLInterface(),
                              context_->GetContextSupport()));
    helper_scaling_.reset(new content::GLHelperScaling(
        context_->GetGLInterface(),
        helper_.get()));
  }

  virtual void TearDown() {
    helper_scaling_.reset(NULL);
    helper_.reset(NULL);
    context_.reset(NULL);
  }


  void LoadPngFileToSkBitmap(const base::FilePath& filename,
                             SkBitmap* bitmap) {
    std::string compressed;
    base::ReadFileToString(base::MakeAbsoluteFilePath(filename), &compressed);
    ASSERT_TRUE(compressed.size());
    ASSERT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(compressed.data()),
        compressed.size(), bitmap));
  }

  // Save the image to a png file. Used to create the initial test files.
  void SaveToFile(SkBitmap* bitmap, const base::FilePath& filename) {
    std::vector<unsigned char> compressed;
    ASSERT_TRUE(gfx::PNGCodec::Encode(
        static_cast<unsigned char*>(bitmap->getPixels()),
        gfx::PNGCodec::FORMAT_BGRA,
        gfx::Size(bitmap->width(), bitmap->height()),
        static_cast<int>(bitmap->rowBytes()),
        true,
        std::vector<gfx::PNGCodec::Comment>(),
        &compressed));
    ASSERT_TRUE(compressed.size());
    FILE* f = base::OpenFile(filename, "wb");
    ASSERT_TRUE(f);
    ASSERT_EQ(fwrite(&*compressed.begin(), 1, compressed.size(), f),
              compressed.size());
    base::CloseFile(f);
  }

  scoped_ptr<webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl>
      context_;
  scoped_ptr<content::GLHelper> helper_;
  scoped_ptr<content::GLHelperScaling> helper_scaling_;
  std::deque<GLHelperScaling::ScaleOp> x_ops_, y_ops_;
};


TEST_F(GLHelperTest, ScaleBenchmark) {
  int output_sizes[] = { 1920, 1080,
                         1249, 720,  // Output size on pixel
                         256, 144 };
  int input_sizes[] = { 3200, 2040,
                        2560, 1476,  // Pixel tab size
                        1920, 1080,
                        1280, 720,
                        800, 480,
                        256, 144 };

  for (size_t q = 0; q < arraysize(kQualities); q++) {
    for (size_t outsize = 0;
         outsize < arraysize(output_sizes);
         outsize += 2) {
      for (size_t insize = 0;
           insize < arraysize(input_sizes);
           insize += 2) {
        WebGLId src_texture = context_->createTexture();
        WebGLId dst_texture = context_->createTexture();
        WebGLId framebuffer = context_->createFramebuffer();
        const gfx::Size src_size(input_sizes[insize],
                                 input_sizes[insize + 1]);
        const gfx::Size dst_size(output_sizes[outsize],
                                 output_sizes[outsize + 1]);
        SkBitmap input;
        input.setConfig(SkBitmap::kARGB_8888_Config,
                        src_size.width(),
                        src_size.height());
        input.allocPixels();
        SkAutoLockPixels lock(input);

        SkBitmap output_pixels;
        input.setConfig(SkBitmap::kARGB_8888_Config,
                        dst_size.width(),
                        dst_size.height());
        output_pixels.allocPixels();
        SkAutoLockPixels output_lock(output_pixels);

        context_->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        context_->bindTexture(GL_TEXTURE_2D, dst_texture);
        context_->texImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA,
                             dst_size.width(),
                             dst_size.height(),
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             0);
        context_->bindTexture(GL_TEXTURE_2D, src_texture);
        context_->texImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RGBA,
                             src_size.width(),
                             src_size.height(),
                             0,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             input.getPixels());

        gfx::Rect src_subrect(0, 0,
                              src_size.width(), src_size.height());
        scoped_ptr<content::GLHelper::ScalerInterface> scaler(
          helper_->CreateScaler(kQualities[q],
                                src_size,
                                src_subrect,
                                dst_size,
                                false,
                                false));
        // Scale once beforehand before we start measuring.
        scaler->Scale(src_texture, dst_texture);
        context_->finish();

        base::TimeTicks start_time = base::TimeTicks::Now();
        int iterations = 0;
        base::TimeTicks end_time;
        while (true) {
          for (int i = 0; i < 50; i++) {
            iterations++;
            scaler->Scale(src_texture, dst_texture);
            context_->flush();
          }
          context_->finish();
          end_time = base::TimeTicks::Now();
          if (iterations > 2000) {
            break;
          }
          if ((end_time - start_time).InMillisecondsF() > 1000) {
            break;
          }
        }
        context_->deleteTexture(dst_texture);
        context_->deleteTexture(src_texture);
        context_->deleteFramebuffer(framebuffer);

        std::string name;
        name = base::StringPrintf("scale_%dx%d_to_%dx%d_%s",
                                  src_size.width(),
                                  src_size.height(),
                                  dst_size.width(),
                                  dst_size.height(),
                                  kQualityNames[q]);

        float ms = (end_time - start_time).InMillisecondsF() / iterations;
        printf("*RESULT gpu_scale_time: %s=%.2f ms\n", name.c_str(), ms);
      }
    }
  }
}

// This is more of a test utility than a test.
// Put an PNG image called "testimage.png" in your
// current directory, then run this test. It will
// create testoutput_Q_P.png, where Q is the scaling
// mode and P is the scaling percentage taken from
// the table below.
TEST_F(GLHelperTest, DISABLED_ScaleTestImage) {
  int percents[] = {
    230,
    180,
    150,
    110,
    90,
    70,
    50,
    49,
    40,
    20,
    10,
  };

  SkBitmap input;
  LoadPngFileToSkBitmap(base::FilePath(
      FILE_PATH_LITERAL("testimage.png")), &input);

  WebGLId framebuffer = context_->createFramebuffer();
  WebGLId src_texture = context_->createTexture();
  const gfx::Size src_size(input.width(), input.height());
  context_->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  context_->bindTexture(GL_TEXTURE_2D, src_texture);
  context_->texImage2D(GL_TEXTURE_2D,
                       0,
                       GL_RGBA,
                       src_size.width(),
                       src_size.height(),
                       0,
                       GL_RGBA,
                       GL_UNSIGNED_BYTE,
                       input.getPixels());

  for (size_t q = 0; q < arraysize(kQualities); q++) {
    for (size_t p = 0; p < arraysize(percents); p++) {
      const gfx::Size dst_size(input.width() * percents[p] / 100,
                               input.height() * percents[p] / 100);
      WebGLId dst_texture = helper_->CopyAndScaleTexture(
        src_texture,
        src_size,
        dst_size,
        false,
        kQualities[q]);

      SkBitmap output_pixels;
      input.setConfig(SkBitmap::kARGB_8888_Config,
                      dst_size.width(),
                      dst_size.height());
      output_pixels.allocPixels();
      SkAutoLockPixels lock(output_pixels);

      helper_->ReadbackTextureSync(
          dst_texture,
          gfx::Rect(0, 0,
                    dst_size.width(),
                    dst_size.height()),
          static_cast<unsigned char *>(output_pixels.getPixels()),
          SkBitmap::kARGB_8888_Config);
      context_->deleteTexture(dst_texture);
      std::string filename = base::StringPrintf("testoutput_%s_%d.ppm",
                                                kQualityNames[q],
                                                percents[p]);
      VLOG(0) << "Writing " <<  filename;
      SaveToFile(&output_pixels, base::FilePath::FromUTF8Unsafe(filename));
    }
  }
  context_->deleteTexture(src_texture);
  context_->deleteFramebuffer(framebuffer);
}

}  // namespace

// These tests needs to run against a proper GL environment, so we
// need to set it up before we can run the tests.
int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  base::TestSuite* suite = new content::ContentTestSuite(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  gfx::GLSurface::InitializeOneOff();

  return content::UnitTestTestSuite(suite).Run();
}
