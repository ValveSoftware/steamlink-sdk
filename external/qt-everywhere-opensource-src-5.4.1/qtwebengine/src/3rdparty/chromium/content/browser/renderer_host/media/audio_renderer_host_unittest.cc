// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/sync_socket.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/audio_input_device_manager.h"
#include "content/browser/renderer_host/media/audio_renderer_host.h"
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
using ::testing::DoAll;
using ::testing::NotNull;

namespace {
const int kRenderProcessId = 1;
const int kRenderViewId = 4;
const int kRenderFrameId = 5;
const int kStreamId = 50;
}  // namespace

namespace content {

class MockAudioMirroringManager : public AudioMirroringManager {
 public:
  MockAudioMirroringManager() {}
  virtual ~MockAudioMirroringManager() {}

  MOCK_METHOD3(AddDiverter,
               void(int render_process_id,
                    int render_view_id,
                    Diverter* diverter));
  MOCK_METHOD3(RemoveDiverter,
               void(int render_process_id,
                    int render_view_id,
                    Diverter* diverter));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioMirroringManager);
};

class MockAudioRendererHost : public AudioRendererHost {
 public:
  MockAudioRendererHost(media::AudioManager* audio_manager,
                        AudioMirroringManager* mirroring_manager,
                        MediaInternals* media_internals,
                        MediaStreamManager* media_stream_manager)
      : AudioRendererHost(kRenderProcessId,
                          audio_manager,
                          mirroring_manager,
                          media_internals,
                          media_stream_manager),
        shared_memory_length_(0) {}

  // A list of mock methods.
  MOCK_METHOD2(OnStreamCreated, void(int stream_id, int length));
  MOCK_METHOD1(OnStreamPlaying, void(int stream_id));
  MOCK_METHOD1(OnStreamPaused, void(int stream_id));
  MOCK_METHOD1(OnStreamError, void(int stream_id));

 private:
  virtual ~MockAudioRendererHost() {
    // Make sure all audio streams have been deleted.
    EXPECT_TRUE(audio_entries_.empty());
  }

  // This method is used to dispatch IPC messages to the renderer. We intercept
  // these messages here and dispatch to our mock methods to verify the
  // conversation between this object and the renderer.
  virtual bool Send(IPC::Message* message) {
    CHECK(message);

    // In this method we dispatch the messages to the according handlers as if
    // we are the renderer.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(MockAudioRendererHost, *message)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamCreated,
                          OnNotifyStreamCreated)
      IPC_MESSAGE_HANDLER(AudioMsg_NotifyStreamStateChanged,
                          OnNotifyStreamStateChanged)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    EXPECT_TRUE(handled);

    delete message;
    return true;
  }

  void OnNotifyStreamCreated(int stream_id,
                             base::SharedMemoryHandle handle,
#if defined(OS_WIN)
                             base::SyncSocket::Handle socket_handle,
#else
                             base::FileDescriptor socket_descriptor,
#endif
                             uint32 length) {
    // Maps the shared memory.
    shared_memory_.reset(new base::SharedMemory(handle, false));
    CHECK(shared_memory_->Map(length));
    CHECK(shared_memory_->memory());
    shared_memory_length_ = length;

    // Create the SyncSocket using the handle.
    base::SyncSocket::Handle sync_socket_handle;
#if defined(OS_WIN)
    sync_socket_handle = socket_handle;
#else
    sync_socket_handle = socket_descriptor.fd;
#endif
    sync_socket_.reset(new base::SyncSocket(sync_socket_handle));

    // And then delegate the call to the mock method.
    OnStreamCreated(stream_id, length);
  }

  void OnNotifyStreamStateChanged(int stream_id,
                                  media::AudioOutputIPCDelegate::State state) {
    switch (state) {
      case media::AudioOutputIPCDelegate::kPlaying:
        OnStreamPlaying(stream_id);
        break;
      case media::AudioOutputIPCDelegate::kPaused:
        OnStreamPaused(stream_id);
        break;
      case media::AudioOutputIPCDelegate::kError:
        OnStreamError(stream_id);
        break;
      default:
        FAIL() << "Unknown stream state";
        break;
    }
  }

  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<base::SyncSocket> sync_socket_;
  uint32 shared_memory_length_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererHost);
};

class AudioRendererHostTest : public testing::Test {
 public:
  AudioRendererHostTest() {
    audio_manager_.reset(media::AudioManager::CreateForTesting());
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));
    host_ = new MockAudioRendererHost(audio_manager_.get(),
                                      &mirroring_manager_,
                                      MediaInternals::GetInstance(),
                                      media_stream_manager_.get());

    // Simulate IPC channel connected.
    host_->set_peer_pid_for_testing(base::GetCurrentProcId());
  }

  virtual ~AudioRendererHostTest() {
    // Simulate closing the IPC channel and give the audio thread time to close
    // the underlying streams.
    host_->OnChannelClosing();
    SyncWithAudioThread();

    // Release the reference to the mock object.  The object will be destructed
    // on message_loop_.
    host_ = NULL;
  }

 protected:
  void Create(bool unified_stream) {
    EXPECT_CALL(*host_.get(), OnStreamCreated(kStreamId, _));

    EXPECT_CALL(mirroring_manager_,
                AddDiverter(kRenderProcessId, kRenderViewId, NotNull()))
        .RetiresOnSaturation();

    // Send a create stream message to the audio output stream and wait until
    // we receive the created message.
    int session_id;
    media::AudioParameters params;
    if (unified_stream) {
      // Use AudioInputDeviceManager::kFakeOpenSessionId as the session id to
      // pass the permission check.
      session_id = AudioInputDeviceManager::kFakeOpenSessionId;
      params = media::AudioParameters(
          media::AudioParameters::AUDIO_FAKE,
          media::CHANNEL_LAYOUT_STEREO,
          2,
          media::AudioParameters::kAudioCDSampleRate, 16,
          media::AudioParameters::kAudioCDSampleRate / 10,
          media::AudioParameters::NO_EFFECTS);
    } else {
      session_id = 0;
      params = media::AudioParameters(
          media::AudioParameters::AUDIO_FAKE,
          media::CHANNEL_LAYOUT_STEREO,
          media::AudioParameters::kAudioCDSampleRate, 16,
          media::AudioParameters::kAudioCDSampleRate / 10);
    }
    host_->OnCreateStream(kStreamId, kRenderViewId, kRenderFrameId, session_id,
                          params);

    // At some point in the future, a corresponding RemoveDiverter() call must
    // be made.
    EXPECT_CALL(mirroring_manager_,
                RemoveDiverter(kRenderProcessId, kRenderViewId, NotNull()))
        .RetiresOnSaturation();
    SyncWithAudioThread();
  }

  void Close() {
    // Send a message to AudioRendererHost to tell it we want to close the
    // stream.
    host_->OnCloseStream(kStreamId);
    SyncWithAudioThread();
  }

  void Play() {
    EXPECT_CALL(*host_.get(), OnStreamPlaying(kStreamId));
    host_->OnPlayStream(kStreamId);
    SyncWithAudioThread();
  }

  void Pause() {
    EXPECT_CALL(*host_.get(), OnStreamPaused(kStreamId));
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

 private:
  // MediaStreamManager uses a DestructionObserver, so it must outlive the
  // TestBrowserThreadBundle.
  scoped_ptr<MediaStreamManager> media_stream_manager_;
  TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<media::AudioManager> audio_manager_;
  MockAudioMirroringManager mirroring_manager_;
  scoped_refptr<MockAudioRendererHost> host_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererHostTest);
};

TEST_F(AudioRendererHostTest, CreateAndClose) {
  Create(false);
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreateAndShutdown) {
  Create(false);
}

TEST_F(AudioRendererHostTest, CreatePlayAndClose) {
  Create(false);
  Play();
  Close();
}

TEST_F(AudioRendererHostTest, CreatePlayPauseAndClose) {
  Create(false);
  Play();
  Pause();
  Close();
}

TEST_F(AudioRendererHostTest, SetVolume) {
  Create(false);
  SetVolume(0.5);
  Play();
  Pause();
  Close();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayAndShutdown) {
  Create(false);
  Play();
}

// Simulate the case where a stream is not properly closed.
TEST_F(AudioRendererHostTest, CreatePlayPauseAndShutdown) {
  Create(false);
  Play();
  Pause();
}

TEST_F(AudioRendererHostTest, SimulateError) {
  Create(false);
  Play();
  SimulateError();
}

// Simulate the case when an error is generated on the browser process,
// the audio device is closed but the render process try to close the
// audio stream again.
TEST_F(AudioRendererHostTest, SimulateErrorAndClose) {
  Create(false);
  Play();
  SimulateError();
  Close();
}

TEST_F(AudioRendererHostTest, CreateUnifiedStreamAndClose) {
  Create(true);
  Close();
}

// TODO(hclam): Add tests for data conversation in low latency mode.

}  // namespace content
