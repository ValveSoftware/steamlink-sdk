// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_

#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

// Contains IPC notifications for the state of the server side
// (AudioOutputController) audio state changes and when an AudioOutputController
// has been created.  Implemented by AudioOutputDevice.
class MEDIA_EXPORT AudioOutputIPCDelegate {
 public:
  // Current status of the audio output stream in the browser process. Browser
  // sends information about the current playback state and error to the
  // renderer process using this type.
  enum State {
    kPlaying,
    kPaused,
    kError,
    kStateLast = kError
  };

  // Called when state of an audio stream has changed.
  virtual void OnStateChanged(State state) = 0;

  // Called when an audio stream has been created.
  // The shared memory |handle| points to a memory section that's used to
  // transfer audio buffers from the AudioOutputIPCDelegate back to the
  // AudioRendererHost.  The implementation of OnStreamCreated takes ownership.
  // The |socket_handle| is used by AudioRendererHost to signal requests for
  // audio data to be written into the shared memory. The AudioOutputIPCDelegate
  // must read from this socket and provide audio whenever data (search for
  // "pending_bytes") is received.
  virtual void OnStreamCreated(base::SharedMemoryHandle handle,
                               base::SyncSocket::Handle socket_handle,
                               int length) = 0;

  // Called when the AudioOutputIPC object is going away and/or when the IPC
  // channel has been closed and no more ipc requests can be made.
  // Implementations should delete their owned AudioOutputIPC instance
  // immediately.
  virtual void OnIPCClosed() = 0;

 protected:
  virtual ~AudioOutputIPCDelegate();
};

// Provides the IPC functionality for an AudioOutputIPCDelegate (e.g., an
// AudioOutputDevice).  The implementation should asynchronously deliver the
// messages to an AudioOutputController object (or create one in the case of
// CreateStream()), that may live in a separate process.
class MEDIA_EXPORT AudioOutputIPC {
 public:
  virtual ~AudioOutputIPC();

  // Sends a request to create an AudioOutputController object in the peer
  // process and configures it to use the specified audio |params| including
  // number of synchronized input channels.|session_id| is used by the browser
  // to select the correct input device if the input channel in |params| is
  // valid, otherwise it will be ignored.  Once the stream has been created,
  // the implementation will notify |delegate| by calling OnStreamCreated().
  virtual void CreateStream(AudioOutputIPCDelegate* delegate,
                            const AudioParameters& params,
                            int session_id) = 0;

  // Starts playing the stream.  This should generate a call to
  // AudioOutputController::Play().
  virtual void PlayStream() = 0;

  // Pauses an audio stream.  This should generate a call to
  // AudioOutputController::Pause().
  virtual void PauseStream() = 0;

  // Closes the audio stream which should shut down the corresponding
  // AudioOutputController in the peer process.
  virtual void CloseStream() = 0;

  // Sets the volume of the audio stream.
  virtual void SetVolume(double volume) = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_IPC_H_
