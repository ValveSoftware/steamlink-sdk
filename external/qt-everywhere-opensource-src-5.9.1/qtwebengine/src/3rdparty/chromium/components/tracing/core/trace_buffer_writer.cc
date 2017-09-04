// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/core/trace_buffer_writer.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "components/tracing/core/proto_utils.h"
#include "components/tracing/proto/event.pbzero.h"
#include "components/tracing/proto/events_chunk.pbzero.h"

namespace tracing {
namespace v2 {

namespace {

using ChunkProto = pbzero::tracing::proto::EventsChunk;

const size_t kEventPreambleSize = 1 + proto::kMessageLengthFieldSize;

// TODO(primiano): replace 16 with a more reasonable size, that is, the size
// of a simple trace event with no args.
const size_t kMinEventSize = 16;

}  // namespace

TraceBufferWriter::TraceBufferWriter(TraceRingBuffer* trace_ring_buffer,
                                     uint32_t writer_id)
    : trace_ring_buffer_(trace_ring_buffer),
      writer_id_(writer_id),
      chunk_seq_id_(0),
      chunk_(nullptr),
      event_data_start_in_current_chunk_(nullptr),
      stream_writer_(this) {
  event_.Reset(&stream_writer_);
}

TraceBufferWriter::~TraceBufferWriter() {}

void TraceBufferWriter::FinalizeCurrentEvent() {
  if (UNLIKELY(!chunk_))
    return;

  // Finalize the last event added. This ensures that it and all its nested
  // fields are committed to the ring buffer and sealed. No further changes to
  // the chunks's memory can be made from the |event_| after this point.
  event_.Finalize();

  // In the unlikely event that the last event did wrap over one or more chunks,
  // is is now time to return those chunks (all but the active one) back.
  TraceRingBuffer::Chunk* retained_chunk = chunk_->next_in_owner_list();
  if (UNLIKELY(retained_chunk)) {
    while (retained_chunk) {
      TraceRingBuffer::Chunk* next = retained_chunk->next_in_owner_list();
      retained_chunk->set_next_in_owner_list(nullptr);
      trace_ring_buffer_->ReturnChunk(retained_chunk);
      retained_chunk = next;
    }
    chunk_->set_next_in_owner_list(nullptr);
  }
}

TraceEventHandle TraceBufferWriter::AddEvent() {
  FinalizeCurrentEvent();

  // In order to start a new event at least kMessageLengthFieldSize + 1 bytes
  // are required in the chunk to write the preamble and size of the event
  // itself. We take a bit more room here, it doesn't make a lot of sense
  // starting a partial event that will fragment immediately after.
  static_assert(kMinEventSize >= proto::kMessageLengthFieldSize + 1,
                "kMinEventSize too small");
  if (stream_writer_.bytes_available() < kMinEventSize)
    stream_writer_.Reset(AcquireNewChunk(false /* is_fragmenting_event */));

  event_.Reset(&stream_writer_);
  WriteEventPreambleForNewChunk(
      stream_writer_.ReserveBytesUnsafe(kEventPreambleSize));
  DCHECK_EQ(stream_writer_.write_ptr(), event_data_start_in_current_chunk_);
  return TraceEventHandle(static_cast<pbzero::tracing::proto::Event*>(&event_));
}

// This is invoked by the ProtoZeroMessage write methods when reaching the
// end of the current chunk during a write.
ContiguousMemoryRange TraceBufferWriter::GetNewBuffer() {
  return AcquireNewChunk(true /* is_fragmenting_event */);
}

void TraceBufferWriter::FinalizeCurrentChunk(bool is_fragmenting_event) {
  DCHECK(!is_fragmenting_event || chunk_);
  if (!chunk_)
    return;
  uint8_t* write_ptr = stream_writer_.write_ptr();
  DCHECK_GE(write_ptr, chunk_->payload());
  DCHECK_LE(write_ptr, chunk_->end() - 2);

  if (is_fragmenting_event) {
    proto::StaticAssertSingleBytePreamble<
        ChunkProto::kLastEventContinuesOnNextChunkFieldNumber>();
    *write_ptr++ = static_cast<uint8_t>(proto::MakeTagVarInt(
        ChunkProto::kLastEventContinuesOnNextChunkFieldNumber));
    *write_ptr++ = 1;  // = true.
  }

  DCHECK_LT(static_cast<uintptr_t>(write_ptr - chunk_->payload()), kChunkSize);
  chunk_->set_used_size(static_cast<uint32_t>(write_ptr - chunk_->payload()));
}

// There are paths that lead to AcquireNewChunk():
// When |is_fragmenting_event| = false:
//   AddEvent() is called and there isn't enough room in the current chunk to
//   start a new event (or we don't have a chunk yet).
// When |is_fragmenting_event| = true:
//   The client is writing an event, a ProtoZeroMessage::Append* method hits
//   the boundary of the chunk and requests a new one via GetNewBuffer().
ContiguousMemoryRange TraceBufferWriter::AcquireNewChunk(
    bool is_fragmenting_event) {
  FinalizeCurrentChunk(is_fragmenting_event);
  TraceRingBuffer::Chunk* new_chunk = trace_ring_buffer_->TakeChunk(writer_id_);
  if (is_fragmenting_event) {
    // Backfill the size field of the event with the partial size accumulated
    // so far in the old chunk. WriteEventPreambleForNewChunk() will take care
    // of resetting the |size_field| of the event to the new chunk.
    DCHECK_GE(event_data_start_in_current_chunk_, chunk_->payload());
    DCHECK_LE(event_data_start_in_current_chunk_,
              chunk_->end() - proto::kMessageLengthFieldSize);
    const uint32_t event_partial_size = static_cast<uint32_t>(
        stream_writer_.write_ptr() - event_data_start_in_current_chunk_);
    proto::WriteRedundantVarInt(event_partial_size, event_.size_field().begin);
    event_.inc_size_already_written(event_partial_size);

    // If this is a continuation of an event, this writer needs to retain the
    // old chunk. The client might still be able to write to it. This is to deal
    // with the case of a nested message which is started in one chunk and
    // ends in another one. The finalization needs to write-back the size field
    // in the old chunk.
    new_chunk->set_next_in_owner_list(chunk_);
  } else if (chunk_) {
    // Otherwise, if this is a new event, the previous chunk can be returned.
    trace_ring_buffer_->ReturnChunk(chunk_);
  }
  chunk_ = new_chunk;

  // Write the protobuf for the chunk header. The generated C++ stub for
  // events_chunk.proto cannot be used here because that would re-enter this
  // class and make this code extremely hard to reason about.
  uint8_t* chunk_proto = new_chunk->payload();

  proto::StaticAssertSingleBytePreamble<ChunkProto::kWriterIdFieldNumber>();
  *chunk_proto++ = static_cast<uint8_t>(
      proto::MakeTagVarInt(ChunkProto::kWriterIdFieldNumber));
  chunk_proto = proto::WriteVarInt(writer_id_, chunk_proto);

  proto::StaticAssertSingleBytePreamble<ChunkProto::kSeqIdFieldNumber>();
  *chunk_proto++ =
      static_cast<uint8_t>(proto::MakeTagVarInt(ChunkProto::kSeqIdFieldNumber));
  chunk_proto = proto::WriteVarInt(chunk_seq_id_, chunk_proto);

  if (is_fragmenting_event) {
    proto::StaticAssertSingleBytePreamble<
        ChunkProto::kFirstEventContinuesFromPrevChunkFieldNumber>();
    *chunk_proto++ = static_cast<uint8_t>(proto::MakeTagVarInt(
        ChunkProto::kFirstEventContinuesFromPrevChunkFieldNumber));
    *chunk_proto++ = 1;  // = true.
  }

  ++chunk_seq_id_;

  // If the new chunk was requested while writing an event (the event spans
  // across chunks) write a new preamble for the partial event in the new chunk.
  if (is_fragmenting_event)
    chunk_proto = WriteEventPreambleForNewChunk(chunk_proto);

  // We reserve 2 bytes from the end, so that FinalizeCurrentChunk() can use
  // them to write the |last_event_continues_on_next_chunk| field.
  return {chunk_proto, new_chunk->end() - 2};
}

// Writes the one-byte preamble for the start of either a new or a partial
// event and reserves kMessageLengthFieldSize bytes for its length. Also
// keeps size-field the bookkeeping up to date. Returns the pointer in the chunk
// past the event preamble, where the event proto should be written.
uint8_t* TraceBufferWriter::WriteEventPreambleForNewChunk(uint8_t* begin) {
  // The caller must have ensured to have enough room in the chunk. The event
  // preamble itself cannot be fragmented.
  uint8_t* const end = begin + kEventPreambleSize;
  proto::StaticAssertSingleBytePreamble<ChunkProto::kEventsFieldNumber>();
  *begin++ = static_cast<uint8_t>(
      proto::MakeTagLengthDelimited(ChunkProto::kEventsFieldNumber));
  ContiguousMemoryRange range = {begin, end};
  event_.set_size_field(range);
  event_data_start_in_current_chunk_ = end;
  return end;
}

void TraceBufferWriter::Flush() {
  if (!chunk_)
    return;
  FinalizeCurrentEvent();
  FinalizeCurrentChunk(false /* is_fragmenting_event */);
  trace_ring_buffer_->ReturnChunk(chunk_);
  chunk_ = nullptr;
}

}  // namespace v2
}  // namespace tracing
