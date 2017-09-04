// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/trace_buffer_writer.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/tracing/core/trace_ring_buffer.h"
#include "components/tracing/proto/event.pbzero.h"
#include "components/tracing/test/golden_protos/events_chunk.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace v2 {
namespace {

class MockEvent : public pbzero::tracing::proto::Event {
 public:
  static TraceEventHandle Add(TraceBufferWriter* writer, size_t event_size) {
    TraceEventHandle handle = writer->AddEvent();
    MockEvent* mock_event = static_cast<MockEvent*>(&*handle);

    size_t buffer_size = 0;
    DCHECK_GT(event_size, 2u);
    if (event_size < (1 << 7) + 2)
      buffer_size = event_size - 2;
    else if (event_size < (1 << 14) + 3)
      buffer_size = event_size - 3;
    else if (event_size < (1 << 21) + 4)
      buffer_size = event_size - 4;
    else
      NOTREACHED();

    DCHECK(buffer_size);
    std::unique_ptr<uint8_t[]> buf(new uint8_t[buffer_size]);
    memset(buf.get(), 0, buffer_size);
    mock_event->AppendBytes(1, buf.get(), buffer_size);

    return handle;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEvent);
};

constexpr uint32_t kNumChunks = 10;
constexpr size_t kBufferSize = kNumChunks * kChunkSize;

class TraceBufferWriterTest : public ::testing::Test {
 public:
  void SetUp() override {
    ring_buf_mem_.reset(new uint8_t[kBufferSize]);
    memset(ring_buf_mem_.get(), 0, kBufferSize);
    ring_buf_.reset(new TraceRingBuffer(ring_buf_mem_.get(), kBufferSize));

    // Estimate the size required to fill one event.
    std::unique_ptr<TraceBufferWriter> writer = CreateWriter(1);
    MockEvent::Add(&*writer, 4);
    event_size_to_fill_chunk_ = writer->stream_writer().bytes_available() + 4;
    writer.reset();
    ring_buf_.reset(new TraceRingBuffer(ring_buf_mem_.get(), kBufferSize));
  }

  void TearDown() override {
    ring_buf_.reset();
    ring_buf_mem_.reset();
  }

  std::unique_ptr<TraceBufferWriter> CreateWriter(uint32_t writer_id) {
    return base::WrapUnique(new TraceBufferWriter(ring_buf_.get(), writer_id));
  }

  const TraceRingBuffer::Chunk* GetChunk(uint32_t i) {
    DCHECK_LT(i, kNumChunks);
    return &ring_buf_->chunks_for_testing()[i];
  }

  ::tracing::proto::EventsChunk ReadBackAndTestChunk(
      uint32_t chunk_id,
      uint32_t expected_writer_id,
      uint32_t expected_seq_id,
      int expected_num_events,
      bool expected_first_event_continues,
      bool expected_last_event_continues) {
    const TraceRingBuffer::Chunk* chunk = GetChunk(chunk_id);
    ::tracing::proto::EventsChunk parsed_chunk;
    EXPECT_TRUE(
        parsed_chunk.ParseFromArray(chunk->payload(), chunk->used_size()));

    EXPECT_TRUE(parsed_chunk.has_writer_id());
    EXPECT_EQ(expected_writer_id, parsed_chunk.writer_id());

    EXPECT_TRUE(parsed_chunk.has_seq_id());
    EXPECT_EQ(expected_seq_id, parsed_chunk.seq_id());

    EXPECT_EQ(expected_first_event_continues,
              parsed_chunk.first_event_continues_from_prev_chunk());
    EXPECT_EQ(expected_last_event_continues,
              parsed_chunk.last_event_continues_on_next_chunk());
    EXPECT_EQ(expected_num_events, parsed_chunk.events_size());
    return parsed_chunk;
  }

  const TraceRingBuffer& ring_buffer() const { return *ring_buf_; }
  size_t event_size_to_fill_chunk() const { return event_size_to_fill_chunk_; }

 private:
  std::unique_ptr<uint8_t[]> ring_buf_mem_;
  std::unique_ptr<TraceRingBuffer> ring_buf_;
  size_t event_size_to_fill_chunk_;
};

TEST_F(TraceBufferWriterTest, SingleEvent) {
  const uint32_t kWriterId = 0x42;
  std::unique_ptr<TraceBufferWriter> writer = CreateWriter(kWriterId);
  MockEvent::Add(writer.get(), 7);
  writer->Flush();

  auto parsed_chunk = ReadBackAndTestChunk(0, kWriterId, 0, 1, false, false);
  EXPECT_EQ(7u, parsed_chunk.events(0).size());
}

TEST_F(TraceBufferWriterTest, ManySmallEvents) {
  const uint32_t kWriterId = 0x42;
  std::unique_ptr<TraceBufferWriter> writer = CreateWriter(kWriterId);

  uint32_t last_owned_chunk_id = 1;
  uint32_t num_times_did_switch_to_chunk[kNumChunks] = {};

  // kBufferSize here is just an upper bound to prevent the test to get stuck
  // undefinitely in the loop if it fails.
  for (size_t i = 0; i < kBufferSize; i++) {
    MockEvent::Add(&*writer, 5);

    // Small events shouldn't never cause more than a chunk to be owned. Check
    // that TraceBufferWriter doesn't accidentally retain chunks.
    uint32_t num_chunks_owned = 0;
    for (uint32_t chunk_id = 0; chunk_id < kNumChunks; chunk_id++) {
      const bool is_owned = GetChunk(chunk_id)->is_owned();
      num_chunks_owned += is_owned ? 1 : 0;
      if (is_owned && chunk_id != last_owned_chunk_id) {
        last_owned_chunk_id = chunk_id;
        num_times_did_switch_to_chunk[chunk_id]++;
      }
    }
    ASSERT_EQ(1u, num_chunks_owned);

    // Stop once the last chunk has been filled twice.
    if (num_times_did_switch_to_chunk[kNumChunks - 1] == 2)
      break;
  }

  // Test the wrap-over logic: all chunks should have been filled twice.
  for (uint32_t chunk_id = 0; chunk_id < kNumChunks; chunk_id++)
    EXPECT_EQ(2u, num_times_did_switch_to_chunk[chunk_id]);

  // Test that Flush() releases all chunks.
  writer->Flush();
  for (uint32_t chunk_id = 0; chunk_id < kNumChunks; chunk_id++)
    EXPECT_FALSE(GetChunk(chunk_id)->is_owned());
}

TEST_F(TraceBufferWriterTest, OneWriterWithFragmentingEvents) {
  const uint32_t kWriterId = 0x42;
  std::unique_ptr<TraceBufferWriter> writer = CreateWriter(kWriterId);

  MockEvent::Add(&*writer, event_size_to_fill_chunk());

  EXPECT_TRUE(GetChunk(0)->is_owned());
  EXPECT_FALSE(GetChunk(1)->is_owned());

  MockEvent::Add(&*writer, event_size_to_fill_chunk());
  EXPECT_TRUE(GetChunk(1)->is_owned());
  EXPECT_FALSE(GetChunk(2)->is_owned());

  // Create one event which starts at the beginning of chunk 2 and overflows
  // into chunk 3.
  MockEvent::Add(&*writer, event_size_to_fill_chunk() + 1);
  EXPECT_TRUE(GetChunk(2)->is_owned());
  EXPECT_TRUE(GetChunk(3)->is_owned());

  // Adding a new event should cause the chunk 2 to be released, while chunk
  // 3 is still retained.
  MockEvent::Add(&*writer, 4);
  EXPECT_FALSE(GetChunk(2)->is_owned());
  EXPECT_TRUE(GetChunk(3)->is_owned());

  // Now add a very large event which spans across 3 chunks (chunks 3, 4 and 5).
  MockEvent::Add(&*writer, event_size_to_fill_chunk() * 2 + 1);
  EXPECT_TRUE(GetChunk(3)->is_owned());
  EXPECT_TRUE(GetChunk(4)->is_owned());
  EXPECT_TRUE(GetChunk(5)->is_owned());

  // Add a final small event and check that chunks 3 and 4 are released.
  MockEvent::Add(&*writer, 4);
  EXPECT_FALSE(GetChunk(3)->is_owned());
  EXPECT_FALSE(GetChunk(4)->is_owned());
  EXPECT_TRUE(GetChunk(5)->is_owned());

  // Flush and readback the chunks using the official protos.
  writer->Flush();

  // The first two chunks should have one event each, neither of them wrapping.
  auto chunk = ReadBackAndTestChunk(0, kWriterId, 0, 1, false, false);
  EXPECT_EQ(event_size_to_fill_chunk(), chunk.events(0).size());

  chunk = ReadBackAndTestChunk(1, kWriterId, 1, 1, false, false);
  EXPECT_EQ(event_size_to_fill_chunk(), chunk.events(0).size());

  // Chunk 2 should have one partial event, which overflows into 3.
  chunk = ReadBackAndTestChunk(2, kWriterId, 2, 1, false, true);
  EXPECT_EQ(event_size_to_fill_chunk(), chunk.events(0).size());

  // Chunk 3 should have the overflowing event from above, a small event, and
  // the beginning of the very large event.
  chunk = ReadBackAndTestChunk(3, kWriterId, 3, 3, true, true);
  EXPECT_EQ(4u, chunk.events(1).size());

  // Chunk 4 should contain the partial continuation of the large event.
  chunk = ReadBackAndTestChunk(4, kWriterId, 4, 1, true, true);
  EXPECT_EQ(event_size_to_fill_chunk() - 2, chunk.events(0).size());

  // Chunk 5 should contain the end of the large event and the final small one.
  chunk = ReadBackAndTestChunk(5, kWriterId, 5, 2, true, false);
  EXPECT_EQ(4u, chunk.events(1).size());
}

TEST_F(TraceBufferWriterTest, ManyWriters) {
  const uint32_t kNumWriters = kNumChunks / 2;
  std::unique_ptr<TraceBufferWriter> writer[kNumWriters];

  for (uint32_t i = 0; i < kNumWriters; ++i) {
    writer[i] = CreateWriter(i + 1);
    MockEvent::Add(writer[i].get(), 4);
    EXPECT_EQ(writer[i]->writer_id(), GetChunk(i)->owner());
  }

  // Write one large and one small event on each writer.
  for (uint32_t i = 0; i < kNumWriters; ++i) {
    MockEvent::Add(writer[i].get(), event_size_to_fill_chunk());
    MockEvent::Add(writer[i].get(), 5 + i);
  }

  // At this point the first 5 chunks should be returned and the last 5 owned
  // by the respective 5 writers.
  for (uint32_t i = 0; i < kNumWriters; ++i)
    EXPECT_FALSE(GetChunk(i)->is_owned());
  for (uint32_t i = kNumWriters; i < kNumWriters * 2; ++i)
    EXPECT_EQ(writer[i - kNumWriters]->writer_id(), GetChunk(i)->owner());

  // Write one large event to writer 0 (currently owning chunk 5). That will
  // make it return the chunk 5 and take ownership of chunks [0, 4].
  MockEvent::Add(writer[0].get(), event_size_to_fill_chunk());
  auto retain_event =
      MockEvent::Add(writer[0].get(), event_size_to_fill_chunk() * 4 + 1);
  for (uint32_t i = 0; i < 5; ++i)
    EXPECT_EQ(writer[0]->writer_id(), GetChunk(i)->owner());

  // At this point the only free chunk is chunk 5. Attempting a write from
  // another writer should fill that.
  EXPECT_FALSE(GetChunk(5)->is_owned());
  auto retain_event_2 =
      MockEvent::Add(writer[3].get(), event_size_to_fill_chunk());

  // Now all the chunks are taken.
  for (uint32_t i = 0; i < kNumChunks; ++i)
    EXPECT_TRUE(GetChunk(i)->is_owned());

  // An attempt to write a larger event on another write should cause the ring
  // buffer to fall in bankrupcy mode.
  auto retain_event_3 =
      MockEvent::Add(writer[4].get(), event_size_to_fill_chunk());
  EXPECT_EQ(ring_buffer().num_chunks(), ring_buffer().GetNumChunksTaken());

  // A small writer to the writer 0 should cause it to return all chunks but the
  // last one and leave the bankrupcy chunk.
  retain_event = MockEvent::Add(writer[0].get(), 7);
  EXPECT_LT(ring_buffer().GetNumChunksTaken(), ring_buffer().num_chunks());
  EXPECT_EQ(writer[0]->writer_id(), GetChunk(4)->owner());
  for (uint32_t i = 0; i < 3; ++i)
    EXPECT_FALSE(GetChunk(i)->is_owned());

  // Flush all the writers and test that all chunks are returned.
  for (uint32_t i = 0; i < kNumWriters; ++i)
    writer[i]->Flush();
  for (uint32_t i = 0; i < kNumChunks; ++i)
    EXPECT_FALSE(GetChunk(i)->is_owned());

  // Readback and test the content of the chunks.

  auto chunk =
      ReadBackAndTestChunk(4, writer[0]->writer_id(), 6, 2, true, false);
  EXPECT_EQ(7u, chunk.events(1).size());

  // writer[1] and writer[2] just have a continuation and a small event each.
  chunk = ReadBackAndTestChunk(6, writer[1]->writer_id(), 1, 2, true, false);
  EXPECT_EQ(5u + 1, chunk.events(1).size());

  chunk = ReadBackAndTestChunk(7, writer[2]->writer_id(), 1, 2, true, false);
  EXPECT_EQ(5u + 2, chunk.events(1).size());

  // writer[3] did the last write before the bankrupcy and has one extra event.
  ReadBackAndTestChunk(8, writer[3]->writer_id(), 1, 3, true, true);

  // writer[4] overflew in the bankrupcy chunk and has 3 events as well.
  ReadBackAndTestChunk(9, writer[4]->writer_id(), 1, 3, true, true);
}

}  // namespace
}  // namespace v2
}  // namespace tracing
