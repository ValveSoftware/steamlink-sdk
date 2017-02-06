// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/bind.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "media/base/video_frame.h"
#include "media/renderers/mock_gpu_video_accelerator_factories.h"
#include "media/video/gpu_memory_buffer_video_frame_pool.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

namespace {
class TestGLES2Interface : public gpu::gles2::GLES2InterfaceStub {
 public:
  unsigned gen_textures = 0u;
  void GenTextures(GLsizei n, GLuint* textures) override {
    DCHECK_EQ(1, n);
    *textures = ++gen_textures;
  }

  void ShallowFlushCHROMIUM() override {
    flushed_fence_sync_ = next_fence_sync_ - 1;
  }

  void OrderingBarrierCHROMIUM() override {
    flushed_fence_sync_ = next_fence_sync_ - 1;
  }

  GLuint64 InsertFenceSyncCHROMIUM() override { return next_fence_sync_++; }

  void GenSyncTokenCHROMIUM(GLuint64 fence_sync, GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    if (fence_sync <= flushed_fence_sync_) {
      sync_token_data.Set(gpu::CommandBufferNamespace::GPU_IO, 0,
                          gpu::CommandBufferId(), fence_sync);
      sync_token_data.SetVerifyFlush();
    }
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  void GenUnverifiedSyncTokenCHROMIUM(GLuint64 fence_sync,
                                      GLbyte* sync_token) override {
    gpu::SyncToken sync_token_data;
    if (fence_sync <= flushed_fence_sync_) {
      sync_token_data.Set(gpu::CommandBufferNamespace::GPU_IO, 0,
                          gpu::CommandBufferId(), fence_sync);
    }
    memcpy(sync_token, &sync_token_data, sizeof(sync_token_data));
  }

  void GenMailboxCHROMIUM(GLbyte* mailbox) override {
    *reinterpret_cast<unsigned*>(mailbox) = ++mailbox_;
  }

 private:
  uint64_t next_fence_sync_ = 1u;
  uint64_t flushed_fence_sync_ = 0u;
  unsigned mailbox_ = 0u;
};

}  // unnamed namespace

class GpuMemoryBufferVideoFramePoolTest : public ::testing::Test {
 public:
  GpuMemoryBufferVideoFramePoolTest() {}
  void SetUp() override {
    gles2_.reset(new TestGLES2Interface);
    media_task_runner_ = make_scoped_refptr(new base::TestSimpleTaskRunner);
    copy_task_runner_ = make_scoped_refptr(new base::TestSimpleTaskRunner);
    media_task_runner_handle_.reset(
        new base::ThreadTaskRunnerHandle(media_task_runner_));
    mock_gpu_factories_.reset(
        new MockGpuVideoAcceleratorFactories(gles2_.get()));
    gpu_memory_buffer_pool_.reset(new GpuMemoryBufferVideoFramePool(
        media_task_runner_, copy_task_runner_.get(),
        mock_gpu_factories_.get()));
  }

  void TearDown() override {
    gpu_memory_buffer_pool_.reset();
    RunUntilIdle();
    mock_gpu_factories_.reset();
  }

  void RunUntilIdle() {
    media_task_runner_->RunUntilIdle();
    copy_task_runner_->RunUntilIdle();
    media_task_runner_->RunUntilIdle();
  }

  static scoped_refptr<media::VideoFrame> CreateTestYUVVideoFrame(
      int dimension) {
    const int kDimension = 10;
    static uint8_t y_data[kDimension * kDimension] = {0};
    static uint8_t u_data[kDimension * kDimension / 2] = {0};
    static uint8_t v_data[kDimension * kDimension / 2] = {0};

    DCHECK_LE(dimension, kDimension);
    gfx::Size size(dimension, dimension);

    scoped_refptr<VideoFrame> video_frame =
        media::VideoFrame::WrapExternalYuvData(
            media::PIXEL_FORMAT_YV12,  // format
            size,                      // coded_size
            gfx::Rect(size),           // visible_rect
            size,                      // natural_size
            size.width(),              // y_stride
            size.width() / 2,          // u_stride
            size.width() / 2,          // v_stride
            y_data,                    // y_data
            u_data,                    // u_data
            v_data,                    // v_data
            base::TimeDelta());        // timestamp
    EXPECT_TRUE(video_frame);
    return video_frame;
  }

 protected:
  std::unique_ptr<MockGpuVideoAcceleratorFactories> mock_gpu_factories_;
  std::unique_ptr<GpuMemoryBufferVideoFramePool> gpu_memory_buffer_pool_;
  scoped_refptr<base::TestSimpleTaskRunner> media_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> copy_task_runner_;
  // GpuMemoryBufferVideoFramePool uses BindToCurrentLoop(), which requires
  // ThreadTaskRunnerHandle initialization.
  std::unique_ptr<base::ThreadTaskRunnerHandle> media_task_runner_handle_;
  std::unique_ptr<TestGLES2Interface> gles2_;
};

void MaybeCreateHardwareFrameCallback(
    scoped_refptr<VideoFrame>* video_frame_output,
    const scoped_refptr<VideoFrame>& video_frame) {
  *video_frame_output = video_frame;
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, VideoFrameOutputFormatUnknown) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  mock_gpu_factories_->SetVideoFrameOutputFormat(PIXEL_FORMAT_UNKNOWN);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(software_frame.get(), frame.get());
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(3u, gles2_->gen_textures);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, ReuseFirstResource) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  gpu::Mailbox mailbox = frame->mailbox_holder(0).mailbox;
  const gpu::SyncToken sync_token = frame->mailbox_holder(0).sync_token;
  EXPECT_EQ(3u, gles2_->gen_textures);

  scoped_refptr<VideoFrame> frame2;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame2));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame2.get());
  EXPECT_NE(mailbox, frame2->mailbox_holder(0).mailbox);
  EXPECT_EQ(6u, gles2_->gen_textures);

  frame = nullptr;
  frame2 = nullptr;
  RunUntilIdle();

  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(6u, gles2_->gen_textures);
  EXPECT_EQ(frame->mailbox_holder(0).mailbox, mailbox);
  EXPECT_NE(frame->mailbox_holder(0).sync_token, sync_token);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, DropResourceWhenSizeIsDifferent) {
  scoped_refptr<VideoFrame> frame;
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(10),
      base::Bind(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();

  EXPECT_EQ(3u, gles2_->gen_textures);

  frame = nullptr;
  RunUntilIdle();
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      CreateTestYUVVideoFrame(4),
      base::Bind(MaybeCreateHardwareFrameCallback, &frame));
  RunUntilIdle();
  EXPECT_EQ(6u, gles2_->gen_textures);
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareUYUVFrame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(PIXEL_FORMAT_UYVY);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(1u, gles2_->gen_textures);
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

TEST_F(GpuMemoryBufferVideoFramePoolTest, CreateOneHardwareNV12Frame) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetVideoFrameOutputFormat(PIXEL_FORMAT_NV12);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(1u, gles2_->gen_textures);
  EXPECT_TRUE(frame->metadata()->IsTrue(
      media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED));
}

// AllocateGpuMemoryBuffer can return null (e.g: when the GPU process is down).
// This test checks that in that case we don't crash and still create the
// textures.
TEST_F(GpuMemoryBufferVideoFramePoolTest, AllocateGpuMemoryBufferFail) {
  scoped_refptr<VideoFrame> software_frame = CreateTestYUVVideoFrame(10);
  scoped_refptr<VideoFrame> frame;
  mock_gpu_factories_->SetFailToAllocateGpuMemoryBufferForTesting(true);
  gpu_memory_buffer_pool_->MaybeCreateHardwareFrame(
      software_frame, base::Bind(MaybeCreateHardwareFrameCallback, &frame));

  RunUntilIdle();

  EXPECT_NE(software_frame.get(), frame.get());
  EXPECT_EQ(3u, gles2_->gen_textures);
}

}  // namespace media
