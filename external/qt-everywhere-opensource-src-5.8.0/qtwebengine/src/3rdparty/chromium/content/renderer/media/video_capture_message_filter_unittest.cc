// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "content/common/media/video_capture_messages.h"
#include "content/renderer/media/video_capture_message_filter.h"
#include "ipc/ipc_test_sink.h"
#include "media/base/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace content {
namespace {

class MockVideoCaptureDelegate : public VideoCaptureMessageFilter::Delegate {
 public:
  MockVideoCaptureDelegate() : device_id_(0) {}

  // VideoCaptureMessageFilter::Delegate implementation.
  MOCK_METHOD3(OnBufferCreated,
               void(base::SharedMemoryHandle handle,
                    int length,
                    int buffer_id));
  MOCK_METHOD3(OnBufferCreated2,
               void(const std::vector<gfx::GpuMemoryBufferHandle>& handles,
                    const gfx::Size& size,
                    int buffer_id));
  MOCK_METHOD1(OnBufferDestroyed, void(int buffer_id));
  MOCK_METHOD7(OnBufferReceived,
               void(int buffer_id,
                    base::TimeDelta timestamp,
                    const base::DictionaryValue& metadata,
                    media::VideoPixelFormat pixel_format,
                    media::VideoFrame::StorageType storage_type,
                    const gfx::Size& coded_size,
                    const gfx::Rect& visible_rect));
  MOCK_METHOD1(OnStateChanged, void(VideoCaptureState state));
  MOCK_METHOD1(OnDeviceSupportedFormatsEnumerated,
               void(const media::VideoCaptureFormats& formats));
  MOCK_METHOD1(OnDeviceFormatsInUseReceived,
               void(const media::VideoCaptureFormats& formats_in_use));

  void OnDelegateAdded(int32_t device_id) override {
    ASSERT_TRUE(device_id != 0);
    ASSERT_TRUE(device_id_ == 0);
    device_id_ = device_id;
  }

  int device_id() { return device_id_; }

 private:
  int device_id_;
};

void ExpectMetadataContainsFooBarBaz(const base::DictionaryValue& metadata) {
  std::string value;
  if (metadata.GetString("foo", &value))
    EXPECT_EQ(std::string("bar"), value);
  else if (metadata.GetString("bar", &value))
    EXPECT_EQ(std::string("baz"), value);
  else
    FAIL() << "Missing key 'foo' or key 'bar'.";
}

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
#if defined(OS_WIN)
  HANDLE h = reinterpret_cast<HANDLE>(10);
  // Passing a process ID that is not the current process's to prevent
  // attachment brokering.
  const base::SharedMemoryHandle handle =
      base::SharedMemoryHandle(h, base::GetCurrentProcId() + 1);
  EXPECT_CALL(delegate,
              OnBufferCreated(
                  ::testing::Property(&base::SharedMemoryHandle::GetHandle, h),
                  100, 1));
#elif defined(OS_MACOSX) && !defined(OS_IOS)
  const base::SharedMemoryHandle handle =
      base::SharedMemoryHandle(10, 100, base::GetCurrentProcId());
  EXPECT_CALL(delegate, OnBufferCreated(handle, 100, 1));
#else
  const base::SharedMemoryHandle handle = base::SharedMemoryHandle(10, true);
  EXPECT_CALL(delegate, OnBufferCreated(handle, 100, 1));
#endif
  filter->OnMessageReceived(VideoCaptureMsg_NewBuffer(
      delegate.device_id(), handle, 100, 1));
  Mock::VerifyAndClearExpectations(&delegate);
  // VideoCaptureMsg_BufferReady
  VideoCaptureMsg_BufferReady_Params params;
  params.device_id = delegate.device_id();
  params.buffer_id = 22;
  params.timestamp = base::TimeDelta::FromMicroseconds(1);
  params.metadata.SetString("foo", "bar");
  media::VideoFrameMetadata frame_metadata;
  frame_metadata.SetTimeTicks(media::VideoFrameMetadata::REFERENCE_TIME,
                              base::TimeTicks::FromInternalValue(1));
  frame_metadata.MergeInternalValuesInto(&params.metadata);
  params.pixel_format = media::PIXEL_FORMAT_I420;
  params.storage_type = media::VideoFrame::STORAGE_SHMEM;
  params.coded_size = gfx::Size(234, 512);
  params.visible_rect = gfx::Rect(100, 200, 300, 400);

  EXPECT_CALL(delegate,
              OnBufferReceived(params.buffer_id, params.timestamp, _,
                               media::PIXEL_FORMAT_I420,
                               media::VideoFrame::STORAGE_SHMEM,
                               params.coded_size, params.visible_rect))
      .WillRepeatedly(WithArg<2>(Invoke(&ExpectMetadataContainsFooBarBaz)));
  filter->OnMessageReceived(VideoCaptureMsg_BufferReady(params));
  Mock::VerifyAndClearExpectations(&delegate);

  // VideoCaptureMsg_FreeBuffer
  EXPECT_CALL(delegate, OnBufferDestroyed(params.buffer_id));
  filter->OnMessageReceived(VideoCaptureMsg_FreeBuffer(
      delegate.device_id(), params.buffer_id));
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
