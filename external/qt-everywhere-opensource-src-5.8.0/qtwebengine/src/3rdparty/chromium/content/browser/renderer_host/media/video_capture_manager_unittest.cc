// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureManager.

#include "content/browser/renderer_host/media/video_capture_manager.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/media/media_stream_options.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;

namespace content {

// Listener class used to track progress of VideoCaptureManager test.
class MockMediaStreamProviderListener : public MediaStreamProviderListener {
 public:
  MockMediaStreamProviderListener() {}
  ~MockMediaStreamProviderListener() {}

  MOCK_METHOD2(Opened, void(MediaStreamType, int));
  MOCK_METHOD2(Closed, void(MediaStreamType, int));
  MOCK_METHOD2(DevicesEnumerated, void(MediaStreamType,
                                       const StreamDeviceInfoArray&));
  MOCK_METHOD2(Aborted, void(MediaStreamType, int));
};  // class MockMediaStreamProviderListener

// Needed as an input argument to StartCaptureForClient().
class MockFrameObserver : public VideoCaptureControllerEventHandler {
 public:
  MOCK_METHOD1(OnError, void(VideoCaptureControllerID id));

  void OnBufferCreated(VideoCaptureControllerID id,
                       base::SharedMemoryHandle handle,
                       int length, int buffer_id) override {}
  void OnBufferCreated2(VideoCaptureControllerID id,
                        const std::vector<gfx::GpuMemoryBufferHandle>& handles,
                        const gfx::Size& size,
                        int buffer_id) override {}
  void OnBufferDestroyed(VideoCaptureControllerID id, int buffer_id) override {}
  void OnBufferReady(VideoCaptureControllerID id,
                     int buffer_id,
                     const scoped_refptr<media::VideoFrame>& frame) override {}
  void OnEnded(VideoCaptureControllerID id) override {}

  void OnGotControllerCallback(VideoCaptureControllerID) {}
};

// Test class
class VideoCaptureManagerTest : public testing::Test {
 public:
  VideoCaptureManagerTest() : next_client_id_(1) {}
  ~VideoCaptureManagerTest() override {}

 protected:
  void SetUp() override {
    listener_.reset(new MockMediaStreamProviderListener());
    message_loop_.reset(new base::MessageLoopForIO);
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    vcm_ = new VideoCaptureManager(
        std::unique_ptr<media::VideoCaptureDeviceFactory>(
            new media::FakeVideoCaptureDeviceFactory()));
    video_capture_device_factory_ =
        static_cast<media::FakeVideoCaptureDeviceFactory*>(
            vcm_->video_capture_device_factory());
    const int32_t kNumberOfFakeDevices = 2;
    video_capture_device_factory_->set_number_of_devices(kNumberOfFakeDevices);
    vcm_->Register(listener_.get(), message_loop_->task_runner().get());
    frame_observer_.reset(new MockFrameObserver());
  }

  void TearDown() override {}

  void OnGotControllerCallback(
      VideoCaptureControllerID id,
      base::Closure quit_closure,
      bool expect_success,
      const base::WeakPtr<VideoCaptureController>& controller) {
    if (expect_success) {
      ASSERT_TRUE(controller);
      ASSERT_TRUE(0 == controllers_.count(id));
      controllers_[id] = controller.get();
    } else {
      ASSERT_FALSE(controller);
    }
    quit_closure.Run();
  }

  VideoCaptureControllerID StartClient(int session_id, bool expect_success) {
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    VideoCaptureControllerID client_id(next_client_id_++);
    base::RunLoop run_loop;
    vcm_->StartCaptureForClient(
        session_id,
        params,
        base::kNullProcessHandle,
        client_id,
        frame_observer_.get(),
        base::Bind(&VideoCaptureManagerTest::OnGotControllerCallback,
                   base::Unretained(this),
                   client_id,
                   run_loop.QuitClosure(),
                   expect_success));
    run_loop.Run();
    return client_id;
  }

  void StopClient(VideoCaptureControllerID client_id) {
    ASSERT_TRUE(1 == controllers_.count(client_id));
    vcm_->StopCaptureForClient(controllers_[client_id], client_id,
                               frame_observer_.get(), false);
    controllers_.erase(client_id);
  }

  void ResumeClient(int session_id, int client_id) {
    ASSERT_EQ(1u, controllers_.count(client_id));
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(320, 240), 30, media::PIXEL_FORMAT_I420);

    vcm_->ResumeCaptureForClient(
        session_id,
        params,
        controllers_[client_id],
        client_id,
        frame_observer_.get());
  }

  void PauseClient(VideoCaptureControllerID client_id) {
    ASSERT_EQ(1u, controllers_.count(client_id));
    vcm_->PauseCaptureForClient(controllers_[client_id], client_id,
                               frame_observer_.get());
  }

#if defined(OS_ANDROID)
  void ApplicationStateChange(base::android::ApplicationState state) {
    vcm_->OnApplicationStateChange(state);
  }
#endif

  int next_client_id_;
  std::map<VideoCaptureControllerID, VideoCaptureController*> controllers_;
  scoped_refptr<VideoCaptureManager> vcm_;
  std::unique_ptr<MockMediaStreamProviderListener> listener_;
  std::unique_ptr<base::MessageLoop> message_loop_;
  std::unique_ptr<BrowserThreadImpl> ui_thread_;
  std::unique_ptr<BrowserThreadImpl> io_thread_;
  std::unique_ptr<MockFrameObserver> frame_observer_;
  media::FakeVideoCaptureDeviceFactory* video_capture_device_factory_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManagerTest);
};

// Test cases

// Try to open, start, stop and close a device.
TEST_F(VideoCaptureManagerTest, CreateAndClose) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id = vcm_->Open(devices.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

TEST_F(VideoCaptureManagerTest, CreateAndCloseMultipleTimes) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  for (int i = 1 ; i < 3 ; ++i) {
    EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, i));
    EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, i));
    int video_session_id = vcm_->Open(devices.front());
    VideoCaptureControllerID client_id = StartClient(video_session_id, true);

    StopClient(client_id);
    vcm_->Close(video_session_id);
  }

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Try to open, start, and abort a device.
TEST_F(VideoCaptureManagerTest, CreateAndAbort) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_, Aborted(MEDIA_DEVICE_VIDEO_CAPTURE, _));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id = vcm_->Open(devices.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Wait for device opened.
  message_loop_->RunUntilIdle();

  vcm_->StopCaptureForClient(controllers_[client_id], client_id,
                             frame_observer_.get(), true);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Open the same device twice.
TEST_F(VideoCaptureManagerTest, OpenTwice) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id_first = vcm_->Open(devices.front());

  // This should trigger an error callback with error code
  // 'kDeviceAlreadyInUse'.
  int video_session_id_second = vcm_->Open(devices.front());
  EXPECT_NE(video_session_id_first, video_session_id_second);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Connect and disconnect devices.
TEST_F(VideoCaptureManagerTest, ConnectAndDisconnectDevices) {
  StreamDeviceInfoArray devices;
  int number_of_devices_keep =
    video_capture_device_factory_->number_of_devices();

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);
  message_loop_->RunUntilIdle();
  ASSERT_EQ(devices.size(), 2u);

  // Simulate we remove 1 fake device.
  video_capture_device_factory_->set_number_of_devices(1);
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);
  message_loop_->RunUntilIdle();
  ASSERT_EQ(devices.size(), 1u);

  // Simulate we add 2 fake devices.
  video_capture_device_factory_->set_number_of_devices(3);
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);
  message_loop_->RunUntilIdle();
  ASSERT_EQ(devices.size(), 3u);

  vcm_->Unregister();
  video_capture_device_factory_->set_number_of_devices(number_of_devices_keep);
}

// Enumerate devices and open the first, then check the list of supported
// formats. Then start the opened device. The capability list should stay the
// same. Finally stop the device and check that the capabilities stay unchanged.
TEST_F(VideoCaptureManagerTest, ManipulateDeviceAndCheckCapabilities) {
  StreamDeviceInfoArray devices;

  // Before enumerating the devices, requesting formats should return false.
  int video_session_id = 0;
  media::VideoCaptureFormats supported_formats;
  supported_formats.clear();
  EXPECT_FALSE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);
  message_loop_->RunUntilIdle();
  ASSERT_GE(devices.size(), 2u);

  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  video_session_id = vcm_->Open(devices.front());
  message_loop_->RunUntilIdle();

  // Right after opening the device, we should see all its formats.
  supported_formats.clear();
  EXPECT_TRUE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));
  ASSERT_GT(supported_formats.size(), 1u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  VideoCaptureControllerID client_id = StartClient(video_session_id, true);
  message_loop_->RunUntilIdle();
  // After StartClient(), device's supported formats should stay the same.
  supported_formats.clear();
  EXPECT_TRUE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  supported_formats.clear();
  EXPECT_TRUE(
      vcm_->GetDeviceSupportedFormats(video_session_id, &supported_formats));
  ASSERT_GE(supported_formats.size(), 2u);
  EXPECT_GT(supported_formats[0].frame_size.width(), 1);
  EXPECT_GT(supported_formats[0].frame_size.height(), 1);
  EXPECT_GT(supported_formats[0].frame_rate, 1);
  EXPECT_GT(supported_formats[1].frame_size.width(), 1);
  EXPECT_GT(supported_formats[1].frame_size.height(), 1);
  EXPECT_GT(supported_formats[1].frame_rate, 1);

  vcm_->Close(video_session_id);
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Enumerate devices and open the first, then check the formats currently in
// use, which should be an empty vector. Then start the opened device. The
// format(s) in use should be just one format (the one used when configuring-
// starting the device). Finally stop the device and check that the formats in
// use is an empty vector.
TEST_F(VideoCaptureManagerTest, StartDeviceAndGetDeviceFormatInUse) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);
  message_loop_->RunUntilIdle();
  ASSERT_GE(devices.size(), 2u);

  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  int video_session_id = vcm_->Open(devices.front());
  message_loop_->RunUntilIdle();

  // Right after opening the device, we should see no format in use.
  media::VideoCaptureFormats formats_in_use;
  EXPECT_TRUE(vcm_->GetDeviceFormatsInUse(video_session_id, &formats_in_use));
  EXPECT_TRUE(formats_in_use.empty());

  VideoCaptureControllerID client_id = StartClient(video_session_id, true);
  message_loop_->RunUntilIdle();
  // After StartClient(), |formats_in_use| should contain one valid format.
  EXPECT_TRUE(vcm_->GetDeviceFormatsInUse(video_session_id, &formats_in_use));
  EXPECT_EQ(formats_in_use.size(), 1u);
  if (formats_in_use.size()) {
    media::VideoCaptureFormat& format_in_use = formats_in_use.front();
    EXPECT_TRUE(format_in_use.IsValid());
    EXPECT_GT(format_in_use.frame_size.width(), 1);
    EXPECT_GT(format_in_use.frame_size.height(), 1);
    EXPECT_GT(format_in_use.frame_rate, 1);
  }
  formats_in_use.clear();

  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  StopClient(client_id);
  message_loop_->RunUntilIdle();
  // After StopClient(), the device's formats in use should be empty again.
  EXPECT_TRUE(vcm_->GetDeviceFormatsInUse(video_session_id, &formats_in_use));
  EXPECT_TRUE(formats_in_use.empty());

  vcm_->Close(video_session_id);
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Open two different devices.
TEST_F(VideoCaptureManagerTest, OpenTwo) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  StreamDeviceInfoArray::iterator it = devices.begin();

  int video_session_id_first = vcm_->Open(*it);
  ++it;
  int video_session_id_second = vcm_->Open(*it);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Try open a non-existing device.
TEST_F(VideoCaptureManagerTest, OpenNotExisting) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*frame_observer_, OnError(_));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  MediaStreamType stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  StreamDeviceInfo dummy_device(stream_type, device_name, device_id);

  // This should fail with an error to the controller.
  int session_id = vcm_->Open(dummy_device);
  VideoCaptureControllerID client_id = StartClient(session_id, true);
  message_loop_->RunUntilIdle();

  StopClient(client_id);
  vcm_->Close(session_id);
  message_loop_->RunUntilIdle();

  vcm_->Unregister();
}

// Start a device without calling Open, using a non-magic ID.
TEST_F(VideoCaptureManagerTest, StartInvalidSession) {
  StartClient(22, false);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Open and start a device, close it before calling Stop.
TEST_F(VideoCaptureManagerTest, CloseWithoutStop) {
  StreamDeviceInfoArray devices;
  base::RunLoop run_loop;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(
          DoAll(SaveArg<1>(&devices),
                InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit)));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  run_loop.Run();
  ASSERT_FALSE(devices.empty());
  int video_session_id = vcm_->Open(devices.front());

  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Close will stop the running device, an assert will be triggered in
  // VideoCaptureManager destructor otherwise.
  vcm_->Close(video_session_id);
  StopClient(client_id);

  // Wait to check callbacks before removing the listener
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Try to open, start, pause and resume a device.
TEST_F(VideoCaptureManagerTest, PauseAndResumeClient) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id = vcm_->Open(devices.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Resume client a second time should cause no problem.
  PauseClient(client_id);
  ResumeClient(video_session_id, client_id);
  ResumeClient(video_session_id, client_id);

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

#if defined(OS_ANDROID)
// Try to open, start, pause and resume a device.
TEST_F(VideoCaptureManagerTest, PauseAndResumeDevice) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _));
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _));

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id = vcm_->Open(devices.front());
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Release/ResumeDevices according to ApplicationStatus. Should cause no
  // problem in any order. Check https://crbug.com/615557 for more details.
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  ApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  StopClient(client_id);
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}
#endif

// TODO(mcasas): Add a test to check consolidation of the supported formats
// provided by the device when http://crbug.com/323913 is closed.

}  // namespace content
