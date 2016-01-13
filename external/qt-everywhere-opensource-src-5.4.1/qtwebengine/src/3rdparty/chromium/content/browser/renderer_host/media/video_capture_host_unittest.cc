// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/media_stream_requester.h"
#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"
#include "content/browser/renderer_host/media/video_capture_host.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/video_capture_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture_types.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace content {

// Id used to identify the capture session between renderer and
// video_capture_host. This is an arbitrary value.
static const int kDeviceId = 555;

// Define to enable test where video is dumped to file.
// #define DUMP_VIDEO

// Define to use a real video capture device.
// #define TEST_REAL_CAPTURE_DEVICE

// Simple class used for dumping video to a file. This can be used for
// verifying the output.
class DumpVideo {
 public:
  DumpVideo() : expected_size_(0) {}
  void StartDump(int width, int height) {
    base::FilePath file_name = base::FilePath(base::StringPrintf(
        FILE_PATH_LITERAL("dump_w%d_h%d.yuv"), width, height));
    file_.reset(base::OpenFile(file_name, "wb"));
    expected_size_ = media::VideoFrame::AllocationSize(
        media::VideoFrame::I420, gfx::Size(width, height));
  }
  void NewVideoFrame(const void* buffer) {
    if (file_.get() != NULL) {
      ASSERT_EQ(1U, fwrite(buffer, expected_size_, 1, file_.get()));
    }
  }

 private:
  base::ScopedFILE file_;
  int expected_size_;
};

class MockMediaStreamRequester : public MediaStreamRequester {
 public:
  MockMediaStreamRequester() {}
  virtual ~MockMediaStreamRequester() {}

  // MediaStreamRequester implementation.
  MOCK_METHOD5(StreamGenerated,
               void(int render_view_id,
                    int page_request_id,
                    const std::string& label,
                    const StreamDeviceInfoArray& audio_devices,
                    const StreamDeviceInfoArray& video_devices));
  MOCK_METHOD3(StreamGenerationFailed,
      void(int render_view_id,
           int page_request_id,
           content::MediaStreamRequestResult result));
  MOCK_METHOD3(DeviceStopped, void(int render_view_id,
                                   const std::string& label,
                                   const StreamDeviceInfo& device));
  MOCK_METHOD4(DevicesEnumerated, void(int render_view_id,
                                       int page_request_id,
                                       const std::string& label,
                                       const StreamDeviceInfoArray& devices));
  MOCK_METHOD4(DeviceOpened, void(int render_view_id,
                                  int page_request_id,
                                  const std::string& label,
                                  const StreamDeviceInfo& device_info));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamRequester);
};

class MockVideoCaptureHost : public VideoCaptureHost {
 public:
  MockVideoCaptureHost(MediaStreamManager* manager)
      : VideoCaptureHost(manager),
        return_buffers_(false),
        dump_video_(false) {}

  // A list of mock methods.
  MOCK_METHOD4(OnNewBufferCreated,
               void(int device_id,
                    base::SharedMemoryHandle handle,
                    int length,
                    int buffer_id));
  MOCK_METHOD2(OnBufferFreed,
               void(int device_id, int buffer_id));
  MOCK_METHOD4(OnBufferFilled,
               void(int device_id,
                    int buffer_id,
                    const media::VideoCaptureFormat& format,
                    base::TimeTicks timestamp));
  MOCK_METHOD5(OnMailboxBufferFilled,
               void(int device_id,
                    int buffer_id,
                    const gpu::MailboxHolder& mailbox_holder,
                    const media::VideoCaptureFormat& format,
                    base::TimeTicks timestamp));
  MOCK_METHOD2(OnStateChanged, void(int device_id, VideoCaptureState state));

  // Use class DumpVideo to write I420 video to file.
  void SetDumpVideo(bool enable) {
    dump_video_ = enable;
  }

  void SetReturnReceivedDibs(bool enable) {
    return_buffers_ = enable;
  }

  // Return Dibs we currently have received.
  void ReturnReceivedDibs(int device_id)  {
    int handle = GetReceivedDib();
    while (handle) {
      this->OnReceiveEmptyBuffer(device_id, handle, std::vector<uint32>());
      handle = GetReceivedDib();
    }
  }

  int GetReceivedDib() {
    if (filled_dib_.empty())
      return 0;
    std::map<int, base::SharedMemory*>::iterator it = filled_dib_.begin();
    int h = it->first;
    delete it->second;
    filled_dib_.erase(it);

    return h;
  }

 private:
  virtual ~MockVideoCaptureHost() {
    STLDeleteContainerPairSecondPointers(filled_dib_.begin(),
                                         filled_dib_.end());
  }

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) OVERRIDE {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockVideoCaptureHost, *message)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_NewBuffer, OnNewBufferCreatedDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_FreeBuffer, OnBufferFreedDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_BufferReady, OnBufferFilledDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_MailboxBufferReady,
                          OnMailboxBufferFilledDispatch)
      IPC_MESSAGE_HANDLER(VideoCaptureMsg_StateChanged, OnStateChangedDispatch)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  // These handler methods do minimal things and delegate to the mock methods.
  void OnNewBufferCreatedDispatch(int device_id,
                                  base::SharedMemoryHandle handle,
                                  uint32 length,
                                  int buffer_id) {
    OnNewBufferCreated(device_id, handle, length, buffer_id);
    base::SharedMemory* dib = new base::SharedMemory(handle, false);
    dib->Map(length);
    filled_dib_[buffer_id] = dib;
  }

  void OnBufferFreedDispatch(int device_id, int buffer_id) {
    OnBufferFreed(device_id, buffer_id);

    std::map<int, base::SharedMemory*>::iterator it =
        filled_dib_.find(buffer_id);
    ASSERT_TRUE(it != filled_dib_.end());
    delete it->second;
    filled_dib_.erase(it);
  }

  void OnBufferFilledDispatch(int device_id,
                              int buffer_id,
                              const media::VideoCaptureFormat& frame_format,
                              base::TimeTicks timestamp) {
    base::SharedMemory* dib = filled_dib_[buffer_id];
    ASSERT_TRUE(dib != NULL);
    if (dump_video_) {
      if (!format_.IsValid()) {
        dumper_.StartDump(frame_format.frame_size.width(),
                          frame_format.frame_size.height());
        format_ = frame_format;
      }
      ASSERT_EQ(format_.frame_size.width(), frame_format.frame_size.width())
          << "Dump format does not handle variable resolution.";
      ASSERT_EQ(format_.frame_size.height(), frame_format.frame_size.height())
          << "Dump format does not handle variable resolution.";
      dumper_.NewVideoFrame(dib->memory());
    }

    OnBufferFilled(device_id, buffer_id, frame_format, timestamp);
    if (return_buffers_) {
      VideoCaptureHost::OnReceiveEmptyBuffer(
          device_id, buffer_id, std::vector<uint32>());
    }
  }

  void OnMailboxBufferFilledDispatch(int device_id,
                                     int buffer_id,
                                     const gpu::MailboxHolder& mailbox_holder,
                                     const media::VideoCaptureFormat& format,
                                     base::TimeTicks timestamp) {
    OnMailboxBufferFilled(
        device_id, buffer_id, mailbox_holder, format, timestamp);
    if (return_buffers_) {
      VideoCaptureHost::OnReceiveEmptyBuffer(
          device_id, buffer_id, std::vector<uint32>());
    }
  }

  void OnStateChangedDispatch(int device_id, VideoCaptureState state) {
    OnStateChanged(device_id, state);
  }

  std::map<int, base::SharedMemory*> filled_dib_;
  bool return_buffers_;
  bool dump_video_;
  media::VideoCaptureFormat format_;
  DumpVideo dumper_;
};

ACTION_P2(ExitMessageLoop, message_loop, quit_closure) {
  message_loop->PostTask(FROM_HERE, quit_closure);
}

// This is an integration test of VideoCaptureHost in conjunction with
// MediaStreamManager, VideoCaptureManager, VideoCaptureController, and
// VideoCaptureDevice.
class VideoCaptureHostTest : public testing::Test {
 public:
  VideoCaptureHostTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        message_loop_(base::MessageLoopProxy::current()),
        opened_session_id_(kInvalidMediaCaptureSessionId) {}

  virtual void SetUp() OVERRIDE {
    SetBrowserClientForTesting(&browser_client_);
    // Create our own MediaStreamManager.
    audio_manager_.reset(media::AudioManager::CreateForTesting());
#ifndef TEST_REAL_CAPTURE_DEVICE
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
#endif
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    media_stream_manager_->UseFakeUI(scoped_ptr<FakeMediaStreamUIProxy>());

    // Create a Host and connect it to a simulated IPC channel.
    host_ = new MockVideoCaptureHost(media_stream_manager_.get());
    host_->OnChannelConnected(base::GetCurrentProcId());

    OpenSession();
  }

  virtual void TearDown() OVERRIDE {
    // Verifies and removes the expectations on host_ and
    // returns true iff successful.
    Mock::VerifyAndClearExpectations(host_.get());
    EXPECT_EQ(0u, host_->entries_.size());

    CloseSession();

    // Simulate closing the IPC sender.
    host_->OnChannelClosing();

    // Release the reference to the mock object. The object will be destructed
    // on the current message loop.
    host_ = NULL;
  }

  void OpenSession() {
    const int render_process_id = 1;
    const int render_view_id = 1;
    const int page_request_id = 1;
    const GURL security_origin("http://test.com");

    ASSERT_TRUE(opened_device_label_.empty());

    // Enumerate video devices.
    StreamDeviceInfoArray devices;
    {
      base::RunLoop run_loop;
      std::string label = media_stream_manager_->EnumerateDevices(
          &stream_requester_,
          render_process_id,
          render_view_id,
          browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
          page_request_id,
          MEDIA_DEVICE_VIDEO_CAPTURE,
          security_origin,
          true);
      EXPECT_CALL(stream_requester_, DevicesEnumerated(render_view_id,
                                                       page_request_id,
                                                       label,
                                                       _))
          .Times(1).WillOnce(
              DoAll(ExitMessageLoop(message_loop_, run_loop.QuitClosure()),
                    SaveArg<3>(&devices)));
      run_loop.Run();
      Mock::VerifyAndClearExpectations(&stream_requester_);
      media_stream_manager_->CancelRequest(label);
    }
    ASSERT_FALSE(devices.empty());
    ASSERT_EQ(StreamDeviceInfo::kNoId, devices[0].session_id);

    // Open the first device.
    {
      base::RunLoop run_loop;
      StreamDeviceInfo opened_device;
      media_stream_manager_->OpenDevice(
          &stream_requester_,
          render_process_id,
          render_view_id,
          browser_context_.GetResourceContext()->GetMediaDeviceIDSalt(),
          page_request_id,
          devices[0].device.id,
          MEDIA_DEVICE_VIDEO_CAPTURE,
          security_origin);
      EXPECT_CALL(stream_requester_, DeviceOpened(render_view_id,
                                                  page_request_id,
                                                  _,
                                                  _))
          .Times(1).WillOnce(
              DoAll(ExitMessageLoop(message_loop_, run_loop.QuitClosure()),
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
  void StartCapture() {
    EXPECT_CALL(*host_, OnNewBufferCreated(kDeviceId, _, _, _))
        .Times(AnyNumber()).WillRepeatedly(Return());

    base::RunLoop run_loop;
    EXPECT_CALL(*host_, OnBufferFilled(kDeviceId, _, _, _))
        .Times(AnyNumber()).WillOnce(ExitMessageLoop(
            message_loop_, run_loop.QuitClosure()));

    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);
    host_->OnStartCapture(kDeviceId, opened_session_id_, params);
    run_loop.Run();
  }

  void StartStopCapture() {
    // Quickly start and then stop capture, without giving much chance for
    // asynchronous start operations to complete.
    InSequence s;
    base::RunLoop run_loop;
    EXPECT_CALL(*host_, OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STOPPED));
    media::VideoCaptureParams params;
    params.requested_format = media::VideoCaptureFormat(
        gfx::Size(352, 288), 30, media::PIXEL_FORMAT_I420);
    host_->OnStartCapture(kDeviceId, opened_session_id_, params);
    host_->OnStopCapture(kDeviceId);
    run_loop.RunUntilIdle();
  }

#ifdef DUMP_VIDEO
  void CaptureAndDumpVideo(int width, int height, int frame_rate) {
    InSequence s;
    EXPECT_CALL(*host_.get(), OnNewBufferCreated(kDeviceId, _, _, _))
        .Times(AnyNumber()).WillRepeatedly(Return());

    base::RunLoop run_loop;
    EXPECT_CALL(*host_, OnBufferFilled(kDeviceId, _, _, _))
        .Times(AnyNumber())
        .WillOnce(ExitMessageLoop(message_loop_, run_loop.QuitClosure()));

    media::VideoCaptureParams params;
    params.requested_format =
        media::VideoCaptureFormat(gfx::Size(width, height), frame_rate);
    host_->SetDumpVideo(true);
    host_->OnStartCapture(kDeviceId, opened_session_id_, params);
    run_loop.Run();
  }
#endif

  void StopCapture() {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_, OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STOPPED))
        .WillOnce(ExitMessageLoop(message_loop_, run_loop.QuitClosure()));

    host_->OnStopCapture(kDeviceId);
    host_->SetReturnReceivedDibs(true);
    host_->ReturnReceivedDibs(kDeviceId);

    run_loop.Run();

    host_->SetReturnReceivedDibs(false);
    // Expect the VideoCaptureDevice has been stopped
    EXPECT_EQ(0u, host_->entries_.size());
  }

  void NotifyPacketReady() {
    base::RunLoop run_loop;
    EXPECT_CALL(*host_, OnBufferFilled(kDeviceId, _, _, _))
        .Times(AnyNumber()).WillOnce(ExitMessageLoop(
            message_loop_, run_loop.QuitClosure()))
        .RetiresOnSaturation();
    run_loop.Run();
  }

  void ReturnReceivedPackets() {
    host_->ReturnReceivedDibs(kDeviceId);
  }

  void SimulateError() {
    // Expect a change state to error state sent through IPC.
    EXPECT_CALL(*host_, OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_ERROR))
        .Times(1);
    VideoCaptureControllerID id(kDeviceId);
    host_->OnError(id);
    // Wait for the error callback.
    base::RunLoop().RunUntilIdle();
  }

  scoped_refptr<MockVideoCaptureHost> host_;

 private:
  StrictMock<MockMediaStreamRequester> stream_requester_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestBrowserContext browser_context_;
  content::TestContentBrowserClient browser_client_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  int opened_session_id_;
  std::string opened_device_label_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureHostTest);
};

TEST_F(VideoCaptureHostTest, CloseSessionWithoutStopping) {
  StartCapture();

  // When the session is closed via the stream without stopping capture, the
  // ENDED event is sent.
  EXPECT_CALL(*host_, OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_ENDED))
      .Times(1);
  CloseSession();
  base::RunLoop().RunUntilIdle();
}

TEST_F(VideoCaptureHostTest, StopWhileStartPending) {
  StartStopCapture();
}

TEST_F(VideoCaptureHostTest, StartCapturePlayStop) {
  StartCapture();
  NotifyPacketReady();
  NotifyPacketReady();
  ReturnReceivedPackets();
  StopCapture();
}

TEST_F(VideoCaptureHostTest, StartCaptureErrorStop) {
  StartCapture();
  SimulateError();
  StopCapture();
}

TEST_F(VideoCaptureHostTest, StartCaptureError) {
  EXPECT_CALL(*host_, OnStateChanged(kDeviceId, VIDEO_CAPTURE_STATE_STOPPED))
      .Times(0);
  StartCapture();
  NotifyPacketReady();
  SimulateError();
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
}

#ifdef DUMP_VIDEO
TEST_F(VideoCaptureHostTest, CaptureAndDumpVideoVga) {
  CaptureAndDumpVideo(640, 480, 30);
}
TEST_F(VideoCaptureHostTest, CaptureAndDump720P) {
  CaptureAndDumpVideo(1280, 720, 30);
}
#endif

}  // namespace content
