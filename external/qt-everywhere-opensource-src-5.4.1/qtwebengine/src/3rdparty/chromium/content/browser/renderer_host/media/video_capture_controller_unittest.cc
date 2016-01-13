// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureController.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Mock;

namespace content {

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  explicit MockVideoCaptureControllerEventHandler(
      VideoCaptureController* controller)
      : controller_(controller) {}
  virtual ~MockVideoCaptureControllerEventHandler() {}

  // These mock methods are delegated to by our fake implementation of
  // VideoCaptureControllerEventHandler, to be used in EXPECT_CALL().
  MOCK_METHOD1(DoBufferCreated, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoBufferDestroyed, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoBufferReady, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoMailboxBufferReady, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoEnded, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoError, void(const VideoCaptureControllerID&));

  virtual void OnError(const VideoCaptureControllerID& id) OVERRIDE {
    DoError(id);
  }
  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE {
    DoBufferCreated(id);
  }
  virtual void OnBufferDestroyed(const VideoCaptureControllerID& id,
                                 int buffer_id) OVERRIDE {
    DoBufferDestroyed(id);
  }
  virtual void OnBufferReady(const VideoCaptureControllerID& id,
                             int buffer_id,
                             const media::VideoCaptureFormat& format,
                             base::TimeTicks timestamp) OVERRIDE {
    DoBufferReady(id);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureController::ReturnBuffer,
                   base::Unretained(controller_),
                   id,
                   this,
                   buffer_id,
                   std::vector<uint32>()));
  }
  virtual void OnMailboxBufferReady(const VideoCaptureControllerID& id,
                                    int buffer_id,
                                    const gpu::MailboxHolder& mailbox_holder,
                                    const media::VideoCaptureFormat& format,
                                    base::TimeTicks timestamp) OVERRIDE {
    DoMailboxBufferReady(id);
    // Use a very different syncpoint value when returning a new syncpoint.
    std::vector<uint32> release_sync_points;
    release_sync_points.push_back(~mailbox_holder.sync_point);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&VideoCaptureController::ReturnBuffer,
                   base::Unretained(controller_),
                   id,
                   this,
                   buffer_id,
                   release_sync_points));
  }
  virtual void OnEnded(const VideoCaptureControllerID& id) OVERRIDE {
    DoEnded(id);
    // OnEnded() must respond by (eventually) unregistering the client.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(base::IgnoreResult(&VideoCaptureController::RemoveClient),
                   base::Unretained(controller_), id, this));
  }

  VideoCaptureController* controller_;
};

// Test class.
class VideoCaptureControllerTest : public testing::Test {
 public:
  VideoCaptureControllerTest() {}
  virtual ~VideoCaptureControllerTest() {}

 protected:
  static const int kPoolSize = 3;

  virtual void SetUp() OVERRIDE {
    controller_.reset(new VideoCaptureController());
    device_ = controller_->NewDeviceClient().Pass();
    client_a_.reset(new MockVideoCaptureControllerEventHandler(
        controller_.get()));
    client_b_.reset(new MockVideoCaptureControllerEventHandler(
        controller_.get()));
  }

  virtual void TearDown() OVERRIDE {
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<media::VideoFrame> WrapI420Buffer(
      const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>& buffer,
      gfx::Size dimensions) {
    return media::VideoFrame::WrapExternalPackedMemory(
        media::VideoFrame::I420,
        dimensions,
        gfx::Rect(dimensions),
        dimensions,
        reinterpret_cast<uint8*>(buffer->data()),
        media::VideoFrame::AllocationSize(media::VideoFrame::I420, dimensions),
        base::SharedMemory::NULLHandle(),
        base::TimeDelta(),
        base::Closure());
  }

  scoped_refptr<media::VideoFrame> WrapMailboxBuffer(
      const scoped_refptr<media::VideoCaptureDevice::Client::Buffer>& buffer,
      scoped_ptr<gpu::MailboxHolder> holder,
      const media::VideoFrame::ReleaseMailboxCB& release_cb,
      gfx::Size dimensions) {
    return media::VideoFrame::WrapNativeTexture(
        holder.Pass(),
        release_cb,
        dimensions,
        gfx::Rect(dimensions),
        dimensions,
        base::TimeDelta(),
        media::VideoFrame::ReadPixelsCB());
  }

  TestBrowserThreadBundle bundle_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> client_a_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> client_b_;
  scoped_ptr<VideoCaptureController> controller_;
  scoped_ptr<media::VideoCaptureDevice::Client> device_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureControllerTest);
};

// A simple test of VideoCaptureController's ability to add, remove, and keep
// track of clients.
TEST_F(VideoCaptureControllerTest, AddAndRemoveClients) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureParams session_200 = session_100;

  media::VideoCaptureParams session_300 = session_100;

  media::VideoCaptureParams session_400 = session_100;

  // Intentionally use the same route ID for two of the clients: the device_ids
  // are a per-VideoCaptureHost namespace, and can overlap across hosts.
  const VideoCaptureControllerID client_a_route_1(44);
  const VideoCaptureControllerID client_a_route_2(30);
  const VideoCaptureControllerID client_b_route_1(30);
  const VideoCaptureControllerID client_b_route_2(1);

  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should initially be zero.";
  controller_->AddClient(client_a_route_1,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         100,
                         session_100);
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Adding client A/1 should bump client count.";
  controller_->AddClient(client_a_route_2,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         200,
                         session_200);
  // Clients in controller: [A/1, A/2]
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Adding client A/2 should bump client count.";
  controller_->AddClient(client_b_route_1,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         300,
                         session_300);
  // Clients in controller: [A/1, A/2, B/1]
  ASSERT_EQ(3, controller_->GetClientCount())
      << "Adding client B/1 should bump client count.";
  ASSERT_EQ(200,
      controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing client A/1 should return its session_id.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
      controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing a nonexistant client should fail.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(300,
      controller_->RemoveClient(client_b_route_1, client_b_.get()))
      << "Removing client B/1 should return its session_id.";
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount());
  controller_->AddClient(client_b_route_2,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         400,
                         session_400);
  // Clients in controller: [A/1, B/2]

  EXPECT_CALL(*client_a_, DoEnded(client_a_route_1)).Times(1);
  controller_->StopSession(100);  // Session 100 == client A/1
  Mock::VerifyAndClearExpectations(client_a_.get());
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Client should be closed but still exist after StopSession.";
  // Clients in controller: [A/1 (closed, removal pending), B/2]
  base::RunLoop().RunUntilIdle();
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Client A/1 should be deleted by now.";
  controller_->StopSession(200);  // Session 200 does not exist anymore
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 200 should be a no-op.";
  controller_->StopSession(256);  // Session 256 never existed.
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 256 should be a no-op.";
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
      controller_->RemoveClient(client_a_route_1, client_a_.get()))
      << "Removing already-removed client A/1 should fail.";
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Removing non-existant session 200 should be a no-op.";
  ASSERT_EQ(400,
      controller_->RemoveClient(client_b_route_2, client_b_.get()))
      << "Removing client B/2 should return its session_id.";
  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should return to zero after all clients are gone.";
}

static void CacheSyncPoint(std::vector<uint32>* called_release_sync_points,
                           const std::vector<uint32>& release_sync_points) {
  DCHECK(called_release_sync_points->empty());
  called_release_sync_points->assign(release_sync_points.begin(),
                                     release_sync_points.end());
}

// This test will connect and disconnect several clients while simulating an
// active capture device being started and generating frames. It runs on one
// thread and is intended to behave deterministically.
TEST_F(VideoCaptureControllerTest, NormalCaptureMultipleClients) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  media::VideoCaptureParams session_300 = session_100;

  media::VideoCaptureParams session_1 = session_100;

  gfx::Size capture_resolution(444, 200);

  // The device format needn't match the VideoCaptureParams (the camera can do
  // what it wants). Pick something random.
  media::VideoCaptureFormat device_format(
      gfx::Size(10, 10), 25, media::PIXEL_FORMAT_RGB24);

  const VideoCaptureControllerID client_a_route_1(0xa1a1a1a1);
  const VideoCaptureControllerID client_a_route_2(0xa2a2a2a2);
  const VideoCaptureControllerID client_b_route_1(0xb1b1b1b1);
  const VideoCaptureControllerID client_b_route_2(0xb2b2b2b2);

  // Start with two clients.
  controller_->AddClient(client_a_route_1,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         100,
                         session_100);
  controller_->AddClient(client_b_route_1,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         300,
                         session_300);
  controller_->AddClient(client_a_route_2,
                         client_a_.get(),
                         base::kNullProcessHandle,
                         200,
                         session_200);
  ASSERT_EQ(3, controller_->GetClientCount());

  // Now, simulate an incoming captured buffer from the capture device. As a
  // side effect this will cause the first buffer to be shared with clients.
  uint8 buffer_no = 1;
  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> buffer;
  buffer =
      device_->ReserveOutputBuffer(media::VideoFrame::I420, capture_resolution);
  ASSERT_TRUE(buffer);
  memset(buffer->data(), buffer_no++, buffer->size());
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1)).Times(1);
    EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_1)).Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1)).Times(1);
    EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_1)).Times(1);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2)).Times(1);
    EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_2)).Times(1);
  }
  device_->OnIncomingCapturedVideoFrame(
      buffer,
      media::VideoCaptureFormat(capture_resolution,
                                device_format.frame_rate,
                                media::PIXEL_FORMAT_I420),
      WrapI420Buffer(buffer, capture_resolution),
      base::TimeTicks());
  buffer = NULL;

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Second buffer which ought to use the same shared memory buffer. In this
  // case pretend that the Buffer pointer is held by the device for a long
  // delay. This shouldn't affect anything.
  buffer =
      device_->ReserveOutputBuffer(media::VideoFrame::I420, capture_resolution);
  ASSERT_TRUE(buffer);
  memset(buffer->data(), buffer_no++, buffer->size());
  device_->OnIncomingCapturedVideoFrame(
      buffer,
      media::VideoCaptureFormat(capture_resolution,
                                device_format.frame_rate,
                                media::PIXEL_FORMAT_I420),
      WrapI420Buffer(buffer, capture_resolution),
      base::TimeTicks());
  buffer = NULL;

  // The buffer should be delivered to the clients in any order.
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_1)).Times(1);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_1)).Times(1);
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_2)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Add a fourth client now that some buffers have come through.
  controller_->AddClient(client_b_route_2,
                         client_b_.get(),
                         base::kNullProcessHandle,
                         1,
                         session_1);
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Third, fourth, and fifth buffers. Pretend they all arrive at the same time.
  for (int i = 0; i < kPoolSize; i++) {
    buffer = device_->ReserveOutputBuffer(media::VideoFrame::I420,
                                          capture_resolution);
    ASSERT_TRUE(buffer);
    memset(buffer->data(), buffer_no++, buffer->size());
    device_->OnIncomingCapturedVideoFrame(
        buffer,
        media::VideoCaptureFormat(capture_resolution,
                                  device_format.frame_rate,
                                  media::PIXEL_FORMAT_I420),
        WrapI420Buffer(buffer, capture_resolution),
        base::TimeTicks());
    buffer = NULL;
  }
  // ReserveOutputBuffer ought to fail now, because the pool is depleted.
  ASSERT_FALSE(device_->ReserveOutputBuffer(media::VideoFrame::I420,
                                            capture_resolution));

  // The new client needs to be told of 3 buffers; the old clients only 2.
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_2)).Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_2)).Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_1)).Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_2)).Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1))
      .Times(kPoolSize - 1);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_1)).Times(kPoolSize);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Now test the interaction of client shutdown and buffer delivery.
  // Kill A1 via renderer disconnect (synchronous).
  controller_->RemoveClient(client_a_route_1, client_a_.get());
  // Kill B1 via session close (posts a task to disconnect).
  EXPECT_CALL(*client_b_, DoEnded(client_b_route_1)).Times(1);
  controller_->StopSession(300);
  // Queue up another buffer.
  buffer =
      device_->ReserveOutputBuffer(media::VideoFrame::I420, capture_resolution);
  ASSERT_TRUE(buffer);
  memset(buffer->data(), buffer_no++, buffer->size());
  device_->OnIncomingCapturedVideoFrame(
      buffer,
      media::VideoCaptureFormat(capture_resolution,
                                device_format.frame_rate,
                                media::PIXEL_FORMAT_I420),
      WrapI420Buffer(buffer, capture_resolution),
      base::TimeTicks());
  buffer = NULL;
  buffer =
      device_->ReserveOutputBuffer(media::VideoFrame::I420, capture_resolution);
  {
    // Kill A2 via session close (posts a task to disconnect, but A2 must not
    // be sent either of these two buffers).
    EXPECT_CALL(*client_a_, DoEnded(client_a_route_2)).Times(1);
    controller_->StopSession(200);
  }
  ASSERT_TRUE(buffer);
  memset(buffer->data(), buffer_no++, buffer->size());
  device_->OnIncomingCapturedVideoFrame(
      buffer,
      media::VideoCaptureFormat(capture_resolution,
                                device_format.frame_rate,
                                media::PIXEL_FORMAT_I420),
      WrapI420Buffer(buffer, capture_resolution),
      base::TimeTicks());
  buffer = NULL;
  // B2 is the only client left, and is the only one that should
  // get the buffer.
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_2)).Times(2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Allocate all buffers from the buffer pool, half as SHM buffer and half as
  // mailbox buffers.  Make sure of different counts though.
  int shm_buffers = kPoolSize / 2;
  int mailbox_buffers = kPoolSize - shm_buffers;
  if (shm_buffers == mailbox_buffers) {
    shm_buffers--;
    mailbox_buffers++;
  }

  for (int i = 0; i < shm_buffers; ++i) {
    buffer = device_->ReserveOutputBuffer(media::VideoFrame::I420,
                                          capture_resolution);
    ASSERT_TRUE(buffer);
    device_->OnIncomingCapturedVideoFrame(
        buffer,
        media::VideoCaptureFormat(capture_resolution,
                                  device_format.frame_rate,
                                  media::PIXEL_FORMAT_I420),
        WrapI420Buffer(buffer, capture_resolution),
        base::TimeTicks());
    buffer = NULL;
  }
  std::vector<uint32> mailbox_syncpoints(mailbox_buffers);
  std::vector<std::vector<uint32> > release_syncpoint_vectors(mailbox_buffers);
  for (int i = 0; i < mailbox_buffers; ++i) {
    buffer = device_->ReserveOutputBuffer(media::VideoFrame::NATIVE_TEXTURE,
                                          gfx::Size(0, 0));
    ASSERT_TRUE(buffer);
    mailbox_syncpoints[i] = i;
    device_->OnIncomingCapturedVideoFrame(
        buffer,
        media::VideoCaptureFormat(capture_resolution,
                                  device_format.frame_rate,
                                  media::PIXEL_FORMAT_TEXTURE),
        WrapMailboxBuffer(
            buffer,
            make_scoped_ptr(new gpu::MailboxHolder(
                gpu::Mailbox(), 0, mailbox_syncpoints[i])),
            base::Bind(&CacheSyncPoint, &release_syncpoint_vectors[i]),
            capture_resolution),
        base::TimeTicks());
    buffer = NULL;
  }
  // ReserveOutputBuffers ought to fail now regardless of buffer format, because
  // the pool is depleted.
  ASSERT_FALSE(device_->ReserveOutputBuffer(media::VideoFrame::I420,
                                            capture_resolution));
  ASSERT_FALSE(device_->ReserveOutputBuffer(media::VideoFrame::NATIVE_TEXTURE,
                                            gfx::Size(0, 0)));
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_2)).Times(shm_buffers);
  EXPECT_CALL(*client_b_, DoMailboxBufferReady(client_b_route_2))
      .Times(mailbox_buffers);
  base::RunLoop().RunUntilIdle();
  for (size_t i = 0; i < mailbox_syncpoints.size(); ++i) {
    // See: MockVideoCaptureControllerEventHandler::OnMailboxBufferReady()
    ASSERT_EQ(1u, release_syncpoint_vectors[i].size());
    ASSERT_EQ(mailbox_syncpoints[i], ~release_syncpoint_vectors[i][0]);
  }
  Mock::VerifyAndClearExpectations(client_b_.get());
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorBeforeDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  const gfx::Size capture_resolution(320, 240);

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(
      route_id, client_a_.get(), base::kNullProcessHandle, 100, session_100);
  device_->OnError("Test Error");
  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(
      route_id, client_b_.get(), base::kNullProcessHandle, 200, session_200);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_b_.get());

  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> buffer =
      device_->ReserveOutputBuffer(media::VideoFrame::I420, capture_resolution);
  ASSERT_TRUE(buffer);

  device_->OnIncomingCapturedVideoFrame(
      buffer,
      media::VideoCaptureFormat(
          capture_resolution, 30, media::PIXEL_FORMAT_I420),
      WrapI420Buffer(buffer, capture_resolution),
      base::TimeTicks());
  buffer = NULL;

  base::RunLoop().RunUntilIdle();
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorAfterDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.requested_format = media::VideoCaptureFormat(
      gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

  media::VideoCaptureParams session_200 = session_100;

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(
      route_id, client_a_.get(), base::kNullProcessHandle, 100, session_100);
  media::VideoCaptureFormat device_format(
      gfx::Size(10, 10), 25, media::PIXEL_FORMAT_ARGB);

  // Start the device. Then, before the first buffer, signal an error and
  // deliver the buffer. The error should be propagated to clients; the buffer
  // should not be.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  const gfx::Size dims(320, 240);
  scoped_refptr<media::VideoCaptureDevice::Client::Buffer> buffer =
      device_->ReserveOutputBuffer(media::VideoFrame::I420, dims);
  ASSERT_TRUE(buffer);

  device_->OnError("Test error");
  device_->OnIncomingCapturedVideoFrame(
      buffer,
      media::VideoCaptureFormat(
          dims, device_format.frame_rate, media::PIXEL_FORMAT_I420),
      WrapI420Buffer(buffer, dims),
      base::TimeTicks());
  buffer = NULL;

  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(
      route_id, client_b_.get(), base::kNullProcessHandle, 200, session_200);
  Mock::VerifyAndClearExpectations(client_b_.get());
}

}  // namespace content
