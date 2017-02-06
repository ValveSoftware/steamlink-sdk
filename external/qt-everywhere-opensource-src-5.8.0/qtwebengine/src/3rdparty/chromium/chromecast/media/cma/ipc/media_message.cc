// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc/media_message.h"

#include <limits>
#include <utility>

#include "base/logging.h"
#include "chromecast/media/cma/ipc/media_memory_chunk.h"

namespace chromecast {
namespace media {

// static
std::unique_ptr<MediaMessage> MediaMessage::CreateDummyMessage(uint32_t type) {
  return std::unique_ptr<MediaMessage>(
      new MediaMessage(type, std::numeric_limits<size_t>::max()));
}

// static
std::unique_ptr<MediaMessage> MediaMessage::CreateMessage(
    uint32_t type,
    const MemoryAllocatorCB& memory_allocator,
    size_t msg_content_capacity) {
  size_t msg_size = minimum_msg_size() + msg_content_capacity;

  // Make the message size a multiple of the alignment
  // so that if we have proper alignment for array of messages.
  size_t end_alignment = msg_size % ALIGNOF(SerializedMsg);
  if (end_alignment != 0)
    msg_size += ALIGNOF(SerializedMsg) - end_alignment;

  std::unique_ptr<MediaMemoryChunk> memory(memory_allocator.Run(msg_size));
  if (!memory)
    return std::unique_ptr<MediaMessage>();

  return std::unique_ptr<MediaMessage>(
      new MediaMessage(type, std::move(memory)));
}

// static
std::unique_ptr<MediaMessage> MediaMessage::CreateMessage(
    uint32_t type,
    std::unique_ptr<MediaMemoryChunk> memory) {
  return std::unique_ptr<MediaMessage>(
      new MediaMessage(type, std::move(memory)));
}

// static
std::unique_ptr<MediaMessage> MediaMessage::MapMessage(
    std::unique_ptr<MediaMemoryChunk> memory) {
  return std::unique_ptr<MediaMessage>(new MediaMessage(std::move(memory)));
}

MediaMessage::MediaMessage(uint32_t type, size_t msg_size)
    : is_dummy_msg_(true),
      cached_header_(&cached_msg_.header),
      msg_(&cached_msg_),
      msg_read_only_(&cached_msg_),
      rd_offset_(0) {
  cached_header_->size = msg_size;
  cached_header_->type = type;
  cached_header_->content_size = 0;
}

MediaMessage::MediaMessage(uint32_t type,
                           std::unique_ptr<MediaMemoryChunk> memory)
    : is_dummy_msg_(false),
      cached_header_(&cached_msg_.header),
      msg_(static_cast<SerializedMsg*>(memory->data())),
      msg_read_only_(msg_),
      mem_(std::move(memory)),
      rd_offset_(0) {
  CHECK(mem_->valid());
  CHECK_GE(mem_->size(), minimum_msg_size());

  // Check memory alignment:
  // needed to cast properly |msg_dst| to a SerializedMsg.
  CHECK_EQ(
      reinterpret_cast<uintptr_t>(mem_->data()) % ALIGNOF(SerializedMsg), 0u);

  // Make sure that |mem_->data()| + |mem_->size()| is also aligned correctly.
  // This is needed if we append a second serialized message next to this one.
  // The second serialized message must be aligned correctly.
  // It is similar to what a compiler is doing for arrays of structures.
  CHECK_EQ(mem_->size() % ALIGNOF(SerializedMsg), 0u);

  cached_header_->size = mem_->size();
  cached_header_->type = type;
  cached_header_->content_size = 0;
  msg_->header = *cached_header_;
}

MediaMessage::MediaMessage(std::unique_ptr<MediaMemoryChunk> memory)
    : is_dummy_msg_(false),
      cached_header_(&cached_msg_.header),
      msg_(NULL),
      msg_read_only_(static_cast<SerializedMsg*>(memory->data())),
      mem_(std::move(memory)),
      rd_offset_(0) {
  CHECK(mem_->valid());

  // Check memory alignment.
  CHECK_EQ(
      reinterpret_cast<uintptr_t>(mem_->data()) % ALIGNOF(SerializedMsg), 0u);

  // Cache the message header which cannot be modified while reading.
  CHECK_GE(mem_->size(), minimum_msg_size());
  *cached_header_ = msg_read_only_->header;
  CHECK_GE(cached_header_->size, minimum_msg_size());

  // Make sure if we have 2 consecutive serialized messages in memory,
  // the 2nd message is also aligned correctly.
  CHECK_EQ(cached_header_->size % ALIGNOF(SerializedMsg), 0u);

  size_t max_content_size = cached_header_->size - minimum_msg_size();
  CHECK_LE(cached_header_->content_size, max_content_size);
}

MediaMessage::~MediaMessage() {
}

bool MediaMessage::IsSerializedMsgAvailable() const {
  return !is_dummy_msg_ && mem_->valid();
}

bool MediaMessage::WriteBuffer(const void* src, size_t size) {
  // No message to write into.
  if (!msg_)
    return false;

  // The underlying memory was invalidated.
  if (!is_dummy_msg_ && !mem_->valid())
    return false;

  size_t max_content_size = cached_header_->size - minimum_msg_size();
  if (cached_header_->content_size + size > max_content_size) {
    cached_header_->content_size = max_content_size;
    msg_->header.content_size = cached_header_->content_size;
    return false;
  }

  // Write the message only for non-dummy messages.
  if (!is_dummy_msg_) {
    uint8_t* wr_ptr = &msg_->content + cached_header_->content_size;
    memcpy(wr_ptr, src, size);
  }

  cached_header_->content_size += size;
  msg_->header.content_size = cached_header_->content_size;
  return true;
}

bool MediaMessage::ReadBuffer(void* dst, size_t size) {
  // No read possible for a dummy message.
  if (is_dummy_msg_)
    return false;

  // The underlying memory was invalidated.
  if (!mem_->valid())
    return false;

  if (rd_offset_ + size > cached_header_->content_size) {
    rd_offset_ = cached_header_->content_size;
    return false;
  }

  const uint8_t* rd_ptr = &msg_read_only_->content + rd_offset_;
  memcpy(dst, rd_ptr, size);
  rd_offset_ += size;
  return true;
}

void* MediaMessage::GetWritableBuffer(size_t size) {
  // No read possible for a dummy message.
  if (is_dummy_msg_)
    return NULL;

  // The underlying memory was invalidated.
  if (!mem_->valid())
    return NULL;

  if (rd_offset_ + size > cached_header_->content_size) {
    rd_offset_ = cached_header_->content_size;
    return NULL;
  }

  uint8_t* rd_ptr = &msg_read_only_->content + rd_offset_;
  rd_offset_ += size;
  return rd_ptr;
}

const void* MediaMessage::GetBuffer(size_t size) {
  return GetWritableBuffer(size);
}

}  // namespace media
}  // namespace chromecast
