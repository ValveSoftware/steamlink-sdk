// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_renderer_host.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/audio_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Assign;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::NotNull;

namespace content {

namespace {
const int kRenderProcessId = 1;
const int kRenderFrameId = 5;
const int kStreamId = 50;
const char kSecurityOrigin[] = "http://localhost";
const char kBadSecurityOrigin[] = "about:about";
const char kDefaultDeviceId[] = "";
const char kNondefaultDeviceId[] =
    "deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
const char kBadDeviceId[] =
    "badbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbadbad1";
const char kInvalidDeviceId[] = "invalid-device-id";

void ValidateRenderFrameId(int render_process_id,
                           int render_frame_id,
                           const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const bool frame_exists = (render_process_id == kRenderProcessId &&
                             render_frame_id == kRenderFrameId);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, frame_exists));
}

}  // namespace

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MockAudioMirroringManager() {}
  virtual ~MockAudioMirroringManager() {}

  MOCK_METHOD3(AddDiverter,
               void(int render_process_id,
                    int render_frame_id,
                    Diverter* diverter));
  MOCK_METHOD1(RemoveDiverter, void(Diverter* diverter));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioMirroringManager);
};

class MockAudioRendererHost : public AudioRendererHost {
 public:
  MockAudioRendererHost(base::RunLoop* auth_run_loop,
                        media::AudioManager* audio_manager,
                        AudioMirroringManager* mirroring_manager,
                        MediaInternals* media_internals,
                        MediaStreamManager* media_stream_manager,
                        const std::string& salt)
      : AudioRendererHost(kRenderProcessId,
                          audio_manager,
                          mirroring_manager,
                          media_internals,
                          media_stream_manager,
                          salt),
        shared_memory_length_(0),
        auth_run_loop_(auth_run_loop) {
    set_render_frame_id_validate_function_for_testing(&ValidateRenderFrameId);
  }

  // A list of mock methods.
  MOCK_METHOD0(ShutdownForBadMessage, void());
  MOCK_METHOD4(OnDeviceAuthorized,
               void(int stream_id,
                    media::OutputDeviceStatus device_status,
                    const media::AudioParameters& output_params,
                    const std::string& matched_device_id));
  MOCK_METHOD2(OnStreamCreated, void(int stream_id, int length));
  MOCK_METHOD1(OnStreamError, void(int stream_id));

 private:
  virtual ~MockAudioRendererHost() {
    // Make sure all audio streams have been deleted.
    EXPECT_TRUE(audio_entries_.empty());
  }

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  // Note: this means that file descriptors won't be duplicated,
  // leading to double-close errors from SyncSocket.
  // See crbug.com/647659.
  virtual bool Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioRendererHost, *message)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyDeviceAuthorized,
                          OnNotifyDeviceAuthorized)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated,
                          OnNotifyStreamCreated)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamError, OnNotifyStreamError)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  void OnNotifyDeviceAuthorized(int stream_id,
                                media::OutputDeviceStatus device_status,
                                const media::AudioParameters& output_params,
                                const std::string& matched_device_id) {
    OnDeviceAuthorized(stream_id, device_status, output_params,
                       matched_device_id);
    auth_run_loop_->Quit();
  }

  void OnNotifyStreamCreated(
      int stream_id,
      base::SharedMemoryHandle handle,
      base::SyncSocket::TransitDescriptor socket_descriptor,
      uint32_t length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    CHECK(shared_memory_->Map(length));
    CHECK(shared_memory_->memory());
    shared_memory_length_ = length;

    // Create the SyncSocket using the handle.
    base::SyncSocket::Handle sync_socket_handle =
        base::SyncSocket::UnwrapHandle(socket_descriptor);
    sync_socket_.reset(new base::SyncSocket(sync_socket_handle));

    // And then delegate the call to the mock method.
    OnStreamCreated(stream_id, length);
  }

  void OnNotifyStreamError(int stream_id) { OnStreamError(stream_id); }

  std::unique_ptr<base::SharedMemory> shared_memory_;
  std::unique_ptr<base::SyncSocket> sync_socket_;
  uint32_t shared_memory_length_;
  base::RunLoop* auth_run_loop_;  // Used to wait for authorization.

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

class AudioRendererHostTest : public testing::Test {
 public:
  AudioRendererHostTest() {
    audio_manager_ = media::AudioManager::CreateForTesting(
        base::ThreadTaskRunnerHandle::Get());
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    host_ = new MockAudioRendererHost(
        &auth_run_loop_, audio_manager_.get(), &mirroring_manager_,
        MediaInternals::GetInstance(), media_stream_manager_.get(),
        std::string());

    EXPECT_CALL(*host_, ShutdownForBadMessage()).Times(0);

    // Simulate IPC channel connected.
    host_->set_peer_process_for_testing(base::Process::Current());
  }

  ~AudioRendererHostTest() override {
    // Simulate closing the IPC channel and give the audio thread time to close
    // the underlying streams.
    host_->OnChannelClosing();
    SyncWithAudioThread();

    // Release the reference to the mock object.  The object will be destructed
    // on message_loop_.
    host_ = NULL;
  }

 protected:
  void Create() {
    Create(false, kDefaultDeviceId, url::Origin(GURL(kSecurityOrigin)), true);
  }

  void Create(bool unified_stream,
              const std::string& device_id,
              const url::Origin& security_origin,
              bool wait_for_auth) {
    media::OutputDeviceStatus expected_device_status =
        device_id == kDefaultDeviceId
            ? media::OUTPUT_DEVICE_STATUS_OK
            : device_id == kBadDeviceId
                  ? media::OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED
                  : media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND;

    EXPECT_CALL(*host_.get(),
                OnDeviceAuthorized(kStreamId, expected_device_status, _, _));

    if (expected_device_status == media::OUTPUT_DEVICE_STATUS_OK) {
      EXPECT_CALL(*host_.get(), OnStreamCreated(kStreamId, _));
      EXPECT_CALL(mirroring_manager_,
                  AddDiverter(kRenderProcessId, kRenderFrameId, NotNull()))
          .RetiresOnSaturation();
    }

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    media::AudioParameters params(
        media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16,
        media::AudioParameters::kAudioCDSampleRate / 10);
    int session_id = 0;
    if (unified_stream) {
      // Use AudioInputDeviceManager::kFakeOpenSessionId as the session id to
      // pass the permission check.
      session_id = AudioInputDeviceManager::kFakeOpenSessionId;
    }
    host_->OnRequestDeviceAuthorization(kStreamId, kRenderFrameId, session_id,
                                        device_id, security_origin);
    if (wait_for_auth)
      auth_run_loop_.Run();

    if (!wait_for_auth ||
        expected_device_status == media::OUTPUT_DEVICE_STATUS_OK)
      host_->OnCreateStream(kStreamId, kRenderFrameId, params);

    if (expected_device_status == media::OUTPUT_DEVICE_STATUS_OK)
      // At some point in the future, a corresponding RemoveDiverter() call must
      // be made.
      EXPECT_CALL(mirroring_manager_, RemoveDiverter(NotNull()))
          .RetiresOnSaturation();
    SyncWithAudioThread();
  }

  void RequestDeviceAuthorizationWithBadOrigin(const std::string& device_id) {
    int session_id = 0;
    host_->OnRequestDeviceAuthorization(kStreamId, kRenderFrameId, session_id,
                                        device_id,
                                        url::Origin(GURL(kBadSecurityOrigin)));
    SyncWithAudioThread();
  }

  void CreateWithoutWaitingForAuth(const std::string& device_id) {
    Create(false, device_id, url::Origin(GURL(kSecurityOrigin)), false);
  }

  void CreateWithInvalidRenderFrameId() {
    // When creating a stream with an invalid render frame ID, the host will
    // reply with a stream error message.
    EXPECT_CALL(*host_, OnStreamError(kStreamId));

    // However, validation does not block stream creation, so these method calls
    // might be made:
    EXPECT_CALL(*host_, OnStreamCreated(kStreamId, _)).Times(AtLeast(0));
    EXPECT_CALL(mirroring_manager_, AddDiverter(_, _, _)).Times(AtLeast(0));
    EXPECT_CALL(mirroring_manager_, RemoveDiverter(_)).Times(AtLeast(0));

    // Provide a seemingly-valid render frame ID; and it should be rejected when
    // AudioRendererHost calls ValidateRenderFrameId().
    const int kInvalidRenderFrameId = kRenderFrameId + 1;
    const media::AudioParameters params(
        media::AudioParameters::AUDIO_FAKE, media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16,
        media::AudioParameters::kAudioCDSampleRate / 10);
    host_->OnCreateStream(kStreamId, kInvalidRenderFrameId, params);
    base::RunLoop().RunUntilIdle();
  }

  void Close() {
    // Send a message to AudioRendererHost to tell it we want to close the
    // stream.
    host_->OnCloseStream(kStreamId);
    SyncWithAudioThread();
  }

  void Play() {
    host_->OnPlayStream(kStreamId);
    SyncWithAudioThread();
  }

  void Pause() {
    host_->OnPauseStream(kStreamId);
    SyncWithAudioThread();
  }

  void SetVolume(double volume) {
    host_->OnSetVolume(kStreamId, volume);
    SyncWithAudioThread();
  }

  void SimulateError() {
    EXPECT_EQ(1u, host_->audio_entries_.size())
        << "Calls Create() before calling this method";

    // Expect an error signal sent through IPC.
    EXPECT_CALL(*host_.get(), OnStreamError(kStreamId));

    // Simulate an error sent from the audio device.
    host_->ReportErrorAndClose(kStreamId);
    SyncWithAudioThread();

    // Expect the audio stream record is removed.
    EXPECT_EQ(0u, host_->audio_entries_.size());
  }

  // SyncWithAudioThread() waits until all pending tasks on the audio thread
  // are executed while also processing pending task in message_loop_ on the
  // current thread. It is used to synchronize with the audio thread when we are
  // closing an audio stream.
  void SyncWithAudioThread() {
    base::RunLoop().RunUntilIdle();

    base::RunLoop run_loop;
    audio_manager_->GetTaskRunner()->PostTask(
        FROM_HERE, media::BindToCurrentLoop(run_loop.QuitClosure()));
    run_loop.Run();
  }

  void ExpectShutdown() {
    EXPECT_CALL(*host_, ShutdownForBadMessage()).Times(1);
  }

 private:
  // MediaStreamManager uses a DestructionObserver, so it must outlive the
  // TestBrowserThreadBundle.
  std::unique_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  media::ScopedAudioManagerPtr audio_manager_;
  MockAudioMirroringManager mirroring_manager_;
  scoped_refptr<MockAudioRendererHost> host_;
  base::RunLoop auth_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHostTest);
};

TEST_F(AudioRendererHostTest, CreateAndClose) {
  Create();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreateAndShutdown) {
  Create();
}

TEST_F(AudioRendererHostTest, CreatePlayAndClose) {
  Create();
  Play();
  Close();
}

TEST_F(AudioRendererHostTest, CreatePlayPauseAndClose) {
  Create();
  Play();
  Pause();
  Close();
}

TEST_F(AudioRendererHostTest, SetVolume) {
  Create();
  SetVolume(0.5);
  Play();
  Pause();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayAndShutdown) {
  Create();
  Play();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayPauseAndShutdown) {
  Create();
  Play();
  Pause();
}

TEST_F(AudioRendererHostTest, SimulateError) {
  Create();
  Play();
  SimulateError();
}

// Simulate the case when an error is generated on the browser process,
// the audio device is closed but the render process try to close the
// audio stream again.
TEST_F(AudioRendererHostTest, SimulateErrorAndClose) {
  Create();
  Play();
  SimulateError();
  Close();
}

TEST_F(AudioRendererHostTest, CreateUnifiedStreamAndClose) {
  Create(true, kDefaultDeviceId, url::Origin(GURL(kSecurityOrigin)), true);
  Close();
}

TEST_F(AudioRendererHostTest, CreateUnauthorizedDevice) {
  Create(false, kBadDeviceId, url::Origin(GURL(kSecurityOrigin)), true);
  Close();
}

TEST_F(AudioRendererHostTest, CreateDeviceWithAuthorizationPendingIsError) {
  ExpectShutdown();
  CreateWithoutWaitingForAuth(kBadDeviceId);
  Close();
}

TEST_F(AudioRendererHostTest, CreateDeviceWithBadSecurityOrigin) {
  ExpectShutdown();
  RequestDeviceAuthorizationWithBadOrigin(kNondefaultDeviceId);
  Close();
}

TEST_F(AudioRendererHostTest, CreateInvalidDevice) {
  Create(false, kInvalidDeviceId, url::Origin(GURL(kSecurityOrigin)), true);
  Close();
}

TEST_F(AudioRendererHostTest, CreateFailsForInvalidRenderFrame) {
  CreateWithInvalidRenderFrameId();
  Close();
}

// TODO(hclam): Add tests for data conversation in low latency mode.

}  // namespace content
