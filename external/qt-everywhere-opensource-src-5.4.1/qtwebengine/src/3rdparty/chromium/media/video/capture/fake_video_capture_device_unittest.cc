// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/fake_video_capture_device_factory.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

namespace media {

class MockClient : public media::VideoCaptureDevice::Client {
 public:
  MOCK_METHOD2(ReserveOutputBuffer,
               scoped_refptr<Buffer>(media::VideoFrame::Format format,
                                     const gfx::Size& dimensions));
  MOCK_METHOD0(OnErr, void());

  explicit MockClient(base::Callback<void(const VideoCaptureFormat&)> frame_cb)
      : main_thread_(base::MessageLoopProxy::current()), frame_cb_(frame_cb) {}

  virtual void OnError(const std::string& error_message) OVERRIDE {
    OnErr();
  }

  virtual void OnIncomingCapturedData(const uint8* data,
                                      int length,
                                      const VideoCaptureFormat& format,
                                      int rotation,
                                      base::TimeTicks timestamp) OVERRIDE {
    main_thread_->PostTask(FROM_HERE, base::Bind(frame_cb_, format));
  }

  virtual void OnIncomingCapturedVideoFrame(
      const scoped_refptr<Buffer>& buffer,
      const media::VideoCaptureFormat& buffer_format,
      const scoped_refptr<media::VideoFrame>& frame,
      base::TimeTicks timestamp) OVERRIDE {
    NOTREACHED();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::Callback<void(const VideoCaptureFormat&)> frame_cb_;
};

class DeviceEnumerationListener :
    public base::RefCounted<DeviceEnumerationListener> {
 public:
  MOCK_METHOD1(OnEnumeratedDevicesCallbackPtr,
               void(media::VideoCaptureDevice::Names* names));
  // GMock doesn't support move-only arguments, so we use this forward method.
  void OnEnumeratedDevicesCallback(
      scoped_ptr<media::VideoCaptureDevice::Names> names) {
    OnEnumeratedDevicesCallbackPtr(names.release());
  }

 private:
  friend class base::RefCounted<DeviceEnumerationListener>;
  virtual ~DeviceEnumerationListener() {}
};

class FakeVideoCaptureDeviceTest : public testing::Test {
 protected:
  typedef media::VideoCaptureDevice::Client Client;

  FakeVideoCaptureDeviceTest()
      : loop_(new base::MessageLoop()),
        client_(new MockClient(
            base::Bind(&FakeVideoCaptureDeviceTest::OnFrameCaptured,
                       base::Unretained(this)))),
        video_capture_device_factory_(new FakeVideoCaptureDeviceFactory()) {
    device_enumeration_listener_ = new DeviceEnumerationListener();
  }

  virtual void SetUp() {
  }

  void OnFrameCaptured(const VideoCaptureFormat& format) {
    last_format_ = format;
    run_loop_->QuitClosure().Run();
  }

  void WaitForCapturedFrame() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  scoped_ptr<media::VideoCaptureDevice::Names> EnumerateDevices() {
    media::VideoCaptureDevice::Names* names;
    EXPECT_CALL(*device_enumeration_listener_,
                OnEnumeratedDevicesCallbackPtr(_)).WillOnce(SaveArg<0>(&names));

    video_capture_device_factory_->EnumerateDeviceNames(
        base::Bind(&DeviceEnumerationListener::OnEnumeratedDevicesCallback,
                   device_enumeration_listener_));
    base::MessageLoop::current()->RunUntilIdle();
    return scoped_ptr<media::VideoCaptureDevice::Names>(names);
  }

  const VideoCaptureFormat& last_format() const { return last_format_; }

  VideoCaptureDevice::Names names_;
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  scoped_ptr<MockClient> client_;
  scoped_refptr<DeviceEnumerationListener> device_enumeration_listener_;
  VideoCaptureFormat last_format_;
  scoped_ptr<VideoCaptureDeviceFactory> video_capture_device_factory_;
};

TEST_F(FakeVideoCaptureDeviceTest, Capture) {
  scoped_ptr<media::VideoCaptureDevice::Names> names(EnumerateDevices());

  ASSERT_GT(static_cast<int>(names->size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(names->front()));
  ASSERT_TRUE(device);

  EXPECT_CALL(*client_, OnErr()).Times(0);

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = false;
  device->AllocateAndStart(capture_params, client_.PassAs<Client>());
  WaitForCapturedFrame();
  EXPECT_EQ(last_format().frame_size.width(), 640);
  EXPECT_EQ(last_format().frame_size.height(), 480);
  EXPECT_EQ(last_format().frame_rate, 30);
  device->StopAndDeAllocate();
}

TEST_F(FakeVideoCaptureDeviceTest, GetDeviceSupportedFormats) {
  scoped_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());

  VideoCaptureFormats supported_formats;
  VideoCaptureDevice::Names::iterator names_iterator;

  for (names_iterator = names->begin(); names_iterator != names->end();
       ++names_iterator) {
    video_capture_device_factory_->GetDeviceSupportedFormats(
        *names_iterator, &supported_formats);
    EXPECT_EQ(supported_formats.size(), 3u);
    EXPECT_EQ(supported_formats[0].frame_size.width(), 320);
    EXPECT_EQ(supported_formats[0].frame_size.height(), 240);
    EXPECT_EQ(supported_formats[0].pixel_format, media::PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[0].frame_rate, 20);
    EXPECT_EQ(supported_formats[1].frame_size.width(), 640);
    EXPECT_EQ(supported_formats[1].frame_size.height(), 480);
    EXPECT_EQ(supported_formats[1].pixel_format, media::PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[1].frame_rate, 20);
    EXPECT_EQ(supported_formats[2].frame_size.width(), 1280);
    EXPECT_EQ(supported_formats[2].frame_size.height(), 720);
    EXPECT_EQ(supported_formats[2].pixel_format, media::PIXEL_FORMAT_I420);
    EXPECT_GE(supported_formats[2].frame_rate, 20);
  }
}

TEST_F(FakeVideoCaptureDeviceTest, CaptureVariableResolution) {
  scoped_ptr<VideoCaptureDevice::Names> names(EnumerateDevices());

  VideoCaptureParams capture_params;
  capture_params.requested_format.frame_size.SetSize(640, 480);
  capture_params.requested_format.frame_rate = 30;
  capture_params.requested_format.pixel_format = PIXEL_FORMAT_I420;
  capture_params.allow_resolution_change = true;

  ASSERT_GT(static_cast<int>(names->size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      video_capture_device_factory_->Create(names->front()));
  ASSERT_TRUE(device);

  // Configure the FakeVideoCaptureDevice to use all its formats as roster.
  VideoCaptureFormats formats;
  video_capture_device_factory_->GetDeviceSupportedFormats(names->front(),
                                                           &formats);
  static_cast<FakeVideoCaptureDevice*>(device.get())->
      PopulateVariableFormatsRoster(formats);

  EXPECT_CALL(*client_, OnErr())
      .Times(0);
  int action_count = 200;

  device->AllocateAndStart(capture_params, client_.PassAs<Client>());

  // We set TimeWait to 200 action timeouts and this should be enough for at
  // least action_count/kFakeCaptureCapabilityChangePeriod calls.
  for (int i = 0; i < action_count; ++i) {
    WaitForCapturedFrame();
  }
  device->StopAndDeAllocate();
}

};  // namespace media
