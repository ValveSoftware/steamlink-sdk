// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_devices.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "media/audio/mock_audio_manager.h"
#include "media/base/media_switches.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {

namespace {

void VideoInputDevicesEnumerated(base::Closure quit_closure,
                                 const std::string& salt,
                                 const url::Origin& security_origin,
                                 MediaDeviceInfoArray* out,
                                 const MediaDeviceEnumeration& enumeration) {
  for (const auto& info : enumeration[MEDIA_DEVICE_TYPE_VIDEO_INPUT]) {
    std::string device_id = MediaStreamManager::GetHMACForMediaDeviceID(
        salt, security_origin, info.device_id);
    out->push_back(MediaDeviceInfo(device_id, info.label, std::string()));
  }
  quit_closure.Run();
}

}  // namespace

// Id used to identify the capture session between renderer and
// video_capture_host. This is an arbitrary value.
static const int kDeviceId = 555;

class MockMediaStreamRequester : public MediaStreamRequester {
 public:
  MockMediaStreamRequester() {}
  virtual ~MockMediaStreamRequester() {}

  // MediaStreamRequester implementation.
  MOCK_METHOD5(StreamGenerated,
               void(int render_frame_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfoArray& audio_devices,
                    const StreamDeviceInfoArray& video_devices));
  MOCK_METHOD3(StreamGenerationFailed,
      void(int render_frame_id,
           int page_request_id,
           content::MediaStreamRequestResult result));
  MOCK_METHOD3(DeviceStopped, void(int render_frame_id,
                                   const std::string& label,
                                   const StreamDeviceInfo& device));
  MOCK_METHOD4(DevicesEnumerated, void(int render_frame_id,
                                       int page_request_id,
                                       const std::string& label,
                                       const StreamDeviceInfoArray& devices));
  MOCK_METHOD4(DeviceOpened, void(int render_frame_id,
                                  int page_request_id,
                                  const std::string& label,
                                  const StreamDeviceInfo& device_info));
  MOCK_METHOD1(DevicesChanged, void(MediaStreamType type));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamRequester);
};

ACTION_P2(ExitMessageLoop, task_runner, quit_closure) {
  task_runner->PostTask(FROM_HERE, quit_closure);
}

// This is an integration test of VideoCaptureHost in conjunction with
// MediaStreamManager, VideoCaptureManager, VideoCaptureController, and
// VideoCaptureDevice.
class VideoCaptureTest : public testing::Test,
                         public mojom::VideoCaptureObserver {
 public:
  VideoCaptureTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        audio_manager_(
            new media::MockAudioManager(base::ThreadTaskRunnerHandle::Get())),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        opened_session_id_(kInvalidMediaCaptureSessionId),
        observer_binding_(this) {}

  void SetUp() override {
    SetBrowserClientForTesting(&browser_client_);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeUIForMediaStream);
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));

    // Create a Host and connect it to a simulated IPC channel.
    host_.reset(new VideoCaptureHost(media_stream_manager_.get()));

    OpenSession();
  }

  void TearDown() override {
    Mock::VerifyAndClearExpectations(host_.get());
    EXPECT_TRUE(host_->controllers_.empty());

    CloseSession();

    // Release the reference to the mock object. The object will be destructed
    // on the current message loop.
    host_ = nullptr;
  }

  void OpenSession() {
    const int render_process_id = 1;
    const int render_frame_id = 1;
    const int page_request_id = 1;
    const url::Origin security_origin(GURL("http://test.com"));

    ASSERT_TRUE(opened_device_label_.empty());

    // Enumerate video devices.
    MediaDeviceInfoArray video_devices;
    {
      base::RunLoop run_loop;
      MediaDevicesManager::BoolDeviceTypes devices_to_enumerate;
      devices_to_enumerate[MEDIA_DEVICE_TYPE_VIDEO_INPUT] = true;
      media_stream_manager_->media_devices_manager()->EnumerateDevices(
          devices_to_enumerate,
          base::Bind(
              &VideoInputDevicesEnumerated, run_loop.QuitClosure(),
              browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
              security_origin, &video_devices));
      run_loop.Run();
    }
    ASSERT_FALSE(video_devices.empty());

    // Open the first device.
    {
      base::RunLoop run_loop;
      StreamDeviceInfo opened_device;
      media_stream_manager_->OpenDevice(
          &stream_requester_, render_process_id, render_frame_id,
          browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
          page_request_id, video_devices[0].device_id,
          MEDIA_DEVICE_VIDEO_CAPTURE, security_origin);
      EXPECT_CALL(stream_requester_,
                  DeviceOpened(render_frame_id, page_request_id, _, _))
          .Times(1)
          .WillOnce(DoAll(ExitMessageLoop(task_runner_, run_loop.QuitClosure()),
                          SaveArg<2>(&opened_device_label_),
                          SaveArg<3>(&opened_device)));
      run_loop.Run();
      Mock::VerifyAndClearExpectations(&stream_requester_);
      ASSERT_NE(StreamDeviceInfo::kNoId, opened_device.session_id);
      opened_session_id_ = opened_device.session_id;
    }
  }

  void CloseSession() {
    if (opened_device_label_.empty())
      return;
    media_stream_manager_->CancelRequest(opened_device_label_);
    opened_device_label_.clear();
    opened_session_id_ = kInvalidMediaCaptureSessionId;
  }

 protected:
  // mojom::VideoCaptureObserver implementation.
  MOCK_METHOD1(OnStateChanged, void(mojom::VideoCaptureState));
  void OnBufferCreated(int32_t buffer_id,
                       mojo::ScopedSharedBufferHandle handle) override {
    DoOnBufferCreated(buffer_id);
  }
  MOCK_METHOD1(DoOnBufferCreated, void(int32_t));
  void OnBufferReady(int32_t buffer_id,
                     mojom::VideoFrameInfoPtr info) override {
    DoOnBufferReady(buffer_id);
  }
  MOCK_METHOD1(DoOnBufferReady, void(int32_t));
  MOCK_METHOD1(OnBufferDestroyed, void(int32_t));

  void StartCapture() {
    base::RunLoop run_loop;
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::STARTED));
    EXPECT_CALL(*this, DoOnBufferCreated(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return());
    EXPECT_CALL(*this, DoOnBufferReady(_))
        .Times(AnyNumber())
        .WillRepeatedly(ExitMessageLoop(task_runner_, run_loop.QuitClosure()));

    host_->Start(kDeviceId, opened_session_id_, params,
                 observer_binding_.CreateInterfacePtrAndBind());

    run_loop.Run();
  }

  void StartAndImmediateStopCapture() {
    // Quickly start and then stop capture, without giving much chance for
    // asynchronous capture operations to produce frames.
    InSequence s;
    base::RunLoop run_loop;

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::STARTED));
    host_->Start(kDeviceId, opened_session_id_, params,
                 observer_binding_.CreateInterfacePtrAndBind());

    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::STOPPED));
    host_->Stop(kDeviceId);
    run_loop.RunUntilIdle();
  }

  void PauseResumeCapture() {
    InSequence s;
    base::RunLoop run_loop;

    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::PAUSED));
    host_->Pause(kDeviceId);

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);

    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::RESUMED));
    host_->Resume(kDeviceId, opened_session_id_, params);
    run_loop.RunUntilIdle();
  }

  void StopCapture() {
    base::RunLoop run_loop;

    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::STOPPED))
        .WillOnce(ExitMessageLoop(task_runner_, run_loop.QuitClosure()));
    host_->Stop(kDeviceId);

    run_loop.Run();

    EXPECT_TRUE(host_->controllers_.empty());
  }

  void WaitForOneCapturedBuffer() {
    base::RunLoop run_loop;

    EXPECT_CALL(*this, DoOnBufferReady(_))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(task_runner_, run_loop.QuitClosure()))
        .RetiresOnSaturation();
    run_loop.Run();
  }

  void SimulateError() {
    EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::FAILED));
    VideoCaptureControllerID id(kDeviceId);
    host_->OnError(id);
    base::RunLoop().RunUntilIdle();
  }

 private:
  // |media_stream_manager_| needs to outlive |thread_bundle_| because it is a
  // MessageLoop::DestructionObserver.
  StrictMock<MockMediaStreamRequester> stream_requester_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  const content::TestBrowserThreadBundle thread_bundle_;
  // |audio_manager_| needs to outlive |thread_bundle_| because it uses the
  // underlying message loop.
  media::ScopedAudioManagerPtr audio_manager_;
  content::TestBrowserContext browser_context_;
  content::TestContentBrowserClient browser_client_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  int opened_session_id_;
  std::string opened_device_label_;

  std::unique_ptr<VideoCaptureHost> host_;
  mojo::Binding<mojom::VideoCaptureObserver> observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureTest);
};

// Construct and destruct all objects. This is a non trivial sequence.
TEST_F(VideoCaptureTest, ConstructAndDestruct) {}

TEST_F(VideoCaptureTest, StartAndImmediateStop) {
  StartAndImmediateStopCapture();
}

TEST_F(VideoCaptureTest, StartAndCaptureAndStop) {
  StartCapture();
  WaitForOneCapturedBuffer();
  WaitForOneCapturedBuffer();
  StopCapture();
}

TEST_F(VideoCaptureTest, StartAndErrorAndStop) {
  StartCapture();
  SimulateError();
  StopCapture();
}

TEST_F(VideoCaptureTest, StartAndCaptureAndError) {
  EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::STOPPED))
      .Times(0);
  StartCapture();
  WaitForOneCapturedBuffer();
  SimulateError();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
}

TEST_F(VideoCaptureTest, StartAndPauseAndResumeAndStop) {
  StartCapture();
  PauseResumeCapture();
  StopCapture();
}

TEST_F(VideoCaptureTest, CloseSessionWithoutStopping) {
  StartCapture();

  // When the session is closed via the stream without stopping capture, the
  // ENDED event is sent.
  EXPECT_CALL(*this, OnStateChanged(mojom::VideoCaptureState::ENDED));
  CloseSession();
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
