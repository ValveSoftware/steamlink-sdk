// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_sync_reader.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/common/content_switches.h"
#include "media/base/audio_parameters.h"

using media::AudioBus;
using media::AudioOutputBuffer;

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum AudioGlitchResult {
  AUDIO_RENDERER_NO_AUDIO_GLITCHES = 0,
  AUDIO_RENDERER_AUDIO_GLITCHES = 1,
  AUDIO_RENDERER_AUDIO_GLITCHES_MAX = AUDIO_RENDERER_AUDIO_GLITCHES
};

void LogAudioGlitchResult(AudioGlitchResult result) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioRendererAudioGlitches",
                            result,
                            AUDIO_RENDERER_AUDIO_GLITCHES_MAX + 1);
}

}  // namespace

namespace content {

AudioSyncReader::AudioSyncReader(base::SharedMemory* shared_memory,
                                 const media::AudioParameters& params)
    : shared_memory_(shared_memory),
      mute_audio_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMuteAudio)),
      packet_size_(shared_memory_->requested_size()),
      renderer_callback_count_(0),
      renderer_missed_callback_count_(0),
      trailing_renderer_missed_callback_count_(0),
#if defined(OS_MACOSX)
      maximum_wait_time_(params.GetBufferDuration() / 2),
#else
      // TODO(dalecurtis): Investigate if we can reduce this on all platforms.
      maximum_wait_time_(base::TimeDelta::FromMilliseconds(20)),
#endif
      buffer_index_(0) {
  DCHECK_EQ(static_cast<size_t>(packet_size_),
            sizeof(media::AudioOutputBufferParameters) +
                AudioBus::CalculateMemorySize(params));
  AudioOutputBuffer* buffer =
      reinterpret_cast<AudioOutputBuffer*>(shared_memory_->memory());
  output_bus_ = AudioBus::WrapMemory(params, buffer->audio);
  output_bus_->Zero();
}

AudioSyncReader::~AudioSyncReader() {
  if (!renderer_callback_count_)
    return;

  DVLOG(1) << "Trailing glitch count on destruction: "
           << trailing_renderer_missed_callback_count_;

  // Subtract 'trailing' count of callbacks missed just before the destructor
  // call. This happens if the renderer process was killed or e.g. the page
  // refreshed while the output device was open etc.
  // This trims off the end of both the missed and total counts so that we
  // preserve the proportion of counts before the teardown period.
  DCHECK_LE(trailing_renderer_missed_callback_count_,
            renderer_missed_callback_count_);
  DCHECK_LE(trailing_renderer_missed_callback_count_, renderer_callback_count_);

  renderer_missed_callback_count_ -= trailing_renderer_missed_callback_count_;
  renderer_callback_count_ -= trailing_renderer_missed_callback_count_;

  if (!renderer_callback_count_)
    return;

  // Recording the percentage of deadline misses gives us a rough overview of
  // how many users might be running into audio glitches.
  int percentage_missed =
      100.0 * renderer_missed_callback_count_ / renderer_callback_count_;
  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioRendererMissedDeadline", percentage_missed);

  // Add more detailed information regarding detected audio glitches where
  // a non-zero value of |renderer_missed_callback_count_| is added to the
  // AUDIO_RENDERER_AUDIO_GLITCHES bin.
  renderer_missed_callback_count_ > 0 ?
      LogAudioGlitchResult(AUDIO_RENDERER_AUDIO_GLITCHES) :
      LogAudioGlitchResult(AUDIO_RENDERER_NO_AUDIO_GLITCHES);
  std::string log_string =
      base::StringPrintf("ASR: number of detected audio glitches=%d",
                         static_cast<int>(renderer_missed_callback_count_));
  MediaStreamManager::SendMessageToNativeLog(log_string);
  DVLOG(1) << log_string;
}

// media::AudioOutputController::SyncReader implementations.
void AudioSyncReader::UpdatePendingBytes(uint32_t bytes,
                                         uint32_t frames_skipped) {
  // Increase the number of skipped frames stored in shared memory. We don't
  // send it over the socket since sending more than 4 bytes might lead to being
  // descheduled. The reading side will zero it when consumed.
  AudioOutputBuffer* buffer =
      reinterpret_cast<AudioOutputBuffer*>(shared_memory_->memory());
  buffer->params.frames_skipped += frames_skipped;

  // Zero out the entire output buffer to avoid stuttering/repeating-buffers
  // in the anomalous case if the renderer is unable to keep up with real-time.
  output_bus_->Zero();

  socket_->Send(&bytes, sizeof(bytes));
  ++buffer_index_;
}

void AudioSyncReader::Read(AudioBus* dest) {
  ++renderer_callback_count_;
  if (!WaitUntilDataIsReady()) {
    ++trailing_renderer_missed_callback_count_;
    ++renderer_missed_callback_count_;
    if (renderer_missed_callback_count_ <= 100) {
      LOG(WARNING) << "AudioSyncReader::Read timed out, audio glitch count="
                   << renderer_missed_callback_count_;
      if (renderer_missed_callback_count_ == 100)
        LOG(WARNING) << "(log cap reached, suppressing further logs)";
    }
    dest->Zero();
    return;
  }

  trailing_renderer_missed_callback_count_ = 0;

  if (mute_audio_)
    dest->Zero();
  else
    output_bus_->CopyTo(dest);
}

void AudioSyncReader::Close() {
  socket_->Close();
}

bool AudioSyncReader::Init() {
  socket_.reset(new base::CancelableSyncSocket());
  foreign_socket_.reset(new base::CancelableSyncSocket());
  return base::CancelableSyncSocket::CreatePair(socket_.get(),
                                                foreign_socket_.get());
}

bool AudioSyncReader::PrepareForeignSocket(
    base::ProcessHandle process_handle,
    base::SyncSocket::TransitDescriptor* descriptor) {
  return foreign_socket_->PrepareTransitDescriptor(process_handle, descriptor);
}

bool AudioSyncReader::WaitUntilDataIsReady() {
  base::TimeDelta timeout = maximum_wait_time_;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeTicks finish_time = start_time + timeout;

  // Check if data is ready and if not, wait a reasonable amount of time for it.
  //
  // Data readiness is achieved via parallel counters, one on the renderer side
  // and one here.  Every time a buffer is requested via UpdatePendingBytes(),
  // |buffer_index_| is incremented.  Subsequently every time the renderer has a
  // buffer ready it increments its counter and sends the counter value over the
  // SyncSocket.  Data is ready when |buffer_index_| matches the counter value
  // received from the renderer.
  //
  // The counter values may temporarily become out of sync if the renderer is
  // unable to deliver audio fast enough.  It's assumed that the renderer will
  // catch up at some point, which means discarding counter values read from the
  // SyncSocket which don't match our current buffer index.
  size_t bytes_received = 0;
  uint32_t renderer_buffer_index = 0;
  while (timeout.InMicroseconds() > 0) {
    bytes_received = socket_->ReceiveWithTimeout(
        &renderer_buffer_index, sizeof(renderer_buffer_index), timeout);
    if (bytes_received != sizeof(renderer_buffer_index)) {
      bytes_received = 0;
      break;
    }

    if (renderer_buffer_index == buffer_index_)
      break;

    // Reduce the timeout value as receives succeed, but aren't the right index.
    timeout = finish_time - base::TimeTicks::Now();
  }

  // Receive timed out or another error occurred.  Receive can timeout if the
  // renderer is unable to deliver audio data within the allotted time.
  if (!bytes_received || renderer_buffer_index != buffer_index_) {
    DVLOG(2) << "AudioSyncReader::WaitUntilDataIsReady() timed out.";

    base::TimeDelta time_since_start = base::TimeTicks::Now() - start_time;
    UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioOutputControllerDataNotReady",
                               time_since_start,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(1000),
                               50);
    return false;
  }

  return true;
}

}  // namespace content
