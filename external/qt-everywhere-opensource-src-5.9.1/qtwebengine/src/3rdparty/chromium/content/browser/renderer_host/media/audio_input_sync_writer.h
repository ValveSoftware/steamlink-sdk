// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/process/process.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/audio/audio_input_controller.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace content {

// A AudioInputController::SyncWriter implementation using SyncSocket. This
// is used by AudioInputController to provide a low latency data source for
// transmitting audio packets between the browser process and the renderer
// process.
class CONTENT_EXPORT AudioInputSyncWriter
    : public media::AudioInputController::SyncWriter {
 public:
  // Maximum fifo size (|overflow_buses_| and |overflow_params_|) in number of
  // AudioBuses.
  enum { kMaxOverflowBusesSize = 100 };

  AudioInputSyncWriter(void* shared_memory,
                       size_t shared_memory_size,
                       int shared_memory_segment_count,
                       const media::AudioParameters& params);

  ~AudioInputSyncWriter() override;

  // media::AudioInputController::SyncWriter implementation.
  void Write(const media::AudioBus* data,
             double volume,
             bool key_pressed,
             uint32_t hardware_delay_bytes) override;
  void Close() override;

  bool Init();
  bool PrepareForeignSocket(base::ProcessHandle process_handle,
                            base::SyncSocket::TransitDescriptor* descriptor);

 protected:
  // Socket for transmitting audio data.
  std::unique_ptr<base::CancelableSyncSocket> socket_;

 private:
  friend class AudioInputSyncWriterTest;
  FRIEND_TEST_ALL_PREFIXES(AudioInputSyncWriterTest, MultipleWritesAndReads);
  FRIEND_TEST_ALL_PREFIXES(AudioInputSyncWriterTest, MultipleWritesNoReads);
  FRIEND_TEST_ALL_PREFIXES(AudioInputSyncWriterTest, FillAndEmptyRingBuffer);
  FRIEND_TEST_ALL_PREFIXES(AudioInputSyncWriterTest, FillRingBufferAndFifo);
  FRIEND_TEST_ALL_PREFIXES(AudioInputSyncWriterTest,
                           MultipleFillAndEmptyRingBufferAndPartOfFifo);

  // Called by Write(). Checks the time since last called and if larger than a
  // threshold logs info about that.
  void CheckTimeSinceLastWrite();

  // Virtual function for native logging to be able to override in tests.
  virtual void AddToNativeLog(const std::string& message);

  // Push |data| and metadata to |audio_buffer_fifo_|. Returns true if
  // successful. Logs error and returns false if the fifo already reached the
  // maximum size.
  bool PushDataToFifo(const media::AudioBus* data,
                      double volume,
                      bool key_pressed,
                      uint32_t hardware_delay_bytes);

  // Writes as much data as possible from the fifo (|overflow_buses_|) to the
  // shared memory ring buffer. Returns true if all operations were successful,
  // otherwise false.
  bool WriteDataFromFifoToSharedMemory();

  // Write audio parameters to current segment in shared memory.
  void WriteParametersToCurrentSegment(double volume,
                                       bool key_pressed,
                                       uint32_t hardware_delay_bytes);

  // Signals over the socket that data has been written to the current segment.
  // Updates counters and returns true if successful. Logs error and returns
  // false if failure.
  bool SignalDataWrittenAndUpdateCounters();

  uint8_t* shared_memory_;
  uint32_t shared_memory_segment_size_;
  uint32_t shared_memory_segment_count_;
  uint32_t current_segment_id_;

  // Socket to be used by the renderer. The reference is released after
  // PrepareForeignSocketHandle() is called and ran successfully.
  std::unique_ptr<base::CancelableSyncSocket> foreign_socket_;

  // The time of the creation of this object.
  base::Time creation_time_;

  // The time of the last Write call.
  base::Time last_write_time_;

  // Size in bytes of each audio bus.
  const int audio_bus_memory_size_;

  // Increasing ID used for checking audio buffers are in correct sequence at
  // read side.
  uint32_t next_buffer_id_;

  // Next expected audio buffer index to have been read at the other side. We
  // will get the index read at the other side over the socket. Note that this
  // index does not correspond to |next_buffer_id_|, it's two separate counters.
  uint32_t next_read_buffer_index_;

  // Keeps track of number of filled buffer segments in the ring buffer to
  // ensure the we don't overwrite data that hasn't been read yet.
  int number_of_filled_segments_;

  // Counts the total number of calls to Write().
  size_t write_count_;

  // Counts the number of writes to the fifo instead of to the shared memory.
  size_t write_to_fifo_count_;

  // Counts the number of errors that causes data to be dropped, due to either
  // the fifo or the socket buffer being full.
  size_t write_error_count_;

  // Counts the fifo writes and errors we get during renderer process teardown
  // so that we can account for that (subtract) when we calculate the overall
  // counts.
  size_t trailing_write_to_fifo_count_;
  size_t trailing_write_error_count_;

  // Vector of audio buses allocated during construction and deleted in the
  // destructor.
  ScopedVector<media::AudioBus> audio_buses_;

  // Fifo for audio that is used in case there isn't room in the shared memory.
  // This can for example happen under load when the consumer side is starved.
  // It should ideally be rare, but we need to guarantee that the data arrives
  // since audio processing such as echo cancelling requires that to perform
  // properly.
  ScopedVector<media::AudioBus> overflow_buses_;
  struct OverflowParams {
    double volume;
    uint32_t hardware_delay_bytes;
    bool key_pressed;
  };
  std::deque<OverflowParams> overflow_params_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioInputSyncWriter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_INPUT_SYNC_WRITER_H_
