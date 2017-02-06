// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_H_
#define CHROMECAST_MEDIA_CMA_IPC_MEDIA_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"

namespace chromecast {
namespace media {
class MediaMemoryChunk;

// MediaMessage -
// Represents a media message, including:
// - a message header that gives for example the message size or its type,
// - the content of the message,
// - and some possible padding if the content does not occupy the whole
//   reserved space.
//
class MediaMessage {
 public:
  // Memory allocator: given a number of bytes to allocate,
  // return the pointer to the allocated block if successful
  // or NULL if allocation failed.
  typedef base::Callback<std::unique_ptr<MediaMemoryChunk>(size_t)>
      MemoryAllocatorCB;

  // Creates a message with no associated memory for its content, i.e.
  // each write on this message is a dummy operation.
  // This type of message can be useful to calculate first the size of the
  // message, before allocating the real message.
  static std::unique_ptr<MediaMessage> CreateDummyMessage(uint32_t type);

  // Creates a message with a capacity of at least |msg_content_capacity|
  // bytes. The actual content size can be smaller than its capacity.
  // The message can be populated with some Write functions.
  static std::unique_ptr<MediaMessage> CreateMessage(
      uint32_t type,
      const MemoryAllocatorCB& memory_allocator,
      size_t msg_content_capacity);

  // Creates a message of type |type| whose serialized structure is stored
  // in |mem|.
  static std::unique_ptr<MediaMessage> CreateMessage(
      uint32_t type,
      std::unique_ptr<MediaMemoryChunk> mem);

  // Creates a message from a memory area which already contains
  // the serialized structure of the message.
  // Only Read functions can be invoked on this type of message.
  static std::unique_ptr<MediaMessage> MapMessage(
      std::unique_ptr<MediaMemoryChunk> mem);

  // Return the minimum size of a message.
  static size_t minimum_msg_size() {
    return offsetof(SerializedMsg, content);
  }

  ~MediaMessage();

  // Indicate whether the underlying serialized structure of the message is
  // available.
  // Note: the serialized structure might be unavailable in case of a dummy
  // message or if the underlying memory has been invalidated.
  bool IsSerializedMsgAvailable() const;

  // Return the message and the total size of the message
  // incuding the header, the content and the possible padding.
  const void* msg() const { return msg_read_only_; }
  size_t size() const { return cached_msg_.header.size; }

  // Return the size of the message without padding.
  size_t actual_size() const {
    return minimum_msg_size() + cached_msg_.header.content_size;
  }

  // Return the size of the content of the message.
  size_t content_size() const { return cached_msg_.header.content_size; }

  // Return the type of the message.
  uint32_t type() const { return cached_msg_.header.type; }

  // Append a POD to the message.
  // Return true if the POD has been succesfully written.
  template<typename T> bool WritePod(T* const& pod);
  template<typename T> bool WritePod(const T& pod) {
    return WriteBuffer(&pod, sizeof(T));
  }

  // Append a raw buffer to the message.
  bool WriteBuffer(const void* src, size_t size);

  // Read a POD from the message.
  template<typename T> bool ReadPod(T* pod) {
    return ReadBuffer(pod, sizeof(T));
  }

  // Read |size| bytes from the message from the last read position
  // and write it to |dst|.
  bool ReadBuffer(void* dst, size_t size);

  // Return a pointer to a buffer of size |size|.
  // Return NULL if not successful.
  const void* GetBuffer(size_t size);
  void* GetWritableBuffer(size_t size);

 private:
  MediaMessage(uint32_t type, size_t msg_size);
  MediaMessage(uint32_t type, std::unique_ptr<MediaMemoryChunk> memory);
  MediaMessage(std::unique_ptr<MediaMemoryChunk> memory);

  struct Header {
    // Total size of the message (including both header & content).
    uint32_t size;
    // Indicate the message type.
    uint32_t type;
    // Actual size of the content in the message.
    uint32_t content_size;
  };

  struct SerializedMsg {
    // Message header.
    Header header;

    // Start of the content of the message.
    // Use uint8_t since no special alignment is needed.
    uint8_t content;
  };

  // Indicate whether the message is a dummy message, i.e. a message without
  // a complete underlying serialized structure: only the message header is
  // available.
  bool is_dummy_msg_;

  // |cached_msg_| is used for 2 purposes:
  // - to create a dummy message
  // - for security purpose: cache the msg header to avoid browser security
  // issues.
  SerializedMsg cached_msg_;
  Header* const cached_header_;

  SerializedMsg* msg_;
  SerializedMsg* msg_read_only_;

  // Memory allocated to store the underlying serialized structure into memory.
  // Note: a dummy message has no underlying serialized structure:
  // |mem_| is a null pointer in that case.
  std::unique_ptr<MediaMemoryChunk> mem_;

  // Read iterator into the message.
  size_t rd_offset_;

  DISALLOW_COPY_AND_ASSIGN(MediaMessage);
};

}  // namespace media
}  // namespace chromecast

#endif
