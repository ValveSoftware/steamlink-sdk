// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android_video_decode_accelerator.h"

#include <stdint.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/android/media_jni_registrar.h"
#include "media/gpu/android_video_decode_accelerator.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace {

bool MakeContextCurrent() {
  return true;
}

base::WeakPtr<gpu::gles2::GLES2Decoder> GetGLES2Decoder(
    const base::WeakPtr<gpu::gles2::GLES2Decoder>& decoder) {
  return decoder;
}

}  // namespace

namespace media {

class MockVideoDecodeAcceleratorClient : public VideoDecodeAccelerator::Client {
 public:
  MockVideoDecodeAcceleratorClient() {}
  ~MockVideoDecodeAcceleratorClient() override {}

  // VideoDecodeAccelerator::Client implementation.
  void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                             VideoPixelFormat format,
                             uint32_t textures_per_buffer,
                             const gfx::Size& dimensions,
                             uint32_t texture_target) override {}
  void DismissPictureBuffer(int32_t picture_buffer_id) override {}
  void PictureReady(const Picture& picture) override {}
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) override {}
  void NotifyFlushDone() override {}
  void NotifyResetDone() override {}
  void NotifyError(VideoDecodeAccelerator::Error error) override {}
};

class AndroidVideoDecodeAcceleratorTest : public testing::Test {
 public:
  ~AndroidVideoDecodeAcceleratorTest() override {}

 protected:
  void SetUp() override {
    JNIEnv* env = base::android::AttachCurrentThread();
    RegisterJni(env);

    gl::init::ClearGLBindings();
    ASSERT_TRUE(gl::init::InitializeGLOneOff());
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size(1024, 1024));
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    context_->MakeCurrent(surface_.get());

    // Start a message loop because AVDA starts a timer task.
    message_loop_.reset(new base::MessageLoop());
    gl_decoder_.reset(new testing::NiceMock<gpu::gles2::MockGLES2Decoder>());
    client_.reset(new MockVideoDecodeAcceleratorClient());

    vda_.reset(new AndroidVideoDecodeAccelerator(
        base::Bind(&MakeContextCurrent),
        base::Bind(&GetGLES2Decoder, gl_decoder_->AsWeakPtr())));
  }

  bool Initialize(VideoCodecProfile profile) {
    return vda_->Initialize(VideoDecodeAccelerator::Config(profile),
                            client_.get());
  }

 private:
  std::unique_ptr<base::MessageLoop> message_loop_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  std::unique_ptr<gpu::gles2::MockGLES2Decoder> gl_decoder_;
  std::unique_ptr<MockVideoDecodeAcceleratorClient> client_;

  // This must be a unique pointer to a VDA and not an AVDA to ensure the
  // the default_delete specialization that calls Destroy() will be used.
  std::unique_ptr<VideoDecodeAccelerator> vda_;
};

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureUnsupportedCodec) {
  ASSERT_FALSE(Initialize(VIDEO_CODEC_PROFILE_UNKNOWN));
}

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureSupportedCodec) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return;
  // H264 is always supported by AVDA.
  ASSERT_TRUE(Initialize(H264PROFILE_BASELINE));
}

}  // namespace media
