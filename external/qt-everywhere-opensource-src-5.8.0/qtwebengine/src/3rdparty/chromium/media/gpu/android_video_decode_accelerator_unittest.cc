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
#include "media/gpu/android_copying_backing_strategy.h"
#include "media/gpu/android_video_decode_accelerator.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace {

bool MockMakeContextCurrent() {
  return true;
}

static base::WeakPtr<gpu::gles2::GLES2Decoder> MockGetGLES2Decoder(
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

    // Start message loop because
    // AndroidVideoDecodeAccelerator::ConfigureMediaCodec() starts a timer task.
    message_loop_.reset(new base::MessageLoop());

    std::unique_ptr<gpu::gles2::MockGLES2Decoder> decoder(
        new gpu::gles2::MockGLES2Decoder());
    std::unique_ptr<MockVideoDecodeAcceleratorClient> client(
        new MockVideoDecodeAcceleratorClient());
    accelerator_.reset(new AndroidVideoDecodeAccelerator(
        base::Bind(&MockMakeContextCurrent),
        base::Bind(&MockGetGLES2Decoder, decoder->AsWeakPtr())));
  }

  bool Configure(VideoCodec codec) {
    AndroidVideoDecodeAccelerator* accelerator =
        static_cast<AndroidVideoDecodeAccelerator*>(accelerator_.get());
    scoped_refptr<gl::SurfaceTexture> surface_texture =
        gl::SurfaceTexture::Create(0);
    accelerator->codec_config_->surface_ =
        gl::ScopedJavaSurface(surface_texture.get());
    accelerator->codec_config_->codec_ = codec;
    return accelerator->ConfigureMediaCodecSynchronously();
  }

 private:
  std::unique_ptr<VideoDecodeAccelerator> accelerator_;
  std::unique_ptr<base::MessageLoop> message_loop_;
};

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureUnsupportedCodec) {
  EXPECT_FALSE(Configure(kUnknownVideoCodec));
}

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureSupportedCodec) {
  if (!MediaCodecUtil::IsMediaCodecAvailable())
    return;
  EXPECT_TRUE(Configure(kCodecVP8));
}

}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
