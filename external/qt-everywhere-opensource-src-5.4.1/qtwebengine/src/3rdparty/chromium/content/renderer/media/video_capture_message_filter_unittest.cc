// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {
namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() : device_id_(0) {}

  // VideoCaptureMessageFilter::Delegate implementation.
  MOCK_METHOD3(OnBufferCreated, void(base::SharedMemoryHandle handle,
                                     int length,
                                     int buffer_id));
  MOCK_METHOD1(OnBufferDestroyed, void(int buffer_id));
  MOCK_METHOD3(OnBufferReceived,
               void(int buffer_id,
                    const media::VideoCaptureFormat& format,
                    base::TimeTicks timestamp));
  MOCK_METHOD4(OnMailboxBufferReceived,
               void(int buffer_id,
                    const gpu::MailboxHolder& mailbox_holder,
                    const media::VideoCaptureFormat& format,
                    base::TimeTicks timestamp));
  MOCK_METHOD1(OnStateChanged, void(VideoCaptureState state));
  MOCK_METHOD1(OnDeviceSupportedFormatsEnumerated,
               void(const media::VideoCaptureFormats& formats));
  MOCK_METHOD1(OnDeviceFormatsInUseReceived,
               void(const media::VideoCaptureFormats& formats_in_use));

  virtual void OnDelegateAdded(int32 device_id) OVERRIDE {
    ASSERT_TRUE(device_id != 0);
    ASSERT_TRUE(device_id_ == 0);
    device_id_ = device_id;
  }

  int device_id() { return device_id_; }

 private:
  int device_id_;
};

}  // namespace

TEST(VideoCaptureMessageFilterTest, Basic) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);
  MockVideoCaptureDelegate delegate;
  filter->AddDelegate(&delegate);
  ASSERT_EQ(1, delegate.device_id());

  // VideoCaptureMsg_StateChanged
  EXPECT_CALL(delegate, OnStateChanged(VIDEO_CAPTURE_STATE_STARTED));
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate.device_id(),
                                   VIDEO_CAPTURE_STATE_STARTED));
  Mock::VerifyAndClearExpectations(&delegate);

  // VideoCaptureMsg_NewBuffer
  const base::SharedMemoryHandle handle =
#if defined(OS_WIN)
      reinterpret_cast<base::SharedMemoryHandle>(10);
#else
      base::SharedMemoryHandle(10, true);
#endif
  EXPECT_CALL(delegate, OnBufferCreated(handle, 100, 1));
  filter->OnMessageReceived(VideoCaptureMsg_NewBuffer(
      delegate.device_id(), handle, 100, 1));
  Mock::VerifyAndClearExpectations(&delegate);

  // VideoCaptureMsg_BufferReady
  int buffer_id = 22;
  base::TimeTicks timestamp = base::TimeTicks::FromInternalValue(1);

  const media::VideoCaptureFormat shm_format(
      gfx::Size(234, 512), 30, media::PIXEL_FORMAT_I420);
  media::VideoCaptureFormat saved_format;
  EXPECT_CALL(delegate, OnBufferReceived(buffer_id, _, timestamp))
      .WillRepeatedly(SaveArg<1>(&saved_format));
  filter->OnMessageReceived(VideoCaptureMsg_BufferReady(
      delegate.device_id(), buffer_id, shm_format, timestamp));
  Mock::VerifyAndClearExpectations(&delegate);
  EXPECT_EQ(shm_format.frame_size, saved_format.frame_size);
  EXPECT_EQ(shm_format.frame_rate, saved_format.frame_rate);
  EXPECT_EQ(shm_format.pixel_format, saved_format.pixel_format);

  // VideoCaptureMsg_MailboxBufferReady
  buffer_id = 33;
  timestamp = base::TimeTicks::FromInternalValue(2);

  const media::VideoCaptureFormat mailbox_format(
      gfx::Size(234, 512), 30, media::PIXEL_FORMAT_TEXTURE);
  gpu::Mailbox mailbox;
  const int8 mailbox_name[arraysize(mailbox.name)] = "TEST MAILBOX";
  mailbox.SetName(mailbox_name);
  unsigned int syncpoint = 44;
  gpu::MailboxHolder saved_mailbox_holder;
  EXPECT_CALL(delegate, OnMailboxBufferReceived(buffer_id, _, _, timestamp))
      .WillRepeatedly(
           DoAll(SaveArg<1>(&saved_mailbox_holder), SaveArg<2>(&saved_format)));
  gpu::MailboxHolder mailbox_holder(mailbox, 0, syncpoint);
  filter->OnMessageReceived(
      VideoCaptureMsg_MailboxBufferReady(delegate.device_id(),
                                         buffer_id,
                                         mailbox_holder,
                                         mailbox_format,
                                         timestamp));
  Mock::VerifyAndClearExpectations(&delegate);
  EXPECT_EQ(mailbox_format.frame_size, saved_format.frame_size);
  EXPECT_EQ(mailbox_format.frame_rate, saved_format.frame_rate);
  EXPECT_EQ(mailbox_format.pixel_format, saved_format.pixel_format);
  EXPECT_EQ(memcmp(mailbox.name,
                   saved_mailbox_holder.mailbox.name,
                   sizeof(mailbox.name)),
            0);

  // VideoCaptureMsg_FreeBuffer
  EXPECT_CALL(delegate, OnBufferDestroyed(buffer_id));
  filter->OnMessageReceived(VideoCaptureMsg_FreeBuffer(
      delegate.device_id(), buffer_id));
  Mock::VerifyAndClearExpectations(&delegate);
}

TEST(VideoCaptureMessageFilterTest, Delegates) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);

  StrictMock<MockVideoCaptureDelegate> delegate1;
  StrictMock<MockVideoCaptureDelegate> delegate2;

  filter->AddDelegate(&delegate1);
  filter->AddDelegate(&delegate2);
  ASSERT_EQ(1, delegate1.device_id());
  ASSERT_EQ(2, delegate2.device_id());

  // Send an IPC message. Make sure the correct delegate gets called.
  EXPECT_CALL(delegate1, OnStateChanged(VIDEO_CAPTURE_STATE_STARTED));
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate1.device_id(),
                                   VIDEO_CAPTURE_STATE_STARTED));
  Mock::VerifyAndClearExpectations(&delegate1);

  EXPECT_CALL(delegate2, OnStateChanged(VIDEO_CAPTURE_STATE_STARTED));
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate2.device_id(),
                                   VIDEO_CAPTURE_STATE_STARTED));
  Mock::VerifyAndClearExpectations(&delegate2);

  // Remove the delegates. Make sure they won't get called.
  filter->RemoveDelegate(&delegate1);
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate1.device_id(),
                                   VIDEO_CAPTURE_STATE_ENDED));

  filter->RemoveDelegate(&delegate2);
  filter->OnMessageReceived(
      VideoCaptureMsg_StateChanged(delegate2.device_id(),
                                   VIDEO_CAPTURE_STATE_ENDED));
}

TEST(VideoCaptureMessageFilterTest, GetSomeDeviceSupportedFormats) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);
  MockVideoCaptureDelegate delegate;
  filter->AddDelegate(&delegate);
  ASSERT_EQ(1, delegate.device_id());

  EXPECT_CALL(delegate, OnDeviceSupportedFormatsEnumerated(_));
  media::VideoCaptureFormats supported_formats;
  filter->OnMessageReceived(VideoCaptureMsg_DeviceSupportedFormatsEnumerated(
      delegate.device_id(), supported_formats));
}

TEST(VideoCaptureMessageFilterTest, GetSomeDeviceFormatInUse) {
  scoped_refptr<VideoCaptureMessageFilter> filter(
      new VideoCaptureMessageFilter());

  IPC::TestSink channel;
  filter->OnFilterAdded(&channel);
  MockVideoCaptureDelegate delegate;
  filter->AddDelegate(&delegate);
  ASSERT_EQ(1, delegate.device_id());

  EXPECT_CALL(delegate, OnDeviceFormatsInUseReceived(_));
  media::VideoCaptureFormats formats_in_use;
  filter->OnMessageReceived(VideoCaptureMsg_DeviceFormatsInUseReceived(
      delegate.device_id(), formats_in_use));
}
}  // namespace content
