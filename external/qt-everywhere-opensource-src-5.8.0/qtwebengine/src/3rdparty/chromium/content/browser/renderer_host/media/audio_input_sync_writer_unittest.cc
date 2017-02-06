// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sync_socket.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using base::TimeDelta;
using media::AudioBus;
using media::AudioParameters;

namespace content {

namespace {

// Number of audio buffers in the faked ring buffer.
const int kSegments = 10;

// Audio buffer parameters.
const int channels = 1;
const int sampling_frequency_hz = 16000;
const int frames = sampling_frequency_hz / 100;  // 10 ms
const int bits_per_sample = 16;

// Faked ring buffer. Must be aligned.
#define DATA_ALIGNMENT 16
static_assert(AudioBus::kChannelAlignment == DATA_ALIGNMENT,
              "Data alignment not same as AudioBus");
ALIGNAS(DATA_ALIGNMENT)
uint8_t data[kSegments * (sizeof(media::AudioInputBufferParameters) +
                          frames * channels * sizeof(float))];

}  // namespace

// Mocked out sockets used for Send/ReceiveWithTimeout. Counts the number of
// outstanding reads, i.e. the diff between send and receive calls.
class MockCancelableSyncSocket : public base::CancelableSyncSocket {
 public:
  MockCancelableSyncSocket(int buffer_size)
      : in_failure_mode_(false),
        writes_(0),
        reads_(0),
        receives_(0),
        buffer_size_(buffer_size),
        read_buffer_index_(0) {}

  size_t Send(const void* buffer, size_t length) override {
    EXPECT_EQ(length, sizeof(uint32_t));

    ++writes_;
    EXPECT_LE(NumberOfBuffersFilled(), buffer_size_);
    return length;
  }

  size_t Receive(void* buffer, size_t length) override {
    EXPECT_EQ(0u, length % sizeof(uint32_t));

    if (in_failure_mode_)
      return 0;
    if (receives_ == reads_)
      return 0;

    uint32_t* ptr = static_cast<uint32_t*>(buffer);
    size_t received = 0;
    for (; received < length / sizeof(uint32_t) && receives_ < reads_;
         ++received, ++ptr) {
      ++receives_;
      EXPECT_LE(receives_, reads_);
      *ptr = ++read_buffer_index_;
    }
    return received * sizeof(uint32_t);
  }

  size_t Peek() override {
    return (reads_ - receives_) * sizeof(uint32_t);
  }

  // Simluates reading |buffers| number of buffers from the ring buffer.
  void Read(int buffers) {
    reads_ += buffers;
    EXPECT_LE(reads_, writes_);
  }

  // When |in_failure_mode_| == true, the socket fails to receive.
  void SetFailureMode(bool in_failure_mode) {
    in_failure_mode_ = in_failure_mode;
  }

  int NumberOfBuffersFilled() { return writes_ - reads_; }

 private:
  bool in_failure_mode_;
  int writes_;
  int reads_;
  int receives_;
  int buffer_size_;
  uint32_t read_buffer_index_;

  DISALLOW_COPY_AND_ASSIGN(MockCancelableSyncSocket);
};

class AudioInputSyncWriterUnderTest : public AudioInputSyncWriter {
 public:
  AudioInputSyncWriterUnderTest(void* shared_memory,
                                size_t shared_memory_size,
                                int shared_memory_segment_count,
                                const media::AudioParameters& params,
                                base::CancelableSyncSocket* socket)
      : AudioInputSyncWriter(shared_memory, shared_memory_size,
                             shared_memory_segment_count, params) {
    socket_.reset(socket);
  }

  ~AudioInputSyncWriterUnderTest() override {}

  MOCK_METHOD1(AddToNativeLog, void(const std::string& message));
};

class AudioInputSyncWriterTest : public testing::Test {
 public:
  AudioInputSyncWriterTest()
      : socket_(nullptr) {
    const media::ChannelLayout layout =
        media::GuessChannelLayout(channels);
    EXPECT_NE(media::ChannelLayout::CHANNEL_LAYOUT_UNSUPPORTED, layout);
    AudioParameters audio_params(
          AudioParameters::AUDIO_FAKE, layout, sampling_frequency_hz,
          bits_per_sample, frames);

    const uint32_t segment_size = sizeof(media::AudioInputBufferParameters) +
                                  AudioBus::CalculateMemorySize(audio_params);
    size_t data_size = kSegments * segment_size;
    EXPECT_LE(data_size, sizeof(data));

    socket_ = new MockCancelableSyncSocket(kSegments);
    writer_.reset(new AudioInputSyncWriterUnderTest(
        &data[0], data_size, kSegments, audio_params, socket_));
    audio_bus_ = AudioBus::Create(audio_params);
  }

  ~AudioInputSyncWriterTest() override {
  }

  // Get total number of expected log calls. On non-Android we expect one log
  // call at first Write() call, zero on Android. Besides that only for errors
  // and fifo info.
  int GetTotalNumberOfExpectedLogCalls(int expected_calls_due_to_error) {
#if defined(OS_ANDROID)
    return expected_calls_due_to_error;
#else
    return expected_calls_due_to_error + 1;
#endif
  }

  // Tests expected numbers which are given as arguments.
  bool TestSocketAndFifoExpectations(int number_of_buffers_in_socket,
                                     size_t number_of_verifications_in_socket,
                                     size_t number_of_buffers_in_fifo) {
    EXPECT_EQ(number_of_buffers_in_socket, socket_->NumberOfBuffersFilled());
    EXPECT_EQ(number_of_verifications_in_socket, socket_->Peek());
    EXPECT_EQ(number_of_buffers_in_fifo, writer_->overflow_buses_.size());

    return number_of_buffers_in_socket == socket_->NumberOfBuffersFilled() &&
           number_of_verifications_in_socket == socket_->Peek() &&
           number_of_buffers_in_fifo == writer_->overflow_buses_.size();
  }

 protected:
  std::unique_ptr<AudioInputSyncWriterUnderTest> writer_;
  MockCancelableSyncSocket* socket_;
  std::unique_ptr<AudioBus> audio_bus_;

 private:
  TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(AudioInputSyncWriterTest);
};

TEST_F(AudioInputSyncWriterTest, SingleWriteAndRead) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(0));

  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(1, 0, 0));

  socket_->Read(1);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 1 * sizeof(uint32_t), 0));
}

TEST_F(AudioInputSyncWriterTest, MultipleWritesAndReads) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(0));

  for (int i = 1; i <= 2 * kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_TRUE(TestSocketAndFifoExpectations(1, 0, 0));
    socket_->Read(1);
    EXPECT_TRUE(TestSocketAndFifoExpectations(0, 1 * sizeof(uint32_t), 0));
  }
}

TEST_F(AudioInputSyncWriterTest, MultipleWritesNoReads) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(1));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_TRUE(TestSocketAndFifoExpectations(i, 0, 0));
  }

  // Now the ring buffer is full, do more writes. We should start filling the
  // fifo and should get one extra log call for that.
  for (size_t i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, i));
  }
}

TEST_F(AudioInputSyncWriterTest, FillAndEmptyRingBuffer) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(2));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Empty half of the ring buffer.
  const int buffers_to_read = kSegments / 2;
  socket_->Read(buffers_to_read);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments - buffers_to_read,
                                            buffers_to_read * sizeof(uint32_t),
                                            0));

  // Fill up again. The first write should do receive until that queue is
  // empty.
  for (int i = kSegments - buffers_to_read + 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
    EXPECT_TRUE(TestSocketAndFifoExpectations(i, 0, 0));
  }

  // Another write, should put the data in the fifo, and render an extra log
  // call.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 1));

  // Empty the ring buffer.
  socket_->Read(kSegments);
  EXPECT_TRUE(
      TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t), 1));

  // Another write, should do receive until that queue is empty and write both
  // the data in the fifo and the new data, and render a log call.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(2, 0, 0));

  // Read the two data blocks.
  socket_->Read(2);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 2 * sizeof(uint32_t), 0));
}

TEST_F(AudioInputSyncWriterTest, FillRingBufferAndFifo) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(2));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Fill the fifo. Should render one log call for starting filling it.
  const size_t max_fifo_size = AudioInputSyncWriter::kMaxOverflowBusesSize;
  for (size_t i = 1; i <= max_fifo_size; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, max_fifo_size));

  // Another write, data should be dropped and render one log call.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, max_fifo_size));
}

TEST_F(AudioInputSyncWriterTest, MultipleFillAndEmptyRingBufferAndPartOfFifo) {
  EXPECT_CALL(*writer_.get(), AddToNativeLog(_))
      .Times(GetTotalNumberOfExpectedLogCalls(4));

  // Fill the ring buffer.
  for (int i = 1; i <= kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 0));

  // Write more data, should be put in the fifo and render one log call for
  // starting filling it.
  for (size_t i = 1; i <= 2 * kSegments; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2 * kSegments));

  // Empty the ring buffer.
  socket_->Read(kSegments);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t),
                                            2 * kSegments));

  // Another write should fill up the ring buffer with data from the fifo and
  // put this data into the fifo.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, kSegments + 1));

  // Empty the ring buffer again.
  socket_->Read(kSegments);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t),
                                            kSegments + 1));

  // Another write should fill up the ring buffer with data from the fifo and
  // put this data into the fifo.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2));

  // Empty the ring buffer again.
  socket_->Read(kSegments);
  EXPECT_TRUE(
      TestSocketAndFifoExpectations(0, kSegments * sizeof(uint32_t), 2));

  // Another write should put the remaining data in the fifo in the ring buffer
  // together with this data. Should render a log call for emptying the fifo.
  writer_->Write(audio_bus_.get(), 0, false, 0);
  EXPECT_TRUE(TestSocketAndFifoExpectations(3, 0, 0));

  // Read the remaining data.
  socket_->Read(3);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 3 * sizeof(uint32_t), 0));

  // Fill the ring buffer and part of the fifo. Should render one log call for
  // starting filling it.
  for (int i = 1; i <= kSegments + 2; ++i) {
    writer_->Write(audio_bus_.get(), 0, false, 0);
  }
  EXPECT_TRUE(TestSocketAndFifoExpectations(kSegments, 0, 2));

  // Empty both. Should render a log call for emptying the fifo.
  socket_->Read(kSegments);
  writer_->Write(audio_bus_.get(), 0, false, 0);
  socket_->Read(3);
  EXPECT_TRUE(TestSocketAndFifoExpectations(0, 3 * sizeof(uint32_t), 0));
}

}  // namespace content
