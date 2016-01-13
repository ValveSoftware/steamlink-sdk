// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/chunked_byte_buffer.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"

namespace {

static const size_t kHeaderLength = sizeof(uint32);

COMPILE_ASSERT(sizeof(size_t) >= kHeaderLength,
               ChunkedByteBufferNotSupportedOnThisArchitecture);

uint32 ReadBigEndian32(const uint8* buffer) {
  return (static_cast<uint32>(buffer[3])) |
         (static_cast<uint32>(buffer[2]) <<  8) |
         (static_cast<uint32>(buffer[1]) << 16) |
         (static_cast<uint32>(buffer[0]) << 24);
}

}  // namespace

namespace content {

ChunkedByteBuffer::ChunkedByteBuffer()
    : partial_chunk_(new Chunk()),
      total_bytes_stored_(0) {
}

ChunkedByteBuffer::~ChunkedByteBuffer() {
  Clear();
}

void ChunkedByteBuffer::Append(const uint8* start, size_t length) {
  size_t remaining_bytes = length;
  const uint8* next_data = start;

  while (remaining_bytes > 0) {
    DCHECK(partial_chunk_ != NULL);
    size_t insert_length = 0;
    bool header_completed = false;
    bool content_completed = false;
    std::vector<uint8>* insert_target;

    if (partial_chunk_->header.size() < kHeaderLength) {
      const size_t bytes_to_complete_header =
          kHeaderLength - partial_chunk_->header.size();
      insert_length = std::min(bytes_to_complete_header, remaining_bytes);
      insert_target = &partial_chunk_->header;
      header_completed = (remaining_bytes >= bytes_to_complete_header);
    } else {
      DCHECK_LT(partial_chunk_->content->size(),
                partial_chunk_->ExpectedContentLength());
      const size_t bytes_to_complete_chunk =
          partial_chunk_->ExpectedContentLength() -
          partial_chunk_->content->size();
      insert_length = std::min(bytes_to_complete_chunk, remaining_bytes);
      insert_target = partial_chunk_->content.get();
      content_completed = (remaining_bytes >= bytes_to_complete_chunk);
    }

    DCHECK_GT(insert_length, 0U);
    DCHECK_LE(insert_length, remaining_bytes);
    DCHECK_LE(next_data + insert_length, start + length);
    insert_target->insert(insert_target->end(),
                          next_data,
                          next_data + insert_length);
    next_data += insert_length;
    remaining_bytes -= insert_length;

    if (header_completed) {
      DCHECK_EQ(partial_chunk_->header.size(), kHeaderLength);
      if (partial_chunk_->ExpectedContentLength() == 0) {
        // Handle zero-byte chunks.
        chunks_.push_back(partial_chunk_.release());
        partial_chunk_.reset(new Chunk());
      } else {
        partial_chunk_->content->reserve(
            partial_chunk_->ExpectedContentLength());
      }
    } else if (content_completed) {
      DCHECK_EQ(partial_chunk_->content->size(),
                partial_chunk_->ExpectedContentLength());
      chunks_.push_back(partial_chunk_.release());
      partial_chunk_.reset(new Chunk());
    }
  }
  DCHECK_EQ(next_data, start + length);
  total_bytes_stored_ += length;
}

void ChunkedByteBuffer::Append(const std::string& string) {
  Append(reinterpret_cast<const uint8*>(string.data()), string.size());
}

bool ChunkedByteBuffer::HasChunks() const {
  return !chunks_.empty();
}

scoped_ptr< std::vector<uint8> > ChunkedByteBuffer::PopChunk() {
  if (chunks_.empty())
    return scoped_ptr< std::vector<uint8> >();
  scoped_ptr<Chunk> chunk(*chunks_.begin());
  chunks_.weak_erase(chunks_.begin());
  DCHECK_EQ(chunk->header.size(), kHeaderLength);
  DCHECK_EQ(chunk->content->size(), chunk->ExpectedContentLength());
  total_bytes_stored_ -= chunk->content->size();
  total_bytes_stored_ -= kHeaderLength;
  return chunk->content.Pass();
}

void ChunkedByteBuffer::Clear() {
  chunks_.clear();
  partial_chunk_.reset(new Chunk());
  total_bytes_stored_ = 0;
}

ChunkedByteBuffer::Chunk::Chunk()
    : content(new std::vector<uint8>()) {
}

ChunkedByteBuffer::Chunk::~Chunk() {
}

size_t ChunkedByteBuffer::Chunk::ExpectedContentLength() const {
  DCHECK_EQ(header.size(), kHeaderLength);
  return static_cast<size_t>(ReadBigEndian32(&header[0]));
}

}  // namespace content
