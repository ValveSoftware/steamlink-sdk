// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/at_exit.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/sync_socket.h"
#include "base/task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/sample_rates.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::CancelableSyncSocket;
using base::SharedMemory;
using base::SyncSocket;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::WithArgs;
using testing::StrictMock;
using testing::Values;

namespace media {

namespace {

const char kDefaultDeviceId[] = "";
const char kNonDefaultDeviceId[] = "valid-nondefault-device-id";
const char kUnauthorizedDeviceId[] = "unauthorized-device-id";
const int kAuthTimeoutForTestingMs = 500;

class MockRenderCallback : public AudioRendererSink::RenderCallback {
 public:
  MockRenderCallback() {}
  virtual ~MockRenderCallback() {}

  MOCK_METHOD3(Render,
               int(AudioBus* dest,
                   uint32_t frames_delayed,
                   uint32_t frames_skipped));
  MOCK_METHOD0(OnRenderError, void());
};

class MockAudioOutputIPC : public AudioOutputIPC {
 public:
  MockAudioOutputIPC() {}
  virtual ~MockAudioOutputIPC() {}

  MOCK_METHOD4(RequestDeviceAuthorization,
               void(AudioOutputIPCDelegate* delegate,
                    int session_id,
                    const std::string& device_id,
                    const url::Origin& security_origin));
  MOCK_METHOD2(CreateStream,
               void(AudioOutputIPCDelegate* delegate,
                    const AudioParameters& params));
  MOCK_METHOD0(PlayStream, void());
  MOCK_METHOD0(PauseStream, void());
  MOCK_METHOD0(CloseStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
};

ACTION_P2(SendPendingBytes, socket, pending_bytes) {
  socket->Send(&pending_bytes, sizeof(pending_bytes));
}

// Used to terminate a loop from a different thread than the loop belongs to.
// |task_runner| should be a SingleThreadTaskRunner.
ACTION_P(QuitLoop, task_runner) {
  task_runner->PostTask(FROM_HERE, base::MessageLoop::QuitWhenIdleClosure());
}

}  // namespace.

class AudioOutputDeviceTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  AudioOutputDeviceTest();
  ~AudioOutputDeviceTest();

  void ReceiveAuthorization(OutputDeviceStatus device_status);
  void StartAudioDevice();
  void CreateStream();
  void ExpectRenderCallback();
  void WaitUntilRenderCallback();
  void StopAudioDevice();
  void CreateDevice(const std::string& device_id);
  void SetDevice(const std::string& device_id);
  void CheckDeviceStatus(OutputDeviceStatus device_status);

 protected:
  // Used to clean up TLS pointers that the test(s) will initialize.
  // Must remain the first member of this class.
  base::ShadowingAtExitManager at_exit_manager_;
  base::MessageLoopForIO io_loop_;
  AudioParameters default_audio_parameters_;
  StrictMock<MockRenderCallback> callback_;
  MockAudioOutputIPC* audio_output_ipc_;  // owned by audio_device_
  scoped_refptr<AudioOutputDevice> audio_device_;
  OutputDeviceStatus device_status_;

 private:
  int CalculateMemorySize();

  SharedMemory shared_memory_;
  CancelableSyncSocket browser_socket_;
  CancelableSyncSocket renderer_socket_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDeviceTest);
};

int AudioOutputDeviceTest::CalculateMemorySize() {
  // Calculate output memory size.
  return sizeof(AudioOutputBufferParameters) +
         AudioBus::CalculateMemorySize(default_audio_parameters_);
}

AudioOutputDeviceTest::AudioOutputDeviceTest()
    : device_status_(OUTPUT_DEVICE_STATUS_ERROR_INTERNAL) {
  default_audio_parameters_.Reset(AudioParameters::AUDIO_PCM_LINEAR,
                                  CHANNEL_LAYOUT_STEREO, 48000, 16, 1024);
  SetDevice(kDefaultDeviceId);
}

AudioOutputDeviceTest::~AudioOutputDeviceTest() {
  audio_device_ = NULL;
}

void AudioOutputDeviceTest::CreateDevice(const std::string& device_id) {
  audio_output_ipc_ = new MockAudioOutputIPC();
  audio_device_ = new AudioOutputDevice(
      base::WrapUnique(audio_output_ipc_), io_loop_.task_runner(), 0, device_id,
      url::Origin(),
      base::TimeDelta::FromMilliseconds(kAuthTimeoutForTestingMs));
}

void AudioOutputDeviceTest::SetDevice(const std::string& device_id) {
  CreateDevice(device_id);
  EXPECT_CALL(*audio_output_ipc_,
              RequestDeviceAuthorization(audio_device_.get(), 0, device_id, _));
  audio_device_->RequestDeviceAuthorization();
  io_loop_.RunUntilIdle();

  // Simulate response from browser
  OutputDeviceStatus device_status =
      (device_id == kUnauthorizedDeviceId)
          ? OUTPUT_DEVICE_STATUS_ERROR_NOT_AUTHORIZED
          : OUTPUT_DEVICE_STATUS_OK;
  ReceiveAuthorization(device_status);

  audio_device_->Initialize(default_audio_parameters_,
                            &callback_);
}

void AudioOutputDeviceTest::CheckDeviceStatus(OutputDeviceStatus status) {
  DCHECK(!io_loop_.task_runner()->BelongsToCurrentThread());
  EXPECT_EQ(status, audio_device_->GetOutputDeviceInfo().device_status());
}

void AudioOutputDeviceTest::ReceiveAuthorization(OutputDeviceStatus status) {
  device_status_ = status;
  if (device_status_ != OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc_, CloseStream());

  audio_device_->OnDeviceAuthorized(device_status_, default_audio_parameters_,
                                    kDefaultDeviceId);
  io_loop_.RunUntilIdle();
}

void AudioOutputDeviceTest::StartAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc_, CreateStream(audio_device_.get(), _));
  else
    EXPECT_CALL(callback_, OnRenderError());

  audio_device_->Start();
  io_loop_.RunUntilIdle();
}

void AudioOutputDeviceTest::CreateStream() {
  const int kMemorySize = CalculateMemorySize();

  ASSERT_TRUE(shared_memory_.CreateAndMapAnonymous(kMemorySize));
  memset(shared_memory_.memory(), 0xff, kMemorySize);

  ASSERT_TRUE(CancelableSyncSocket::CreatePair(&browser_socket_,
                                               &renderer_socket_));

  // Create duplicates of the handles we pass to AudioOutputDevice since
  // ownership will be transferred and AudioOutputDevice is responsible for
  // freeing.
  SyncSocket::TransitDescriptor audio_device_socket_descriptor;
  ASSERT_TRUE(renderer_socket_.PrepareTransitDescriptor(
      base::GetCurrentProcessHandle(), &audio_device_socket_descriptor));
  base::SharedMemoryHandle duplicated_memory_handle;
  ASSERT_TRUE(shared_memory_.ShareToProcess(base::GetCurrentProcessHandle(),
                                            &duplicated_memory_handle));

  audio_device_->OnStreamCreated(
      duplicated_memory_handle,
      SyncSocket::UnwrapHandle(audio_device_socket_descriptor), kMemorySize);
  io_loop_.RunUntilIdle();
}

void AudioOutputDeviceTest::ExpectRenderCallback() {
  // We should get a 'play' notification when we call OnStreamCreated().
  // Respond by asking for some audio data.  This should ask our callback
  // to provide some audio data that AudioOutputDevice then writes into the
  // shared memory section.
  const int kMemorySize = CalculateMemorySize();

  EXPECT_CALL(*audio_output_ipc_, PlayStream())
      .WillOnce(SendPendingBytes(&browser_socket_, kMemorySize));

  // We expect calls to our audio renderer callback, which returns the number
  // of frames written to the memory section.
  // Here's the second place where it gets hacky:  There's no way for us to
  // know (without using a sleep loop!) when the AudioOutputDevice has finished
  // writing the interleaved audio data into the shared memory section.
  // So, for the sake of this test, we consider the call to Render a sign
  // of success and quit the loop.
  const int kNumberOfFramesToProcess = 0;
  EXPECT_CALL(callback_, Render(_, _, _))
      .WillOnce(DoAll(QuitLoop(io_loop_.task_runner()),
                      Return(kNumberOfFramesToProcess)));
}

void AudioOutputDeviceTest::WaitUntilRenderCallback() {
  // Don't hang the test if we never get the Render() callback.
  io_loop_.task_runner()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      TestTimeouts::action_timeout());
  io_loop_.Run();
}

void AudioOutputDeviceTest::StopAudioDevice() {
  if (device_status_ == OUTPUT_DEVICE_STATUS_OK)
    EXPECT_CALL(*audio_output_ipc_, CloseStream());

  audio_device_->Stop();
  io_loop_.RunUntilIdle();
}

TEST_P(AudioOutputDeviceTest, Initialize) {
  // Tests that the object can be constructed, initialized and destructed
  // without having ever been started.
  StopAudioDevice();
}

// Calls Start() followed by an immediate Stop() and check for the basic message
// filter messages being sent in that case.
TEST_P(AudioOutputDeviceTest, StartStop) {
  StartAudioDevice();
  StopAudioDevice();
}

// AudioOutputDevice supports multiple start/stop sequences.
TEST_P(AudioOutputDeviceTest, StartStopStartStop) {
  StartAudioDevice();
  StopAudioDevice();
  StartAudioDevice();
  StopAudioDevice();
}

// Simulate receiving OnStreamCreated() prior to processing ShutDownOnIOThread()
// on the IO loop.
TEST_P(AudioOutputDeviceTest, StopBeforeRender) {
  StartAudioDevice();

  // Call Stop() but don't run the IO loop yet.
  audio_device_->Stop();

  // Expect us to shutdown IPC but not to render anything despite the stream
  // getting created.
  EXPECT_CALL(*audio_output_ipc_, CloseStream());
  CreateStream();
}

// Full test with output only.
TEST_P(AudioOutputDeviceTest, CreateStream) {
  StartAudioDevice();
  ExpectRenderCallback();
  CreateStream();
  WaitUntilRenderCallback();
  StopAudioDevice();
}

// Full test with output only with nondefault device.
TEST_P(AudioOutputDeviceTest, NonDefaultCreateStream) {
  SetDevice(kNonDefaultDeviceId);
  StartAudioDevice();
  ExpectRenderCallback();
  CreateStream();
  WaitUntilRenderCallback();
  StopAudioDevice();
}

// Multiple start/stop with nondefault device
TEST_P(AudioOutputDeviceTest, NonDefaultStartStopStartStop) {
  SetDevice(kNonDefaultDeviceId);
  StartAudioDevice();
  StopAudioDevice();

  EXPECT_CALL(*audio_output_ipc_,
              RequestDeviceAuthorization(audio_device_.get(), 0, _, _));
  StartAudioDevice();
  // Simulate reply from browser
  ReceiveAuthorization(OUTPUT_DEVICE_STATUS_OK);

  StopAudioDevice();
}

TEST_P(AudioOutputDeviceTest, UnauthorizedDevice) {
  SetDevice(kUnauthorizedDeviceId);
  StartAudioDevice();
  StopAudioDevice();
}

TEST_P(AudioOutputDeviceTest, AuthorizationTimedOut) {
  base::Thread thread("DeviceInfo");
  thread.Start();

  CreateDevice(kNonDefaultDeviceId);
  EXPECT_CALL(*audio_output_ipc_,
              RequestDeviceAuthorization(audio_device_.get(), 0,
                                         kNonDefaultDeviceId, _));
  EXPECT_CALL(*audio_output_ipc_, CloseStream());

  // Request authorization; no reply from the browser.
  audio_device_->RequestDeviceAuthorization();

  media::WaitableMessageLoopEvent event;

  // Request device info on another thread.
  thread.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&AudioOutputDeviceTest::CheckDeviceStatus,
                 base::Unretained(this), OUTPUT_DEVICE_STATUS_ERROR_TIMED_OUT),
      event.GetClosure());

  io_loop_.RunUntilIdle();

  // Runs the loop and waits for |thread| to call event's closure.
  event.RunAndWait();

  audio_device_->Stop();
  io_loop_.RunUntilIdle();
}

INSTANTIATE_TEST_CASE_P(Render, AudioOutputDeviceTest, Values(false));

}  // namespace media.
