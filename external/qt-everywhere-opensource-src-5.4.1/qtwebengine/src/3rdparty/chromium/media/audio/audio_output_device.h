// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Audio rendering unit utilizing audio output stream provided by browser
// process through IPC.
//
// Relationship of classes.
//
//  AudioOutputController                AudioOutputDevice
//           ^                                ^
//           |                                |
//           v               IPC              v
//    AudioRendererHost  <---------> AudioOutputIPC (AudioMessageFilter)
//
// Transportation of audio samples from the render to the browser process
// is done by using shared memory in combination with a sync socket pair
// to generate a low latency transport. The AudioOutputDevice user registers an
// AudioOutputDevice::RenderCallback at construction and will be polled by the
// AudioOutputDevice for audio to be played out by the underlying audio layers.
//
// State sequences.
//
//            Task [IO thread]                  IPC [IO thread]
//
// Start -> CreateStreamOnIOThread -----> CreateStream ------>
//       <- OnStreamCreated <- AudioMsg_NotifyStreamCreated <-
//       ---> PlayOnIOThread -----------> PlayStream -------->
//
// Optionally Play() / Pause() sequences may occur:
// Play -> PlayOnIOThread --------------> PlayStream --------->
// Pause -> PauseOnIOThread ------------> PauseStream -------->
// (note that Play() / Pause() sequences before OnStreamCreated are
//  deferred until OnStreamCreated, with the last valid state being used)
//
// AudioOutputDevice::Render => audio transport on audio thread =>
//                               |
// Stop --> ShutDownOnIOThread -------->  CloseStream -> Close
//
// This class utilizes several threads during its lifetime, namely:
// 1. Creating thread.
//    Must be the main render thread.
// 2. Control thread (may be the main render thread or another thread).
//    The methods: Start(), Stop(), Play(), Pause(), SetVolume()
//    must be called on the same thread.
// 3. IO thread (internal implementation detail - not exposed to public API)
//    The thread within which this class receives all the IPC messages and
//    IPC communications can only happen in this thread.
// 4. Audio transport thread (See AudioDeviceThread).
//    Responsible for calling the AudioThreadCallback implementation that in
//    turn calls AudioRendererSink::RenderCallback which feeds audio samples to
//    the audio layer in the browser process using sync sockets and shared
//    memory.
//
// Implementation notes:
// - The user must call Stop() before deleting the class instance.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "media/audio/audio_device_thread.h"
#include "media/audio/audio_output_ipc.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/scoped_task_runner_observer.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AudioOutputDevice
    : NON_EXPORTED_BASE(public AudioRendererSink),
      NON_EXPORTED_BASE(public AudioOutputIPCDelegate),
      NON_EXPORTED_BASE(public ScopedTaskRunnerObserver) {
 public:
  // NOTE: Clients must call Initialize() before using.
  AudioOutputDevice(
      scoped_ptr<AudioOutputIPC> ipc,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);

  // Initialize function for clients wishing to have unified input and
  // output, |params| may specify |input_channels| > 0, representing a
  // number of input channels which will be at the same sample-rate
  // and buffer-size as the output as specified in |params|. |session_id| is
  // used for the browser to select the correct input device.
  void InitializeWithSessionId(const AudioParameters& params,
                               RenderCallback* callback,
                               int session_id);

  // AudioRendererSink implementation.
  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* callback) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Play() OVERRIDE;
  virtual void Pause() OVERRIDE;
  virtual bool SetVolume(double volume) OVERRIDE;

  // Methods called on IO thread ----------------------------------------------
  // AudioOutputIPCDelegate methods.
  virtual void OnStateChanged(AudioOutputIPCDelegate::State state) OVERRIDE;
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               int length) OVERRIDE;
  virtual void OnIPCClosed() OVERRIDE;

 protected:
  // Magic required by ref_counted.h to avoid any code deleting the object
  // accidentally while there are references to it.
  friend class base::RefCountedThreadSafe<AudioOutputDevice>;
  virtual ~AudioOutputDevice();

 private:
  // Note: The ordering of members in this enum is critical to correct behavior!
  enum State {
    IPC_CLOSED,  // No more IPCs can take place.
    IDLE,  // Not started.
    CREATING_STREAM,  // Waiting for OnStreamCreated() to be called back.
    PAUSED,  // Paused.  OnStreamCreated() has been called.  Can Play()/Stop().
    PLAYING,  // Playing back.  Can Pause()/Stop().
  };

  // Methods called on IO thread ----------------------------------------------
  // The following methods are tasks posted on the IO thread that need to
  // be executed on that thread.  They use AudioOutputIPC to send IPC messages
  // upon state changes.
  void CreateStreamOnIOThread(const AudioParameters& params);
  void PlayOnIOThread();
  void PauseOnIOThread();
  void ShutDownOnIOThread();
  void SetVolumeOnIOThread(double volume);

  // base::MessageLoop::DestructionObserver implementation for the IO loop.
  // If the IO loop dies before we do, we shut down the audio thread from here.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  AudioParameters audio_parameters_;

  RenderCallback* callback_;

  // A pointer to the IPC layer that takes care of sending requests over to
  // the AudioRendererHost.  Only valid when state_ != IPC_CLOSED and must only
  // be accessed on the IO thread.
  scoped_ptr<AudioOutputIPC> ipc_;

  // Current state (must only be accessed from the IO thread).  See comments for
  // State enum above.
  State state_;

  // State of Play() / Pause() calls before OnStreamCreated() is called.
  bool play_on_start_;

  // The media session ID used to identify which input device to be started.
  // Only used by Unified IO.
  int session_id_;

  // Our audio thread callback class.  See source file for details.
  class AudioThreadCallback;

  // In order to avoid a race between OnStreamCreated and Stop(), we use this
  // guard to control stopping and starting the audio thread.
  base::Lock audio_thread_lock_;
  AudioDeviceThread audio_thread_;
  scoped_ptr<AudioOutputDevice::AudioThreadCallback> audio_callback_;

  // Temporary hack to ignore OnStreamCreated() due to the user calling Stop()
  // so we don't start the audio thread pointing to a potentially freed
  // |callback_|.
  //
  // TODO(scherkus): Replace this by changing AudioRendererSink to either accept
  // the callback via Start(). See http://crbug.com/151051 for details.
  bool stopping_hack_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputDevice);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DEVICE_H_
