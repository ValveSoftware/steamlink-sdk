// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <algorithm>

#include "base/format_macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"

using media::AudioBus;
using media::AudioInputBuffer;
using media::AudioInputBufferParameters;

namespace content {

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum AudioGlitchResult {
  AUDIO_CAPTURER_NO_AUDIO_GLITCHES = 0,
  AUDIO_CAPTURER_AUDIO_GLITCHES = 1,
  AUDIO_CAPTURER_AUDIO_GLITCHES_MAX = AUDIO_CAPTURER_AUDIO_GLITCHES
};

}  // namespace

AudioInputSyncWriter::AudioInputSyncWriter(void* shared_memory,
                                           size_t shared_memory_size,
                                           int shared_memory_segment_count,
                                           const media::AudioParameters& params)
    : shared_memory_(static_cast<uint8_t*>(shared_memory)),
      shared_memory_segment_count_(shared_memory_segment_count),
      current_segment_id_(0),
      creation_time_(base::Time::Now()),
      audio_bus_memory_size_(AudioBus::CalculateMemorySize(params)),
      next_buffer_id_(0),
      next_read_buffer_index_(0),
      number_of_filled_segments_(0),
      write_count_(0),
      write_to_fifo_count_(0),
      write_error_count_(0),
      trailing_write_to_fifo_count_(0),
      trailing_write_error_count_(0) {
  DCHECK_GT(shared_memory_segment_count, 0);
  DCHECK_EQ(shared_memory_size % shared_memory_segment_count, 0u);
  shared_memory_segment_size_ =
      shared_memory_size / shared_memory_segment_count;
  DVLOG(1) << "shared_memory_size: " << shared_memory_size;
  DVLOG(1) << "shared_memory_segment_count: " << shared_memory_segment_count;
  DVLOG(1) << "audio_bus_memory_size: " << audio_bus_memory_size_;

  // Create vector of audio buses by wrapping existing blocks of memory.
  uint8_t* ptr = shared_memory_;
  for (int i = 0; i < shared_memory_segment_count; ++i) {
    CHECK_EQ(0U, reinterpret_cast<uintptr_t>(ptr) &
        (AudioBus::kChannelAlignment - 1));
    AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);
    std::unique_ptr<AudioBus> audio_bus =
        AudioBus::WrapMemory(params, buffer->audio);
    audio_buses_.push_back(std::move(audio_bus));
    ptr += shared_memory_segment_size_;
  }
}

AudioInputSyncWriter::~AudioInputSyncWriter() {
  // We log the following:
  // - Percentage of data written to fifo (and not to shared memory).
  // - Percentage of data dropped (fifo reached max size or socket buffer full).
  // - Glitch yes/no (at least 1 drop).
  //
  // Subtract 'trailing' counts that will happen if the renderer process was
  // killed or e.g. the page refreshed while the input device was open etc.
  // This trims off the end of both the error and write counts so that we
  // preserve the proportion of counts before the teardown period. We pick
  // the largest trailing count as the time we consider that the trailing errors
  // begun, and subract that from the total write count.
  DCHECK_LE(trailing_write_to_fifo_count_, write_to_fifo_count_);
  DCHECK_LE(trailing_write_to_fifo_count_, write_count_);
  DCHECK_LE(trailing_write_error_count_, write_error_count_);
  DCHECK_LE(trailing_write_error_count_, write_count_);

  write_to_fifo_count_ -= trailing_write_to_fifo_count_;
  write_error_count_ -= trailing_write_error_count_;
  write_count_ -= std::max(trailing_write_to_fifo_count_,
                           trailing_write_error_count_);

  if (write_count_ == 0)
    return;

  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioCapturerMissedReadDeadline",
      100.0 * write_to_fifo_count_ / write_count_);

  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioCapturerDroppedData",
      100.0 * write_error_count_ / write_count_);

  UMA_HISTOGRAM_ENUMERATION("Media.AudioCapturerAudioGlitches",
                            write_error_count_ == 0 ?
                                AUDIO_CAPTURER_NO_AUDIO_GLITCHES :
                                AUDIO_CAPTURER_AUDIO_GLITCHES,
                            AUDIO_CAPTURER_AUDIO_GLITCHES_MAX + 1);

  std::string log_string = base::StringPrintf(
      "AISW: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      write_error_count_, write_count_);
  MediaStreamManager::SendMessageToNativeLog(log_string);
  DVLOG(1) << log_string;
}

void AudioInputSyncWriter::Write(const AudioBus* data,
                                 double volume,
                                 bool key_pressed,
                                 uint32_t hardware_delay_bytes) {
  TRACE_EVENT0("audio", "AudioInputSyncWriter::Write");
  ++write_count_;
  CheckTimeSinceLastWrite();

  // Check that the renderer side has read data so that we don't overwrite data
  // that hasn't been read yet. The renderer side sends a signal over the socket
  // each time it has read data. Here, we read those verifications before
  // writing. We verify that each buffer index is in sequence.
  size_t number_of_indices_available = socket_->Peek() / sizeof(uint32_t);
  if (number_of_indices_available > 0) {
    std::unique_ptr<uint32_t[]> indices(
        new uint32_t[number_of_indices_available]);
    size_t bytes_received = socket_->Receive(
        &indices[0],
        number_of_indices_available * sizeof(indices[0]));
    DCHECK_EQ(number_of_indices_available * sizeof(indices[0]), bytes_received);
    for (size_t i = 0; i < number_of_indices_available; ++i) {
      ++next_read_buffer_index_;
      CHECK_EQ(indices[i], next_read_buffer_index_);
      --number_of_filled_segments_;
      CHECK_GE(number_of_filled_segments_, 0);
    }
  }

  bool write_error = !WriteDataFromFifoToSharedMemory();

  // Write the current data to the shared memory if there is room, otherwise
  // put it in the fifo.
  if (number_of_filled_segments_ <
      static_cast<int>(shared_memory_segment_count_)) {
    WriteParametersToCurrentSegment(volume, key_pressed, hardware_delay_bytes);

    // Copy data into shared memory using pre-allocated audio buses.
    AudioBus* audio_bus = audio_buses_[current_segment_id_];
    data->CopyTo(audio_bus);

    if (!SignalDataWrittenAndUpdateCounters())
      write_error = true;

    trailing_write_to_fifo_count_ = 0;
  } else {
    if (!PushDataToFifo(data, volume, key_pressed, hardware_delay_bytes))
      write_error = true;

    ++write_to_fifo_count_;
    ++trailing_write_to_fifo_count_;
  }

  // Increase write error counts if error, or reset the trailing error counter
  // if all write operations went well (no data dropped).
  if (write_error) {
    ++write_error_count_;
    ++trailing_write_error_count_;
    TRACE_EVENT_INSTANT0("audio", "AudioInputSyncWriter write error",
                         TRACE_EVENT_SCOPE_THREAD);
  } else {
    trailing_write_error_count_ = 0;
  }
}

void AudioInputSyncWriter::Close() {
  socket_->Close();
}

bool AudioInputSyncWriter::Init() {
  socket_.reset(new base::CancelableSyncSocket());
  foreign_socket_.reset(new base::CancelableSyncSocket());
  return base::CancelableSyncSocket::CreatePair(socket_.get(),
                                                foreign_socket_.get());
}

bool AudioInputSyncWriter::PrepareForeignSocket(
    base::ProcessHandle process_handle,
    base::SyncSocket::TransitDescriptor* descriptor) {
  return foreign_socket_->PrepareTransitDescriptor(process_handle, descriptor);
}

void AudioInputSyncWriter::CheckTimeSinceLastWrite() {
#if !defined(OS_ANDROID)
  static const base::TimeDelta kLogDelayThreadhold =
      base::TimeDelta::FromMilliseconds(500);

  std::ostringstream oss;
  if (last_write_time_.is_null()) {
    // This is the first time Write is called.
    base::TimeDelta interval = base::Time::Now() - creation_time_;
    oss << "AISW::Write: audio input data received for the first time: delay "
           "= " << interval.InMilliseconds() << "ms";
  } else {
    base::TimeDelta interval = base::Time::Now() - last_write_time_;
    if (interval > kLogDelayThreadhold) {
      oss << "AISW::Write: audio input data delay unexpectedly long: delay = "
          << interval.InMilliseconds() << "ms";
    }
  }
  if (!oss.str().empty()) {
    AddToNativeLog(oss.str());
    DVLOG(1) << oss.str();
  }

  last_write_time_ = base::Time::Now();
#endif
}

void AudioInputSyncWriter::AddToNativeLog(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog(message);
}

bool AudioInputSyncWriter::PushDataToFifo(const AudioBus* data,
                                          double volume,
                                          bool key_pressed,
                                          uint32_t hardware_delay_bytes) {
  if (overflow_buses_.size() == kMaxOverflowBusesSize) {
    // We use |write_error_count_| for capping number of log messages.
    // |write_error_count_| also includes socket Send() errors, but those should
    // be rare.
    if (write_error_count_ <= 50) {
      const std::string error_message = "AISW: No room in fifo.";
      LOG(WARNING) << error_message;
      AddToNativeLog(error_message);
      if (write_error_count_ == 50) {
        const std::string error_message =
            "AISW: Log cap reached, suppressing further fifo overflow logs.";
        LOG(WARNING) << error_message;
        AddToNativeLog(error_message);
      }
    }
    return false;
  }

  if (overflow_buses_.empty()) {
    const std::string message = "AISW: Starting to use fifo.";
    DVLOG(1) << message;
    AddToNativeLog(message);
  }

  // Push parameters to fifo.
  OverflowParams params = { volume, hardware_delay_bytes, key_pressed };
  overflow_params_.push_back(params);

  // Push audio data to fifo.
  std::unique_ptr<AudioBus> audio_bus =
      AudioBus::Create(data->channels(), data->frames());
  data->CopyTo(audio_bus.get());
  overflow_buses_.push_back(std::move(audio_bus));

  DCHECK_LE(overflow_buses_.size(), static_cast<size_t>(kMaxOverflowBusesSize));
  DCHECK_EQ(overflow_params_.size(), overflow_buses_.size());

  return true;
}

bool AudioInputSyncWriter::WriteDataFromFifoToSharedMemory() {
  if (overflow_buses_.empty())
    return true;

  const int segment_count = static_cast<int>(shared_memory_segment_count_);
  bool write_error = false;
  auto params_it = overflow_params_.begin();
  auto audio_bus_it = overflow_buses_.begin();
  DCHECK_EQ(overflow_params_.size(), overflow_buses_.size());

  while (audio_bus_it != overflow_buses_.end() &&
         number_of_filled_segments_ < segment_count) {
    // Write parameters to shared memory.
    WriteParametersToCurrentSegment((*params_it).volume,
                                    (*params_it).key_pressed,
                                    (*params_it).hardware_delay_bytes);

    // Copy data from the fifo into shared memory using pre-allocated audio
    // buses.
    (*audio_bus_it)->CopyTo(audio_buses_[current_segment_id_]);

    if (!SignalDataWrittenAndUpdateCounters())
      write_error = true;

    ++params_it;
    ++audio_bus_it;
  }

  // Erase all copied data from fifo.
  overflow_params_.erase(overflow_params_.begin(), params_it);
  overflow_buses_.erase(overflow_buses_.begin(), audio_bus_it);

  if (overflow_buses_.empty()) {
    const std::string message = "AISW: Fifo emptied.";
    DVLOG(1) << message;
    AddToNativeLog(message);
  }

  DCHECK_EQ(overflow_params_.size(), overflow_buses_.size());
  return !write_error;
}

void AudioInputSyncWriter::WriteParametersToCurrentSegment(
    double volume,
    bool key_pressed,
    uint32_t hardware_delay_bytes) {
  uint8_t* ptr = shared_memory_;
  ptr += current_segment_id_ * shared_memory_segment_size_;
  AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);
  buffer->params.volume = volume;
  buffer->params.size = audio_bus_memory_size_;
  buffer->params.key_pressed = key_pressed;
  buffer->params.hardware_delay_bytes = hardware_delay_bytes;
  buffer->params.id = next_buffer_id_;
}

bool AudioInputSyncWriter::SignalDataWrittenAndUpdateCounters() {
  if (socket_->Send(&current_segment_id_, sizeof(current_segment_id_)) !=
      sizeof(current_segment_id_)) {
    const std::string error_message = "AISW: No room in socket buffer.";
    LOG(WARNING) << error_message;
    AddToNativeLog(error_message);
    TRACE_EVENT_INSTANT0("audio",
                         "AudioInputSyncWriter: No room in socket buffer",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (++current_segment_id_ >= shared_memory_segment_count_)
    current_segment_id_ = 0;
  ++number_of_filled_segments_;
  CHECK_LE(number_of_filled_segments_,
           static_cast<int>(shared_memory_segment_count_));
  ++next_buffer_id_;

  return true;
}

}  // namespace content
