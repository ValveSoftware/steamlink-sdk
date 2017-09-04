// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_AUDIO_INPUT_MAC_H_
#define MEDIA_AUDIO_MAC_AUDIO_INPUT_MAC_H_

#include <AudioToolbox/AudioFormat.h>
#include <AudioToolbox/AudioQueue.h>
#include <stdint.h>

#include <memory>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioBus;
class AudioManagerMac;

// Implementation of AudioInputStream for Mac OS X using the audio queue service
// present in OS 10.5 and later. Design reflects PCMQueueOutAudioOutputStream.
class PCMQueueInAudioInputStream : public AudioInputStream {
 public:
  // Parameters as per AudioManager::MakeAudioInputStream.
  PCMQueueInAudioInputStream(AudioManagerMac* manager,
                             const AudioParameters& params);
  ~PCMQueueInAudioInputStream() override;

  // Implementation of AudioInputStream.
  bool Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;

 private:
  // Issue the OnError to |callback_|;
  void HandleError(OSStatus err);

  // Allocates and prepares the memory that will be used for recording.
  bool SetupBuffers();

  // Sends a buffer to the audio driver for recording.
  OSStatus QueueNextBuffer(AudioQueueBufferRef audio_buffer);

  // Callback from OS, delegates to non-static version below.
  static void HandleInputBufferStatic(
      void* data,
      AudioQueueRef audio_queue,
      AudioQueueBufferRef audio_buffer,
      const AudioTimeStamp* start_time,
      UInt32 num_packets,
      const AudioStreamPacketDescription* desc);

  // Handles callback from OS. Will be called on OS internal thread.
  void HandleInputBuffer(AudioQueueRef audio_queue,
                         AudioQueueBufferRef audio_buffer,
                         const AudioTimeStamp* start_time,
                         UInt32 num_packets,
                         const AudioStreamPacketDescription* packet_desc);

  static const int kNumberBuffers = 3;

  // Manager that owns this stream, used for closing down.
  AudioManagerMac* manager_;
  // We use the callback mostly to periodically supply the recorded audio data.
  AudioInputCallback* callback_;
  // Structure that holds the stream format details such as bitrate.
  AudioStreamBasicDescription format_;
  // Handle to the OS audio queue object.
  AudioQueueRef audio_queue_;
  // Size of each of the buffers in |audio_buffers_|
  uint32_t buffer_size_bytes_;
  // True iff Start() has been called successfully.
  bool started_;
  // Used to determine if we need to slow down |callback_| calls.
  base::TimeTicks last_fill_;
  // Used to defer Start() to workaround http://crbug.com/160920.
  base::CancelableClosure deferred_start_cb_;

  std::unique_ptr<media::AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(PCMQueueInAudioInputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_MAC_AUDIO_INPUT_MAC_H_
