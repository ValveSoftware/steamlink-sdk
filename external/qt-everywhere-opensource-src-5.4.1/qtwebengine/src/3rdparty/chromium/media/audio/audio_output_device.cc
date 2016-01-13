// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_device.h"

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "media/audio/audio_output_controller.h"
#include "media/base/limits.h"

namespace media {

// Takes care of invoking the render callback on the audio thread.
// An instance of this class is created for each capture stream in
// OnStreamCreated().
class AudioOutputDevice::AudioThreadCallback
    : public AudioDeviceThread::Callback {
 public:
  AudioThreadCallback(const AudioParameters& audio_parameters,
                      base::SharedMemoryHandle memory,
                      int memory_length,
                      AudioRendererSink::RenderCallback* render_callback);
  virtual ~AudioThreadCallback();

  virtual void MapSharedMemory() OVERRIDE;

  // Called whenever we receive notifications about pending data.
  virtual void Process(int pending_data) OVERRIDE;

 private:
  AudioRendererSink::RenderCallback* render_callback_;
  scoped_ptr<AudioBus> output_bus_;
  DISALLOW_COPY_AND_ASSIGN(AudioThreadCallback);
};

AudioOutputDevice::AudioOutputDevice(
    scoped_ptr<AudioOutputIPC> ipc,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : ScopedTaskRunnerObserver(io_task_runner),
      callback_(NULL),
      ipc_(ipc.Pass()),
      state_(IDLE),
      play_on_start_(true),
      session_id_(-1),
      stopping_hack_(false) {
  CHECK(ipc_);

  // The correctness of the code depends on the relative values assigned in the
  // State enum.
  COMPILE_ASSERT(IPC_CLOSED < IDLE, invalid_enum_value_assignment_0);
  COMPILE_ASSERT(IDLE < CREATING_STREAM, invalid_enum_value_assignment_1);
  COMPILE_ASSERT(CREATING_STREAM < PAUSED, invalid_enum_value_assignment_2);
  COMPILE_ASSERT(PAUSED < PLAYING, invalid_enum_value_assignment_3);
}

void AudioOutputDevice::InitializeWithSessionId(const AudioParameters& params,
                                                RenderCallback* callback,
                                                int session_id) {
  DCHECK(!callback_) << "Calling InitializeWithSessionId() twice?";
  DCHECK(params.IsValid());
  audio_parameters_ = params;
  callback_ = callback;
  session_id_ = session_id;
}

void AudioOutputDevice::Initialize(const AudioParameters& params,
                                   RenderCallback* callback) {
  InitializeWithSessionId(params, callback, 0);
}

AudioOutputDevice::~AudioOutputDevice() {
  // The current design requires that the user calls Stop() before deleting
  // this class.
  DCHECK(audio_thread_.IsStopped());
}

void AudioOutputDevice::Start() {
  DCHECK(callback_) << "Initialize hasn't been called";
  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::CreateStreamOnIOThread, this,
                 audio_parameters_));
}

void AudioOutputDevice::Stop() {
  {
    base::AutoLock auto_lock(audio_thread_lock_);
    audio_thread_.Stop(base::MessageLoop::current());
    stopping_hack_ = true;
  }

  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::ShutDownOnIOThread, this));
}

void AudioOutputDevice::Play() {
  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::PlayOnIOThread, this));
}

void AudioOutputDevice::Pause() {
  task_runner()->PostTask(FROM_HERE,
      base::Bind(&AudioOutputDevice::PauseOnIOThread, this));
}

bool AudioOutputDevice::SetVolume(double volume) {
  if (volume < 0 || volume > 1.0)
    return false;

  if (!task_runner()->PostTask(FROM_HERE,
          base::Bind(&AudioOutputDevice::SetVolumeOnIOThread, this, volume))) {
    return false;
  }

  return true;
}

void AudioOutputDevice::CreateStreamOnIOThread(const AudioParameters& params) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (state_ == IDLE) {
    state_ = CREATING_STREAM;
    ipc_->CreateStream(this, params, session_id_);
  }
}

void AudioOutputDevice::PlayOnIOThread() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (state_ == PAUSED) {
    ipc_->PlayStream();
    state_ = PLAYING;
    play_on_start_ = false;
  } else {
    play_on_start_ = true;
  }
}

void AudioOutputDevice::PauseOnIOThread() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (state_ == PLAYING) {
    ipc_->PauseStream();
    state_ = PAUSED;
  }
  play_on_start_ = false;
}

void AudioOutputDevice::ShutDownOnIOThread() {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // Close the stream, if we haven't already.
  if (state_ >= CREATING_STREAM) {
    ipc_->CloseStream();
    state_ = IDLE;
  }

  // We can run into an issue where ShutDownOnIOThread is called right after
  // OnStreamCreated is called in cases where Start/Stop are called before we
  // get the OnStreamCreated callback.  To handle that corner case, we call
  // Stop(). In most cases, the thread will already be stopped.
  //
  // Another situation is when the IO thread goes away before Stop() is called
  // in which case, we cannot use the message loop to close the thread handle
  // and can't rely on the main thread existing either.
  base::AutoLock auto_lock_(audio_thread_lock_);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  audio_thread_.Stop(NULL);
  audio_callback_.reset();
  stopping_hack_ = false;
}

void AudioOutputDevice::SetVolumeOnIOThread(double volume) {
  DCHECK(task_runner()->BelongsToCurrentThread());
  if (state_ >= CREATING_STREAM)
    ipc_->SetVolume(volume);
}

void AudioOutputDevice::OnStateChanged(AudioOutputIPCDelegate::State state) {
  DCHECK(task_runner()->BelongsToCurrentThread());

  // Do nothing if the stream has been closed.
  if (state_ < CREATING_STREAM)
    return;

  // TODO(miu): Clean-up inconsistent and incomplete handling here.
  // http://crbug.com/180640
  switch (state) {
    case AudioOutputIPCDelegate::kPlaying:
    case AudioOutputIPCDelegate::kPaused:
      break;
    case AudioOutputIPCDelegate::kError:
      DLOG(WARNING) << "AudioOutputDevice::OnStateChanged(kError)";
      // Don't dereference the callback object if the audio thread
      // is stopped or stopping.  That could mean that the callback
      // object has been deleted.
      // TODO(tommi): Add an explicit contract for clearing the callback
      // object.  Possibly require calling Initialize again or provide
      // a callback object via Start() and clear it in Stop().
      if (!audio_thread_.IsStopped())
        callback_->OnRenderError();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AudioOutputDevice::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length) {
  DCHECK(task_runner()->BelongsToCurrentThread());
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK_GT(length, 0);

  if (state_ != CREATING_STREAM)
    return;

  // We can receive OnStreamCreated() on the IO thread after the client has
  // called Stop() but before ShutDownOnIOThread() is processed. In such a
  // situation |callback_| might point to freed memory. Instead of starting
  // |audio_thread_| do nothing and wait for ShutDownOnIOThread() to get called.
  //
  // TODO(scherkus): The real fix is to have sane ownership semantics. The fact
  // that |callback_| (which should own and outlive this object!) can point to
  // freed memory is a mess. AudioRendererSink should be non-refcounted so that
  // owners (WebRtcAudioDeviceImpl, AudioRendererImpl, etc...) can Stop() and
  // delete as they see fit. AudioOutputDevice should internally use WeakPtr
  // to handle teardown and thread hopping. See http://crbug.com/151051 for
  // details.
  base::AutoLock auto_lock(audio_thread_lock_);
  if (stopping_hack_)
    return;

  DCHECK(audio_thread_.IsStopped());
  audio_callback_.reset(new AudioOutputDevice::AudioThreadCallback(
      audio_parameters_, handle, length, callback_));
  audio_thread_.Start(
      audio_callback_.get(), socket_handle, "AudioOutputDevice", true);
  state_ = PAUSED;

  // We handle the case where Play() and/or Pause() may have been called
  // multiple times before OnStreamCreated() gets called.
  if (play_on_start_)
    PlayOnIOThread();
}

void AudioOutputDevice::OnIPCClosed() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  state_ = IPC_CLOSED;
  ipc_.reset();
}

void AudioOutputDevice::WillDestroyCurrentMessageLoop() {
  LOG(ERROR) << "IO loop going away before the audio device has been stopped";
  ShutDownOnIOThread();
}

// AudioOutputDevice::AudioThreadCallback

AudioOutputDevice::AudioThreadCallback::AudioThreadCallback(
    const AudioParameters& audio_parameters,
    base::SharedMemoryHandle memory,
    int memory_length,
    AudioRendererSink::RenderCallback* render_callback)
    : AudioDeviceThread::Callback(audio_parameters, memory, memory_length, 1),
      render_callback_(render_callback) {}

AudioOutputDevice::AudioThreadCallback::~AudioThreadCallback() {
}

void AudioOutputDevice::AudioThreadCallback::MapSharedMemory() {
  CHECK_EQ(total_segments_, 1);
  CHECK(shared_memory_.Map(memory_length_));
  DCHECK_EQ(memory_length_, AudioBus::CalculateMemorySize(audio_parameters_));

  output_bus_ =
      AudioBus::WrapMemory(audio_parameters_, shared_memory_.memory());
}

// Called whenever we receive notifications about pending data.
void AudioOutputDevice::AudioThreadCallback::Process(int pending_data) {
  // Negative |pending_data| indicates the browser side stream has stopped.
  if (pending_data < 0)
    return;

  // Convert the number of pending bytes in the render buffer into milliseconds.
  int audio_delay_milliseconds = pending_data / bytes_per_ms_;

  TRACE_EVENT0("audio", "AudioOutputDevice::FireRenderCallback");

  // Update the audio-delay measurement then ask client to render audio.  Since
  // |output_bus_| is wrapping the shared memory the Render() call is writing
  // directly into the shared memory.
  render_callback_->Render(output_bus_.get(), audio_delay_milliseconds);
}

}  // namespace media.
