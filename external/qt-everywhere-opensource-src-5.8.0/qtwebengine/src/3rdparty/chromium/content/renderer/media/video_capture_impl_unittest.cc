// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "content/child/child_process.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

namespace content {

class MockVideoCaptureMessageFilter : public VideoCaptureMessageFilter {
 public:
  MockVideoCaptureMessageFilter() : VideoCaptureMessageFilter() {}

  // Filter implementation.
  MOCK_METHOD1(Send, bool(IPC::Message* message));

 protected:
  virtual ~MockVideoCaptureMessageFilter() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureMessageFilter);
};

class VideoCaptureImplTest : public ::testing::Test {
 public:
  class MockVideoCaptureImpl : public VideoCaptureImpl {
   public:
    MockVideoCaptureImpl(const media::VideoCaptureSessionId id,
                         VideoCaptureMessageFilter* filter)
        : VideoCaptureImpl(id, filter), received_buffer_count_(0) {}
    ~MockVideoCaptureImpl() override {}

    // Override Send() to mimic device to send events.
    void Send(IPC::Message* message) override {
      CHECK(message);

      // In this method, messages are sent to the according handlers as if
      // we are the device.
      bool handled = true;
      IPC_BEGIN_MESSAGE_MAP(MockVideoCaptureImpl, *message)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Start, DeviceStartCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Pause, DevicePauseCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_Stop, DeviceStopCapture)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_BufferReady,
                            DeviceReceiveEmptyBuffer)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_GetDeviceSupportedFormats,
                            DeviceGetSupportedFormats)
        IPC_MESSAGE_HANDLER(VideoCaptureHostMsg_GetDeviceFormatsInUse,
                            DeviceGetFormatsInUse)
        IPC_MESSAGE_UNHANDLED(handled = false)
      IPC_END_MESSAGE_MAP()
      EXPECT_TRUE(handled);
      delete message;
    }

    void DeviceStartCapture(int device_id,
                            media::VideoCaptureSessionId session_id,
                            const media::VideoCaptureParams& params) {
      OnStateChanged(VIDEO_CAPTURE_STATE_STARTED);
      capture_params_ = params;
    }

    void DevicePauseCapture(int device_id) {}

    void DeviceStopCapture(int device_id) {
      OnStateChanged(VIDEO_CAPTURE_STATE_STOPPED);
    }

    void DeviceReceiveEmptyBuffer(int device_id,
                                  int buffer_id,
                                  const gpu::SyncToken& release_sync_token,
                                  double consumer_resource_utilization) {
      received_buffer_count_++;
    }

    void DeviceGetSupportedFormats(int device_id,
                                   media::VideoCaptureSessionId session_id) {
      // When the mock message filter receives a request for the device
      // supported formats, replies immediately with an empty format list.
      OnDeviceSupportedFormatsEnumerated(media::VideoCaptureFormats());
    }

    void DeviceGetFormatsInUse(int device_id,
                               media::VideoCaptureSessionId session_id) {
      OnDeviceFormatsInUseReceived(media::VideoCaptureFormats());
    }

    void ReceiveStateChangeMessage(VideoCaptureState state) {
      OnStateChanged(state);
    }

    int received_buffer_count() const { return received_buffer_count_; }

    const media::VideoCaptureParams& capture_params() const {
      return capture_params_;
    }

   private:
    int received_buffer_count_;
    media::VideoCaptureParams capture_params_;
  };

  VideoCaptureImplTest() {
    params_small_.requested_format = media::VideoCaptureFormat(
        gfx::Size(176, 144), 30, media::PIXEL_FORMAT_I420);

    params_large_.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    child_process_.reset(new ChildProcess());

    message_filter_ = new MockVideoCaptureMessageFilter;
    session_id_ = 1;

    video_capture_impl_.reset(new MockVideoCaptureImpl(
        session_id_, message_filter_.get()));

    video_capture_impl_->device_id_ = 2;
  }

  virtual ~VideoCaptureImplTest() {
  }

 protected:
  MOCK_METHOD2(OnFrameReady,
               void(const scoped_refptr<media::VideoFrame>&, base::TimeTicks));
  MOCK_METHOD1(OnStateUpdate, void(VideoCaptureState));
  MOCK_METHOD1(OnDeviceFormatsInUse,
               void(const media::VideoCaptureFormats&));
  MOCK_METHOD1(OnDeviceSupportedFormats,
               void(const media::VideoCaptureFormats&));

  void Init() {
    video_capture_impl_->Init();
  }

  void StartCapture(int client_id, const media::VideoCaptureParams& params) {
    video_capture_impl_->StartCapture(
        client_id, params, base::Bind(&VideoCaptureImplTest::OnStateUpdate,
                                      base::Unretained(this)),
        base::Bind(&VideoCaptureImplTest::OnFrameReady,
                   base::Unretained(this)));
  }

  void StopCapture(int client_id) {
    video_capture_impl_->StopCapture(client_id);
  }

  void NewBuffer(int buffer_id, const base::SharedMemory& shm) {
    video_capture_impl_->OnBufferCreated(
        base::SharedMemory::DuplicateHandle(shm.handle()),
        shm.mapped_size(), buffer_id);
  }

  void BufferReceived(int buffer_id, const gfx::Size& size) {
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta timestamp = now - base::TimeTicks();

    media::VideoFrameMetadata frame_metadata;
    frame_metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                                now);
    base::DictionaryValue metadata;
    frame_metadata.MergeInternalValuesInto(&metadata);

    video_capture_impl_->OnBufferReceived(
        buffer_id, timestamp, metadata, media::PIXEL_FORMAT_I420,
        media::VideoFrame::STORAGE_SHMEM, size, gfx::Rect(size));
  }

  void BufferDestroyed(int buffer_id) {
    video_capture_impl_->OnBufferDestroyed(buffer_id);
  }

  void DeInit() {
    video_capture_impl_->DeInit();
  }

  void GetDeviceSupportedFormats() {
    const base::Callback<void(const media::VideoCaptureFormats&)>
        callback = base::Bind(
            &VideoCaptureImplTest::OnDeviceSupportedFormats,
            base::Unretained(this));
    video_capture_impl_->GetDeviceSupportedFormats(callback);
  }

  void GetDeviceFormatsInUse() {
    const base::Callback<void(const media::VideoCaptureFormats&)>
        callback = base::Bind(
            &VideoCaptureImplTest::OnDeviceFormatsInUse,
            base::Unretained(this));
    video_capture_impl_->GetDeviceFormatsInUse(callback);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  scoped_refptr<MockVideoCaptureMessageFilter> message_filter_;
  media::VideoCaptureSessionId session_id_;
  std::unique_ptr<MockVideoCaptureImpl> video_capture_impl_;
  media::VideoCaptureParams params_small_;
  media::VideoCaptureParams params_large_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureImplTest);
};

TEST_F(VideoCaptureImplTest, Simple) {
  // Execute SetCapture() and StopCapture() for one client.
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED));
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED));

  Init();
  StartCapture(0, params_small_);
  StopCapture(0);
  DeInit();
}

TEST_F(VideoCaptureImplTest, TwoClientsInSequence) {
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED)).Times(2);
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED)).Times(2);

  Init();
  StartCapture(0, params_small_);
  StopCapture(0);
  StartCapture(1, params_small_);
  StopCapture(1);
  DeInit();
}

TEST_F(VideoCaptureImplTest, LargeAndSmall) {
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED)).Times(2);
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED)).Times(2);

  Init();
  StartCapture(0, params_large_);
  StopCapture(0);
  StartCapture(1, params_small_);
  StopCapture(1);
  DeInit();
}

TEST_F(VideoCaptureImplTest, SmallAndLarge) {
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED)).Times(2);
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED)).Times(2);

  Init();
  StartCapture(0, params_small_);
  StopCapture(0);
  StartCapture(1, params_large_);
  StopCapture(1);
  DeInit();
}

// Check that a request to GetDeviceSupportedFormats() ends up eventually in the
// provided callback.
TEST_F(VideoCaptureImplTest, GetDeviceFormats) {
  EXPECT_CALL(*this, OnDeviceSupportedFormats(_));

  Init();
  GetDeviceSupportedFormats();
  DeInit();
}

// Check that two requests to GetDeviceSupportedFormats() end up eventually
// calling the provided callbacks.
TEST_F(VideoCaptureImplTest, TwoClientsGetDeviceFormats) {
  EXPECT_CALL(*this, OnDeviceSupportedFormats(_)).Times(2);

  Init();
  GetDeviceSupportedFormats();
  GetDeviceSupportedFormats();
  DeInit();
}

// Check that a request to GetDeviceFormatsInUse() ends up eventually in the
// provided callback.
TEST_F(VideoCaptureImplTest, GetDeviceFormatsInUse) {
  EXPECT_CALL(*this, OnDeviceFormatsInUse(_));

  Init();
  GetDeviceFormatsInUse();
  DeInit();
}

TEST_F(VideoCaptureImplTest, BufferReceived) {
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED)).Times(1);
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED)).Times(1);
  EXPECT_CALL(*this, OnFrameReady(_, _)).Times(1);

  const gfx::Size size(1280, 720);

  // Create a fake shared memory for buffer.
  base::SharedMemory shm;
  const size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, size);
  ASSERT_TRUE(shm.CreateAndMapAnonymous(frame_size));

  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      size, 30, media::PIXEL_FORMAT_I420);

  Init();
  StartCapture(0, params);
  NewBuffer(0, shm);
  BufferReceived(0, size);
  StopCapture(0);
  BufferDestroyed(0);
  DeInit();
}

TEST_F(VideoCaptureImplTest, BufferReceivedAfterStop) {
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED)).Times(1);
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED)).Times(1);
  EXPECT_CALL(*this, OnFrameReady(_, _)).Times(0);

  // Create a fake shared memory for buffer.
  base::SharedMemory shm;
  const size_t i420_frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, params_large_.requested_format.frame_size);
  ASSERT_TRUE(shm.CreateAndMapAnonymous(i420_frame_size));

  Init();
  StartCapture(0, params_large_);
  NewBuffer(0, shm);
  StopCapture(0);
  BufferReceived(0, params_large_.requested_format.frame_size);
  BufferDestroyed(0);
  DeInit();

  EXPECT_EQ(this->video_capture_impl_->received_buffer_count(), 1);
}

TEST_F(VideoCaptureImplTest, AlreadyStarted) {
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED)).Times(2);
  EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED)).Times(2);

  Init();
  StartCapture(0, params_small_);
  StartCapture(1, params_large_);
  StopCapture(0);
  StopCapture(1);
  DeInit();
  DCHECK(video_capture_impl_->capture_params().requested_format.frame_size ==
         params_small_.requested_format.frame_size);
}

TEST_F(VideoCaptureImplTest, EndedBeforeStop) {
   EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED));
   EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STOPPED));

   Init();
   StartCapture(0, params_small_);

   // Receive state change message from browser.
   video_capture_impl_->ReceiveStateChangeMessage(VIDEO_CAPTURE_STATE_ENDED);

   StopCapture(0);
   DeInit();
}

TEST_F(VideoCaptureImplTest, ErrorBeforeStop) {
   EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_STARTED));
   EXPECT_CALL(*this, OnStateUpdate(VIDEO_CAPTURE_STATE_ERROR));

   Init();
   StartCapture(0, params_small_);

   // Receive state change message from browser.
   video_capture_impl_->ReceiveStateChangeMessage(VIDEO_CAPTURE_STATE_ERROR);

   StopCapture(0);
   DeInit();
}

}  // namespace content
