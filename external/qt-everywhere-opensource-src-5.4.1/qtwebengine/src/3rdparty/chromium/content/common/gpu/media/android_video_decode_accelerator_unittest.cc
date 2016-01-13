// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/android_video_decode_accelerator.h"

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/gpu/media/android_video_decode_accelerator.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_jni_registrar.h"
#include "media/video/picture.h"
#include "media/video/video_decode_accelerator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/android/surface_texture.h"

namespace {

bool MockMakeContextCurrent() {
  return true;
}

}  // namespace

namespace content {

// TODO(felipeg): Add more unit tests to test the ordinary behavior of
// AndroidVideoDecodeAccelerator.
// http://crbug.com/178647
class MockVideoDecodeAcceleratorClient
    : public media::VideoDecodeAccelerator::Client {
 public:
  MockVideoDecodeAcceleratorClient() {};
  virtual ~MockVideoDecodeAcceleratorClient() {};

  // VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(uint32 requested_num_of_buffers,
                                     const gfx::Size& dimensions,
                                     uint32 texture_target) OVERRIDE {};
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE {};
  virtual void PictureReady(const media::Picture& picture) OVERRIDE {};
  virtual void NotifyEndOfBitstreamBuffer(
      int32 bitstream_buffer_id) OVERRIDE {};
  virtual void NotifyFlushDone() OVERRIDE {};
  virtual void NotifyResetDone() OVERRIDE {};
  virtual void NotifyError(
      media::VideoDecodeAccelerator::Error error) OVERRIDE {};
};

class AndroidVideoDecodeAcceleratorTest : public testing::Test {
 public:
  virtual ~AndroidVideoDecodeAcceleratorTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    JNIEnv* env = base::android::AttachCurrentThread();
    media::RegisterJni(env);
    // TODO(felipeg): fix GL bindings, so that the decoder can perform GL
    // calls.
    scoped_ptr<gpu::gles2::MockGLES2Decoder> decoder(
        new gpu::gles2::MockGLES2Decoder());
    scoped_ptr<MockVideoDecodeAcceleratorClient> client(
        new MockVideoDecodeAcceleratorClient());
    accelerator_.reset(new AndroidVideoDecodeAccelerator(
        decoder->AsWeakPtr(), base::Bind(&MockMakeContextCurrent)));
  }

  bool Configure(media::VideoCodec codec) {
    AndroidVideoDecodeAccelerator* accelerator =
        static_cast<AndroidVideoDecodeAccelerator*>(accelerator_.get());
    accelerator->surface_texture_ = gfx::SurfaceTexture::Create(0);
    accelerator->codec_ = codec;
    return accelerator->ConfigureMediaCodec();
  }

 private:
  scoped_ptr<media::VideoDecodeAccelerator> accelerator_;
};

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureUnsupportedCodec) {
  EXPECT_FALSE(Configure(media::kUnknownVideoCodec));
}

TEST_F(AndroidVideoDecodeAcceleratorTest, ConfigureSupportedCodec) {
  if (!media::MediaCodecBridge::IsAvailable())
    return;
  EXPECT_TRUE(Configure(media::kCodecVP8));
}

}  // namespace content

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
