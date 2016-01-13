// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureBufferPool.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class VideoCaptureBufferPoolTest : public testing::Test {
 protected:
  class Buffer {
   public:
    Buffer(const scoped_refptr<VideoCaptureBufferPool> pool,
           int id,
           void* data,
           size_t size)
        : pool_(pool), id_(id), data_(data), size_(size) {}
    ~Buffer() { pool_->RelinquishProducerReservation(id()); }
    int id() const { return id_; }
    void* data() const { return data_; }
    size_t size() const { return size_; }

   private:
    const scoped_refptr<VideoCaptureBufferPool> pool_;
    const int id_;
    void* const data_;
    const size_t size_;
  };
  VideoCaptureBufferPoolTest()
      : expected_dropped_id_(0),
        pool_(new VideoCaptureBufferPool(3)) {}

  void ExpectDroppedId(int expected_dropped_id) {
    expected_dropped_id_ = expected_dropped_id;
  }

  scoped_ptr<Buffer> ReserveI420Buffer(const gfx::Size& dimensions) {
    const size_t frame_bytes =
        media::VideoFrame::AllocationSize(media::VideoFrame::I420, dimensions);
    // To verify that ReserveI420Buffer always sets |buffer_id_to_drop|,
    // initialize it to something different than the expected value.
    int buffer_id_to_drop = ~expected_dropped_id_;
    int buffer_id = pool_->ReserveForProducer(frame_bytes, &buffer_id_to_drop);
    if (buffer_id == VideoCaptureBufferPool::kInvalidId)
      return scoped_ptr<Buffer>();

    void* memory;
    size_t size;
    pool_->GetBufferInfo(buffer_id, &memory, &size);
    EXPECT_EQ(expected_dropped_id_, buffer_id_to_drop);
    return scoped_ptr<Buffer>(new Buffer(pool_, buffer_id, memory, size));
  }

  int expected_dropped_id_;
  scoped_refptr<VideoCaptureBufferPool> pool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureBufferPoolTest);
};

TEST_F(VideoCaptureBufferPoolTest, BufferPool) {
  const gfx::Size size_lo = gfx::Size(640, 480);
  const gfx::Size size_hi = gfx::Size(1024, 768);
  scoped_refptr<media::VideoFrame> non_pool_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12, size_lo,
                                     gfx::Rect(size_lo), size_lo,
                                     base::TimeDelta());

  // Reallocation won't happen for the first part of the test.
  ExpectDroppedId(VideoCaptureBufferPool::kInvalidId);

  scoped_ptr<Buffer> buffer1 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer1.get());
  ASSERT_LE(media::VideoFrame::AllocationSize(media::VideoFrame::I420, size_lo),
            buffer1->size());
  scoped_ptr<Buffer> buffer2 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer2.get());
  ASSERT_LE(media::VideoFrame::AllocationSize(media::VideoFrame::I420, size_lo),
            buffer2->size());
  scoped_ptr<Buffer> buffer3 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer3.get());
  ASSERT_LE(media::VideoFrame::AllocationSize(media::VideoFrame::I420, size_lo),
            buffer3->size());

  // Touch the memory.
  memset(buffer1->data(), 0x11, buffer1->size());
  memset(buffer2->data(), 0x44, buffer2->size());
  memset(buffer3->data(), 0x77, buffer3->size());

  // Fourth buffer should fail.
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  // Release 1st buffer and retry; this should succeed.
  buffer1.reset();
  scoped_ptr<Buffer> buffer4 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer4.get());

  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";
  ASSERT_FALSE(ReserveI420Buffer(size_hi)) << "Pool should be empty";

  // Validate the IDs
  int buffer_id2 = buffer2->id();
  ASSERT_EQ(1, buffer_id2);
  int buffer_id3 = buffer3->id();
  ASSERT_EQ(2, buffer_id3);
  void* const memory_pointer3 = buffer3->data();
  int buffer_id4 = buffer4->id();
  ASSERT_EQ(0, buffer_id4);

  // Deliver a buffer.
  pool_->HoldForConsumers(buffer_id3, 2);

  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  buffer3.reset();  // Old producer releases buffer. Should be a noop.
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";
  ASSERT_FALSE(ReserveI420Buffer(size_hi)) << "Pool should be empty";

  buffer2.reset();  // Active producer releases buffer. Should free a buffer.

  buffer1 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer1.get());
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  // First consumer finishes.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  // Second consumer finishes. This should free that buffer.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  buffer3 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer3.get());
  ASSERT_EQ(buffer_id3, buffer3->id()) << "Buffer ID should be reused.";
  ASSERT_EQ(memory_pointer3, buffer3->data());
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  // Now deliver & consume buffer1, but don't release the buffer.
  int buffer_id1 = buffer1->id();
  ASSERT_EQ(1, buffer_id1);
  pool_->HoldForConsumers(buffer_id1, 5);
  pool_->RelinquishConsumerHold(buffer_id1, 5);

  // Even though the consumer is done with the buffer at |buffer_id1|, it cannot
  // be re-allocated to the producer, because |buffer1| still references it. But
  // when |buffer1| goes away, we should be able to re-reserve the buffer (and
  // the ID ought to be the same).
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";
  buffer1.reset();  // Should free the buffer.
  buffer2 = ReserveI420Buffer(size_lo);
  ASSERT_TRUE(NULL != buffer2.get());
  ASSERT_EQ(buffer_id1, buffer2->id());
  buffer_id2 = buffer_id1;
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  // Now try reallocation with different resolutions. We expect reallocation
  // to occur only when the old buffer is too small.
  buffer2.reset();
  ExpectDroppedId(buffer_id2);
  buffer2 = ReserveI420Buffer(size_hi);
  ASSERT_TRUE(NULL != buffer2.get());
  ASSERT_LE(media::VideoFrame::AllocationSize(media::VideoFrame::I420, size_hi),
            buffer2->size());
  ASSERT_EQ(3, buffer2->id());
  void* const memory_pointer_hi = buffer2->data();
  buffer2.reset();  // Frees it.
  ExpectDroppedId(VideoCaptureBufferPool::kInvalidId);
  buffer2 = ReserveI420Buffer(size_lo);
  void* const memory_pointer_lo = buffer2->data();
  ASSERT_EQ(memory_pointer_hi, memory_pointer_lo)
      << "Decrease in resolution should not reallocate buffer";
  ASSERT_TRUE(NULL != buffer2.get());
  ASSERT_EQ(3, buffer2->id());
  ASSERT_LE(media::VideoFrame::AllocationSize(media::VideoFrame::I420, size_lo),
            buffer2->size());
  ASSERT_FALSE(ReserveI420Buffer(size_lo)) << "Pool should be empty";

  // Tear down the pool_, writing into the buffers. The buffer should preserve
  // the lifetime of the underlying memory.
  buffer3.reset();
  pool_ = NULL;

  // Touch the memory.
  memset(buffer2->data(), 0x22, buffer2->size());
  memset(buffer4->data(), 0x55, buffer4->size());

  buffer2.reset();

  memset(buffer4->data(), 0x77, buffer4->size());
  buffer4.reset();
}

} // namespace content
