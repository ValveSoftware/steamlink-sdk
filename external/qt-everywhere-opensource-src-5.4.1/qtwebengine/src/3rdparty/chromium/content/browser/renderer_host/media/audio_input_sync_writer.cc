// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <algorithm>

#include "base/memory/shared_memory.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"

using media::AudioBus;

namespace content {

AudioInputSyncWriter::AudioInputSyncWriter(base::SharedMemory* shared_memory,
                                           int shared_memory_segment_count,
                                           const media::AudioParameters& params)
    : shared_memory_(shared_memory),
      shared_memory_segment_count_(shared_memory_segment_count),
      current_segment_id_(0),
      creation_time_(base::Time::Now()),
      audio_bus_memory_size_(AudioBus::CalculateMemorySize(params)) {
  DCHECK_GT(shared_memory_segment_count, 0);
  DCHECK_EQ(shared_memory->requested_size() % shared_memory_segment_count, 0u);
  shared_memory_segment_size_ =
      shared_memory->requested_size() / shared_memory_segment_count;
  DVLOG(1) << "SharedMemory::requested_size: "
           << shared_memory->requested_size();
  DVLOG(1) << "shared_memory_segment_count: " << shared_memory_segment_count;
  DVLOG(1) << "audio_bus_memory_size: " << audio_bus_memory_size_;

  // Create vector of audio buses by wrapping existing blocks of memory.
  uint8* ptr = static_cast<uint8*>(shared_memory_->memory());
  for (int i = 0; i < shared_memory_segment_count; ++i) {
    media::AudioInputBuffer* buffer =
        reinterpret_cast<media::AudioInputBuffer*>(ptr);
    scoped_ptr<media::AudioBus> audio_bus =
        media::AudioBus::WrapMemory(params, buffer->audio);
    audio_buses_.push_back(audio_bus.release());
    ptr += shared_memory_segment_size_;
  }
}

AudioInputSyncWriter::~AudioInputSyncWriter() {}

// TODO(henrika): Combine into one method (including Write).
void AudioInputSyncWriter::UpdateRecordedBytes(uint32 bytes) {
  socket_->Send(&bytes, sizeof(bytes));
}

void AudioInputSyncWriter::Write(const media::AudioBus* data,
                                 double volume,
                                 bool key_pressed) {
#if !defined(OS_ANDROID)
  static const base::TimeDelta kLogDelayThreadhold =
      base::TimeDelta::FromMilliseconds(500);

  std::ostringstream oss;
  if (last_write_time_.is_null()) {
    // This is the first time Write is called.
    base::TimeDelta interval = base::Time::Now() - creation_time_;
    oss << "Audio input data received for the first time: delay = "
        << interval.InMilliseconds() << "ms.";

  } else {
    base::TimeDelta interval = base::Time::Now() - last_write_time_;
    if (interval > kLogDelayThreadhold) {
      oss << "Audio input data delay unexpectedly long: delay = "
          << interval.InMilliseconds() << "ms.";
    }
  }
  if (!oss.str().empty())
    MediaStreamManager::SendMessageToNativeLog(oss.str());

  last_write_time_ = base::Time::Now();
#endif

  // Write audio parameters to shared memory.
  uint8* ptr = static_cast<uint8*>(shared_memory_->memory());
  ptr += current_segment_id_ * shared_memory_segment_size_;
  media::AudioInputBuffer* buffer =
      reinterpret_cast<media::AudioInputBuffer*>(ptr);
  buffer->params.volume = volume;
  buffer->params.size = audio_bus_memory_size_;
  buffer->params.key_pressed = key_pressed;

  // Copy data from the native audio layer into shared memory using pre-
  // allocated audio buses.
  media::AudioBus* audio_bus = audio_buses_[current_segment_id_];
  data->CopyTo(audio_bus);

  if (++current_segment_id_ >= shared_memory_segment_count_)
    current_segment_id_ = 0;
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

#if defined(OS_WIN)

bool AudioInputSyncWriter::PrepareForeignSocketHandle(
    base::ProcessHandle process_handle,
    base::SyncSocket::Handle* foreign_handle) {
  ::DuplicateHandle(GetCurrentProcess(), foreign_socket_->handle(),
                    process_handle, foreign_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  return (*foreign_handle != 0);
}

#else

bool AudioInputSyncWriter::PrepareForeignSocketHandle(
    base::ProcessHandle process_handle,
    base::FileDescriptor* foreign_handle) {
  foreign_handle->fd = foreign_socket_->handle();
  foreign_handle->auto_close = false;
  return (foreign_handle->fd != -1);
}

#endif

}  // namespace content
