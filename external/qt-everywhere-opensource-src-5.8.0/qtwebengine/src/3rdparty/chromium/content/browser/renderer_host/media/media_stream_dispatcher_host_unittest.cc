// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <stddef.h>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/system_monitor/system_monitor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_messages.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/browser/media_device_id.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/mock_audio_manager.h"
#include "media/base/media_switches.h"
#include "media/capture/video/fake_video_capture_device_factory.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#endif

using ::testing::_;
using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

const int kProcessId = 5;
const int kRenderId = 6;
const int kPageRequestId = 7;

namespace content {

class MockMediaStreamDispatcherHost : public MediaStreamDispatcherHost,
                                      public TestContentBrowserClient {
 public:
  MockMediaStreamDispatcherHost(
      const std::string& salt,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      MediaStreamManager* manager)
      : MediaStreamDispatcherHost(kProcessId, salt, manager, true),
        task_runner_(task_runner),
        current_ipc_(NULL) {}

  // A list of mock methods.
  MOCK_METHOD4(OnStreamGenerated,
               void(int routing_id, int request_id, int audio_array_size,
                    int video_array_size));
  MOCK_METHOD3(OnStreamGenerationFailed, void(int routing_id,
                                              int request_id,
                                              MediaStreamRequestResult result));
  MOCK_METHOD1(OnDeviceStopped, void(int routing_id));
  MOCK_METHOD2(OnDeviceOpened, void(int routing_id, int request_id));

  // Accessor to private functions.
  void OnGenerateStream(int render_frame_id,
                        int page_request_id,
                        const StreamControls& controls,
                        const url::Origin& security_origin,
                        const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnGenerateStream(
        render_frame_id, page_request_id, controls, security_origin, false);
  }

  void OnStopStreamDevice(int render_frame_id,
                          const std::string& device_id) {
    MediaStreamDispatcherHost::OnStopStreamDevice(render_frame_id, device_id);
  }

  void OnOpenDevice(int render_frame_id,
                    int page_request_id,
                    const std::string& device_id,
                    MediaStreamType type,
                    const url::Origin& security_origin,
                    const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnOpenDevice(
        render_frame_id, page_request_id, device_id, type, security_origin);
  }

  void OnEnumerateDevices(int render_frame_id,
                          int page_request_id,
                          MediaStreamType type,
                          const url::Origin& security_origin,
                          const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnEnumerateDevices(
        render_frame_id, page_request_id, type, security_origin);
  }

  void OnCancelEnumerateDevices(int render_frame_id, int page_request_id) {
    MediaStreamDispatcherHost::OnCancelEnumerateDevices(render_frame_id,
                                                        page_request_id);
  }

  void OnSubscribeToDeviceChangeNotifications(
      int render_frame_id,
      const url::Origin& security_origin,
      const base::Closure& quit_closure) {
    quit_closures_.push(quit_closure);
    MediaStreamDispatcherHost::OnSubscribeToDeviceChangeNotifications(
        render_frame_id, security_origin);
  }

  std::string label_;
  StreamDeviceInfoArray audio_devices_;
  StreamDeviceInfoArray video_devices_;
  StreamDeviceInfo opened_device_;
  StreamDeviceInfoArray enumerated_devices_;

 private:
  ~MockMediaStreamDispatcherHost() override {}

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  bool Send(IPC::Message* message) override {
    CHECK(message);
    current_ipc_ = message;

    // In this method we dispatch the messages to the corresponding handlers as
    // if we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockMediaStreamDispatcherHost, *message)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerated,
                          OnStreamGeneratedInternal)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_StreamGenerationFailed,
                          OnStreamGenerationFailedInternal)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceStopped, OnDeviceStoppedInternal)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DeviceOpened, OnDeviceOpenedInternal)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesEnumerated, OnDevicesEnumerated)
      IPC_MESSAGE_HANDLER(MediaStreamMsg_DevicesChanged, OnDevicesChanged)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    current_ipc_ = NULL;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnStreamGeneratedInternal(
      int request_id,
      std::string label,
      StreamDeviceInfoArray audio_device_list,
      StreamDeviceInfoArray video_device_list) {
    OnStreamGenerated(current_ipc_->routing_id(), request_id,
                      audio_device_list.size(), video_device_list.size());
    // Notify that the event have occurred.
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));

    label_ = label;
    audio_devices_ = audio_device_list;
    video_devices_ = video_device_list;
  }

  void OnStreamGenerationFailedInternal(
      int request_id,
      content::MediaStreamRequestResult result) {
    OnStreamGenerationFailed(current_ipc_->routing_id(), request_id, result);
    if (!quit_closures_.empty()) {
      base::Closure quit_closure = quit_closures_.front();
      quit_closures_.pop();
      task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    }

    label_ = "";
  }

  void OnDeviceStoppedInternal(const std::string& label,
                               const content::StreamDeviceInfo& device) {
    if (IsVideoMediaType(device.device.type))
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(device, video_devices_[0]));
    if (IsAudioInputMediaType(device.device.type))
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(device, audio_devices_[0]));

    OnDeviceStopped(current_ipc_->routing_id());
  }

  void OnDeviceOpenedInternal(int request_id,
                              const std::string& label,
                              const StreamDeviceInfo& device) {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    label_ = label;
    opened_device_ = device;
  }

  void OnDevicesEnumerated(int request_id,
                           const StreamDeviceInfoArray& devices) {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
    enumerated_devices_ = devices;
  }

  void OnDevicesChanged() {
    base::Closure quit_closure = quit_closures_.front();
    quit_closures_.pop();
    task_runner_->PostTask(FROM_HERE, base::ResetAndReturn(&quit_closure));
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  IPC::Message* current_ipc_;
  std::queue<base::Closure> quit_closures_;
};

class MockMediaStreamUIProxy : public FakeMediaStreamUIProxy {
 public:
  MOCK_METHOD2(
      OnStarted,
      void(const base::Closure& stop,
           const MediaStreamUIProxy::WindowIdCallback& window_id_callback));
};

class MediaStreamDispatcherHostTest : public testing::Test {
 public:
  MediaStreamDispatcherHostTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        old_browser_client_(NULL),
        origin_(GURL("https://test.com")) {
    audio_manager_.reset(
        new media::MockAudioManager(base::ThreadTaskRunnerHandle::Get()));
    // Make sure we use fake devices to avoid long delays.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    // Create our own MediaStreamManager.
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    video_capture_device_factory_ =
        static_cast<media::FakeVideoCaptureDeviceFactory*>(
            media_stream_manager_->video_capture_manager()
            ->video_capture_device_factory());
    DCHECK(video_capture_device_factory_);
#if defined(OS_WIN)
    // Override the Video Capture Thread that MediaStreamManager constructs.
    media_stream_manager_->video_capture_manager()->set_device_task_runner(
        base::ThreadTaskRunnerHandle::Get());
#endif

    MockResourceContext* mock_resource_context =
        static_cast<MockResourceContext*>(
            browser_context_.GetResourceContext());

    host_ = new MockMediaStreamDispatcherHost(
        mock_resource_context->GetMediaDeviceIDSalt(),
        base::ThreadTaskRunnerHandle::Get(), media_stream_manager_.get());

    // Use the fake content client and browser.
    content_client_.reset(new TestContentClient());
    SetContentClient(content_client_.get());
    old_browser_client_ = SetBrowserClientForTesting(host_.get());

#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  }

  ~MediaStreamDispatcherHostTest() override {
#if defined(OS_CHROMEOS)
    chromeos::CrasAudioHandler::Shutdown();
#endif
  }

  void SetUp() override {
    video_capture_device_factory_->GetDeviceNames(&physical_video_devices_);
    ASSERT_GT(physical_video_devices_.size(), 0u);

    media_stream_manager_->audio_input_device_manager()->GetFakeDeviceNames(
        &physical_audio_devices_);
    ASSERT_GT(physical_audio_devices_.size(), 0u);
  }

  void TearDown() override { host_->OnChannelClosing(); }

 protected:
  virtual void SetupFakeUI(bool expect_started) {
    stream_ui_ = new MockMediaStreamUIProxy();
    if (expect_started) {
      EXPECT_CALL(*stream_ui_, OnStarted(_, _));
    }
    media_stream_manager_->UseFakeUIForTests(
        std::unique_ptr<FakeMediaStreamUIProxy>(stream_ui_));
  }

  void GenerateStreamAndWaitForResult(int render_frame_id,
                                      int page_request_id,
                                      const StreamControls& controls) {
    base::RunLoop run_loop;
    int expected_audio_array_size =
        (controls.audio.requested && !physical_audio_devices_.empty()) ? 1 : 0;
    int expected_video_array_size =
        (controls.video.requested && !physical_video_devices_.empty()) ? 1 : 0;
    EXPECT_CALL(*host_.get(), OnStreamGenerated(render_frame_id,
                                                page_request_id,
                                                expected_audio_array_size,
                                                expected_video_array_size));
    host_->OnGenerateStream(render_frame_id, page_request_id, controls, origin_,
                            run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->audio_devices_));
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->audio_devices_, origin_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void GenerateStreamAndWaitForFailure(
      int render_frame_id,
      int page_request_id,
      const StreamControls& controls,
      MediaStreamRequestResult expected_result) {
      base::RunLoop run_loop;
      EXPECT_CALL(*host_.get(),
                  OnStreamGenerationFailed(render_frame_id,
                                           page_request_id,
                                           expected_result));
      host_->OnGenerateStream(render_frame_id, page_request_id, controls,
                              origin_, run_loop.QuitClosure());
      run_loop.Run();
  }

  void OpenVideoDeviceAndWaitForResult(int render_frame_id,
                                       int page_request_id,
                                       const std::string& device_id) {
    base::RunLoop run_loop;
    host_->OnOpenDevice(render_frame_id, page_request_id, device_id,
                        MEDIA_DEVICE_VIDEO_CAPTURE, origin_,
                        run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_FALSE(DoesContainRawIds(host_->video_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->video_devices_, origin_));
  }

  void EnumerateDevicesAndWaitForResult(int render_frame_id,
                                        int page_request_id,
                                        MediaStreamType type) {
    base::RunLoop run_loop;
    host_->OnEnumerateDevices(render_frame_id, page_request_id, type, origin_,
                              run_loop.QuitClosure());
    run_loop.Run();
    ASSERT_FALSE(host_->enumerated_devices_.empty());
    EXPECT_FALSE(DoesContainRawIds(host_->enumerated_devices_));
    EXPECT_TRUE(DoesEveryDeviceMapToRawId(host_->enumerated_devices_, origin_));
    // Enumeration requests must be cancelled manually.
    host_->OnCancelEnumerateDevices(render_frame_id, page_request_id);
  }

  void SubscribeToDeviceChangeNotificationsAndWaitForNotification(
      int render_frame_id) {
    base::RunLoop run_loop;
    host_->OnSubscribeToDeviceChangeNotifications(render_frame_id, origin_,
                                                  run_loop.QuitClosure());
    // Simulate a change in the set of devices.
    video_capture_device_factory_->set_number_of_devices(5);
    media_stream_manager_->OnDevicesChanged(
        base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);
    run_loop.Run();
  }

  bool DoesContainRawIds(const StreamDeviceInfoArray& devices) {
    for (size_t i = 0; i < devices.size(); ++i) {
      media::AudioDeviceNames::const_iterator audio_it =
          physical_audio_devices_.begin();
      for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
        if (audio_it->unique_id == devices[i].device.id)
          return true;
      }
      media::VideoCaptureDevice::Names::const_iterator video_it =
          physical_video_devices_.begin();
      for (; video_it != physical_video_devices_.end(); ++video_it) {
        if (video_it->id() == devices[i].device.id)
          return true;
      }
    }
    return false;
  }

  bool DoesEveryDeviceMapToRawId(const StreamDeviceInfoArray& devices,
                                 const url::Origin& origin) {
    for (size_t i = 0; i < devices.size(); ++i) {
      bool found_match = false;
      media::AudioDeviceNames::const_iterator audio_it =
          physical_audio_devices_.begin();
      for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
        if (content::DoesMediaDeviceIDMatchHMAC(
                browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
                origin,
                devices[i].device.id,
                audio_it->unique_id)) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      media::VideoCaptureDevice::Names::const_iterator video_it =
          physical_video_devices_.begin();
      for (; video_it != physical_video_devices_.end(); ++video_it) {
        if (content::DoesMediaDeviceIDMatchHMAC(
                browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
                origin,
                devices[i].device.id,
                video_it->id())) {
          EXPECT_FALSE(found_match);
          found_match = true;
        }
      }
      if (!found_match)
        return false;
    }
    return true;
  }

  // Returns true if all devices have labels, false otherwise.
  bool DoesContainLabels(const StreamDeviceInfoArray& devices) {
    for (size_t i = 0; i < devices.size(); ++i) {
      if (devices[i].device.name.empty())
        return false;
    }
    return true;
  }

  // Returns true if no devices have labels, false otherwise.
  bool DoesNotContainLabels(const StreamDeviceInfoArray& devices) {
    for (size_t i = 0; i < devices.size(); ++i) {
      if (!devices[i].device.name.empty())
        return false;
    }
    return true;
  }

  scoped_refptr<MockMediaStreamDispatcherHost> host_;
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<media::AudioManager, media::AudioManagerDeleter>
      audio_manager_;
  MockMediaStreamUIProxy* stream_ui_;
  ContentBrowserClient* old_browser_client_;
  std::unique_ptr<ContentClient> content_client_;
  content::TestBrowserContext browser_context_;
  media::AudioDeviceNames physical_audio_devices_;
  media::VideoCaptureDevice::Names physical_video_devices_;
  url::Origin origin_;
  media::FakeVideoCaptureDeviceFactory* video_capture_device_factory_;
};

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithVideoOnly) {
  StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioOnly) {
  StreamControls controls(true, false);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 0u);
}

// This test simulates a shutdown scenario: we don't setup a fake UI proxy for
// MediaStreamManager, so it will create an ordinary one which will not find
// a RenderFrameHostDelegate. This normally should only be the case at shutdown.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithNothing) {
  StreamControls controls(false, false);

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamWithAudioAndVideo) {
  StreamControls controls(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

// This test generates two streams with video only using the same render frame
// id. The same capture device with the same device and session id is expected
// to be used.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsFromSameRenderId) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().device.id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId + 1, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().device.id;
  int session_id2 = host_->video_devices_.front().session_id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_EQ(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamAndOpenDeviceFromSameRenderId) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().device.id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId, device_id1);

  const std::string device_id2 = host_->opened_device_.device.id;
  const int session_id2 = host_->opened_device_.session_id;
  const std::string label2 = host_->label_;

  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}


// This test generates two streams with video only using two separate render
// frame ids. The same device id but different session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsDifferentRenderId) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label1 = host_->label_;
  const std::string device_id1 = host_->video_devices_.front().device.id;
  const int session_id1 = host_->video_devices_.front().session_id;

  // Generate second stream from another render frame.
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId + 1, kPageRequestId + 1, controls);

  // Check the latest generated stream.
  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);
  const std::string label2 = host_->label_;
  const std::string device_id2 = host_->video_devices_.front().device.id;
  const int session_id2 = host_->video_devices_.front().session_id;
  EXPECT_EQ(device_id1, device_id2);
  EXPECT_NE(session_id1, session_id2);
  EXPECT_NE(label1, label2);
}

// This test request two streams with video only without waiting for the first
// stream to be generated before requesting the second.
// The same device id and session ids are expected.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithoutWaiting) {
  StreamControls controls(false, true);

  // Generate first stream.
  SetupFakeUI(true);
  {
    InSequence s;
    EXPECT_CALL(*host_.get(),
                OnStreamGenerated(kRenderId, kPageRequestId, 0, 1));

    // Generate second stream.
    EXPECT_CALL(*host_.get(),
                OnStreamGenerated(kRenderId, kPageRequestId + 1, 0, 1));
  }
  base::RunLoop run_loop1;
  base::RunLoop run_loop2;
  host_->OnGenerateStream(kRenderId, kPageRequestId, controls, origin_,
                          run_loop1.QuitClosure());
  host_->OnGenerateStream(kRenderId, kPageRequestId + 1, controls, origin_,
                          run_loop2.QuitClosure());

  run_loop1.Run();
  run_loop2.Run();
}

// Test that we can generate streams where a mandatory sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithMandatorySourceId) {
  ASSERT_GE(physical_audio_devices_.size(), 1u);
  ASSERT_GE(physical_video_devices_.size(), 1u);

  media::AudioDeviceNames::const_iterator audio_it =
      physical_audio_devices_.begin();
  for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    StreamControls controls(true, true);
    controls.audio.device_ids.push_back(source_id);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
    EXPECT_EQ(host_->audio_devices_[0].device.id, source_id);
  }

  media::VideoCaptureDevice::Names::const_iterator video_it =
      physical_video_devices_.begin();
  for (; video_it != physical_video_devices_.end(); ++video_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        video_it->id());
    ASSERT_FALSE(source_id.empty());
    StreamControls controls(true, true);
    controls.video.device_ids.push_back(source_id);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
    EXPECT_EQ(host_->video_devices_[0].device.id, source_id);
  }
}

// Test that we can generate streams where a optional sourceId is specified in
// the request.
TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsWithOptionalSourceId) {
  ASSERT_GE(physical_audio_devices_.size(), 1u);
  ASSERT_GE(physical_video_devices_.size(), 1u);

  media::AudioDeviceNames::const_iterator audio_it =
      physical_audio_devices_.begin();
  for (; audio_it != physical_audio_devices_.end(); ++audio_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        audio_it->unique_id);
    ASSERT_FALSE(source_id.empty());
    StreamControls controls(true, true);
    controls.audio.device_ids.push_back(source_id);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
    EXPECT_EQ(host_->audio_devices_[0].device.id, source_id);
  }

  media::VideoCaptureDevice::Names::const_iterator video_it =
      physical_video_devices_.begin();
  for (; video_it != physical_video_devices_.end(); ++video_it) {
    std::string source_id = content::GetHMACForMediaDeviceID(
        browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
        origin_,
        video_it->id());
    ASSERT_FALSE(source_id.empty());
    StreamControls controls(true, true);
    controls.video.device_ids.push_back(source_id);

    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
    EXPECT_EQ(host_->video_devices_[0].device.id, source_id);
  }
}

// Test that generating a stream with an invalid mandatory video source id fail.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidMandatoryVideoSourceId) {
  StreamControls controls(true, true);
  controls.video.device_ids.push_back("invalid source id");

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_NO_HARDWARE);
}

// Test that generating a stream with an invalid mandatory audio source id fail.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidMandatoryAudioSourceId) {
  StreamControls controls(true, true);
  controls.audio.device_ids.push_back("invalid source id");

  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_NO_HARDWARE);
}

// Test that generating a stream with an invalid optional video source id
// succeed.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidOptionalVideoSourceId) {
  StreamControls controls(true, true);
  controls.video.alternate_device_ids.push_back("invalid source id");

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
}

// Test that generating a stream with an invalid optional audio source id
// succeed.
TEST_F(MediaStreamDispatcherHostTest,
       GenerateStreamsWithInvalidOptionalAudioSourceId) {
  StreamControls controls(true, true);
  controls.video.alternate_device_ids.push_back("invalid source id");

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
}

TEST_F(MediaStreamDispatcherHostTest, GenerateStreamsNoAvailableVideoDevice) {
  physical_video_devices_.clear();
  video_capture_device_factory_->set_number_of_devices(0);
  video_capture_device_factory_->GetDeviceNames(&physical_video_devices_);
  StreamControls controls(true, true);

  SetupFakeUI(false);
  GenerateStreamAndWaitForFailure(kRenderId, kPageRequestId, controls,
                                  MEDIA_DEVICE_NO_HARDWARE);
}

// Test that if a OnStopStreamDevice message is received for a device that has
// been opened in a MediaStream and by pepper, the device is only stopped for
// the MediaStream.
TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStream) {
  StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  std::string stream_request_label = host_->label_;
  StreamDeviceInfo video_device_info = host_->video_devices_.front();
  ASSERT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      stream_request_label).size());

  // Open the same device by Pepper.
  OpenVideoDeviceAndWaitForResult(kRenderId, kPageRequestId,
                                  video_device_info.device.id);
  std::string open_device_request_label = host_->label_;

  // Stop the device in the MediaStream.
  host_->OnStopStreamDevice(kRenderId, video_device_info.device.id);

  EXPECT_EQ(0u, media_stream_manager_->GetDevicesOpenedByRequest(
      stream_request_label).size());
  EXPECT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      open_device_request_label).size());
}

TEST_F(MediaStreamDispatcherHostTest, StopDeviceInStreamAndRestart) {
  StreamControls controls(true, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  std::string request_label1 = host_->label_;
  StreamDeviceInfo video_device_info = host_->video_devices_.front();
  // Expect that 1 audio and 1 video device has been opened.
  EXPECT_EQ(2u, media_stream_manager_->GetDevicesOpenedByRequest(
      request_label1).size());

  host_->OnStopStreamDevice(kRenderId, video_device_info.device.id);
  EXPECT_EQ(1u, media_stream_manager_->GetDevicesOpenedByRequest(
      request_label1).size());

  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  std::string request_label2 = host_->label_;

  StreamDeviceInfoArray request1_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label1);
  StreamDeviceInfoArray request2_devices =
      media_stream_manager_->GetDevicesOpenedByRequest(request_label2);

  ASSERT_EQ(1u, request1_devices.size());
  ASSERT_EQ(2u, request2_devices.size());

  // Test that the same audio device has been opened in both streams.
  EXPECT_TRUE(StreamDeviceInfo::IsEqual(request1_devices[0],
                                        request2_devices[0]) ||
              StreamDeviceInfo::IsEqual(request1_devices[0],
                                        request2_devices[1]));
}

TEST_F(MediaStreamDispatcherHostTest,
       GenerateTwoStreamsAndStopDeviceWhileWaitingForSecondStream) {
  StreamControls controls(false, true);

  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  // Generate a second stream.
  EXPECT_CALL(*host_.get(),
              OnStreamGenerated(kRenderId, kPageRequestId + 1, 0, 1));

  base::RunLoop run_loop1;
  host_->OnGenerateStream(kRenderId, kPageRequestId + 1, controls, origin_,
                          run_loop1.QuitClosure());

  // Stop the video stream device from stream 1 while waiting for the
  // second stream to be generated.
  host_->OnStopStreamDevice(kRenderId, host_->video_devices_[0].device.id);
  run_loop1.Run();

  EXPECT_EQ(host_->video_devices_.size(), 1u);
}

TEST_F(MediaStreamDispatcherHostTest, CancelPendingStreamsOnChannelClosing) {
  StreamControls controls(false, true);

  base::RunLoop run_loop;

  // Create multiple GenerateStream requests.
  size_t streams = 5;
  for (size_t i = 1; i <= streams; ++i) {
    host_->OnGenerateStream(kRenderId, kPageRequestId + i, controls, origin_,
                            run_loop.QuitClosure());
  }

  // Calling OnChannelClosing() to cancel all the pending requests.
  host_->OnChannelClosing();
  run_loop.RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, StopGeneratedStreamsOnChannelClosing) {
  StreamControls controls(false, true);

  // Create first group of streams.
  size_t generated_streams = 3;
  for (size_t i = 0; i < generated_streams; ++i) {
    SetupFakeUI(true);
    GenerateStreamAndWaitForResult(kRenderId, kPageRequestId + i, controls);
  }

  // Calling OnChannelClosing() to cancel all the pending/generated streams.
  host_->OnChannelClosing();
  base::RunLoop().RunUntilIdle();
}

TEST_F(MediaStreamDispatcherHostTest, CloseFromUI) {
  StreamControls controls(false, true);

  base::Closure close_callback;
  std::unique_ptr<MockMediaStreamUIProxy> stream_ui(
      new MockMediaStreamUIProxy());
  EXPECT_CALL(*stream_ui, OnStarted(_, _))
      .WillOnce(SaveArg<0>(&close_callback));
  media_stream_manager_->UseFakeUIForTests(std::move(stream_ui));

  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);

  EXPECT_EQ(host_->audio_devices_.size(), 0u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  ASSERT_FALSE(close_callback.is_null());
  EXPECT_CALL(*host_.get(), OnDeviceStopped(kRenderId));
  close_callback.Run();
  base::RunLoop().RunUntilIdle();
}

// Test that the dispatcher is notified if a video device that is in use is
// being unplugged.
TEST_F(MediaStreamDispatcherHostTest, VideoDeviceUnplugged) {
  StreamControls controls(true, true);
  SetupFakeUI(true);
  GenerateStreamAndWaitForResult(kRenderId, kPageRequestId, controls);
  EXPECT_EQ(host_->audio_devices_.size(), 1u);
  EXPECT_EQ(host_->video_devices_.size(), 1u);

  video_capture_device_factory_->set_number_of_devices(0);

  base::RunLoop run_loop;
  EXPECT_CALL(*host_.get(), OnDeviceStopped(kRenderId))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  media_stream_manager_->OnDevicesChanged(
      base::SystemMonitor::DEVTYPE_VIDEO_CAPTURE);

  run_loop.Run();
}

TEST_F(MediaStreamDispatcherHostTest, EnumerateAudioDevices) {
  SetupFakeUI(false);
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_AUDIO_CAPTURE);
  EXPECT_TRUE(DoesContainLabels(host_->enumerated_devices_));
}

TEST_F(MediaStreamDispatcherHostTest, EnumerateVideoDevices) {
  SetupFakeUI(false);
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(DoesContainLabels(host_->enumerated_devices_));
}

TEST_F(MediaStreamDispatcherHostTest, EnumerateAudioDevicesNoAccess) {
  SetupFakeUI(false);
  stream_ui_->SetMicAccess(false);
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_AUDIO_CAPTURE);
  EXPECT_TRUE(DoesNotContainLabels(host_->enumerated_devices_));
}

TEST_F(MediaStreamDispatcherHostTest, EnumerateVideoDevicesNoAccess) {
  SetupFakeUI(false);
  stream_ui_->SetCameraAccess(false);
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_VIDEO_CAPTURE);
  EXPECT_TRUE(DoesNotContainLabels(host_->enumerated_devices_));
}

TEST_F(MediaStreamDispatcherHostTest, DeviceChangeNotification) {
  SetupFakeUI(false);
  // warm up the cache
  EnumerateDevicesAndWaitForResult(kRenderId, kPageRequestId,
                                   MEDIA_DEVICE_VIDEO_CAPTURE);
  SubscribeToDeviceChangeNotificationsAndWaitForNotification(kRenderId);
}

};  // namespace content
