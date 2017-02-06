// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SYNC_READER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SYNC_READER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/process/process.h"
#include "base/sync_socket.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/audio_output_controller.h"
#include "media/base/audio_bus.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace base {
class SharedMemory;
}

namespace content {

// A AudioOutputController::SyncReader implementation using SyncSocket. This
// is used by AudioOutputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class AudioSyncReader : public media::AudioOutputController::SyncReader {
 public:
  AudioSyncReader(base::SharedMemory* shared_memory,
                  const media::AudioParameters& params);

  ~AudioSyncReader() override;

  // media::AudioOutputController::SyncReader implementations.
  void UpdatePendingBytes(uint32_t bytes, uint32_t frames_skipped) override;
  void Read(media::AudioBus* dest) override;
  void Close() override;

  bool Init();
  bool PrepareForeignSocket(base::ProcessHandle process_handle,
                            base::SyncSocket::TransitDescriptor* descriptor);

 private:
  // Blocks until data is ready for reading or a timeout expires.  Returns false
  // if an error or timeout occurs.
  bool WaitUntilDataIsReady();

  const base::SharedMemory* const shared_memory_;

  // Mutes all incoming samples. This is used to prevent audible sound
  // during automated testing.
  const bool mute_audio_;

  // Socket for transmitting audio data.
  std::unique_ptr<base::CancelableSyncSocket> socket_;

  // Socket to be used by the renderer. The reference is released after
  // PrepareForeignSocketHandle() is called and ran successfully.
  std::unique_ptr<base::CancelableSyncSocket> foreign_socket_;

  // Shared memory wrapper used for transferring audio data to Read() callers.
  std::unique_ptr<media::AudioBus> output_bus_;

  // Maximum amount of audio data which can be transferred in one Read() call.
  const int packet_size_;

  // Track the number of times the renderer missed its real-time deadline and
  // report a UMA stat during destruction.
  size_t renderer_callback_count_;
  size_t renderer_missed_callback_count_;
  size_t trailing_renderer_missed_callback_count_;

  // The maximum amount of time to wait for data from the renderer.  Calculated
  // from the parameters given at construction.
  const base::TimeDelta maximum_wait_time_;

  // The index of the audio buffer we're expecting to be sent from the renderer;
  // used to block with timeout for audio data.
  uint32_t buffer_index_;

  DISALLOW_COPY_AND_ASSIGN(AudioSyncReader);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_SYNC_READER_H_
