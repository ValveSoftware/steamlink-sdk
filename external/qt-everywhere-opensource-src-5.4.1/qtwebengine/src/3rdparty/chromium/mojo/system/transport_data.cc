// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/transport_data.h"

#include <string.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/system/channel.h"
#include "mojo/system/constants.h"
#include "mojo/system/message_in_transit.h"

namespace mojo {
namespace system {

// The maximum amount of space needed per platform handle.
// (|{Channel,RawChannel}::GetSerializedPlatformHandleSize()| should always
// return a value which is at most this. This is only used to calculate
// |TransportData::kMaxBufferSize|. This value should be a multiple of the
// alignment in order to simplify calculations, even though the actual amount of
// space needed need not be a multiple of the alignment.
const size_t kMaxSizePerPlatformHandle = 8;
COMPILE_ASSERT(kMaxSizePerPlatformHandle %
                   MessageInTransit::kMessageAlignment == 0,
               kMaxSizePerPlatformHandle_not_a_multiple_of_alignment);

STATIC_CONST_MEMBER_DEFINITION const size_t
    TransportData::kMaxSerializedDispatcherSize;
STATIC_CONST_MEMBER_DEFINITION const size_t
    TransportData::kMaxSerializedDispatcherPlatformHandles;

// static
const size_t TransportData::kMaxPlatformHandles =
    kMaxMessageNumHandles * kMaxSerializedDispatcherPlatformHandles;

// In additional to the header, for each attached (Mojo) handle there'll be a
// handle table entry and serialized dispatcher data.
// Note: This definition must follow the one for |kMaxPlatformHandles|;
// otherwise, we get a static initializer with gcc (but not clang).
// static
const size_t TransportData::kMaxBufferSize =
    sizeof(Header) +
    kMaxMessageNumHandles * (sizeof(HandleTableEntry) +
                                 kMaxSerializedDispatcherSize) +
    kMaxPlatformHandles * kMaxSizePerPlatformHandle;

struct TransportData::PrivateStructForCompileAsserts {
  // The size of |Header| must be a multiple of the alignment.
  COMPILE_ASSERT(sizeof(Header) % MessageInTransit::kMessageAlignment == 0,
                 sizeof_MessageInTransit_Header_invalid);

  // The maximum serialized dispatcher size must be a multiple of the alignment.
  COMPILE_ASSERT(kMaxSerializedDispatcherSize %
                     MessageInTransit::kMessageAlignment == 0,
                 kMaxSerializedDispatcherSize_not_a_multiple_of_alignment);

  // The size of |HandleTableEntry| must be a multiple of the alignment.
  COMPILE_ASSERT(sizeof(HandleTableEntry) %
                     MessageInTransit::kMessageAlignment == 0,
                 sizeof_MessageInTransit_HandleTableEntry_invalid);
};

TransportData::TransportData(scoped_ptr<DispatcherVector> dispatchers,
                             Channel* channel) {
  DCHECK(dispatchers);
  DCHECK(channel);

  const size_t num_handles = dispatchers->size();
  DCHECK_GT(num_handles, 0u);

  // The offset to the start of the (Mojo) handle table.
  const size_t handle_table_start_offset = sizeof(Header);
  // The offset to the start of the serialized dispatcher data.
  const size_t serialized_dispatcher_start_offset =
      handle_table_start_offset + num_handles * sizeof(HandleTableEntry);
  // The estimated size of the secondary buffer. We compute this estimate below.
  // It must be at least as big as the (eventual) actual size.
  size_t estimated_size = serialized_dispatcher_start_offset;
  size_t estimated_num_platform_handles = 0;
#if DCHECK_IS_ON
  std::vector<size_t> all_max_sizes(num_handles);
  std::vector<size_t> all_max_platform_handles(num_handles);
#endif
  for (size_t i = 0; i < num_handles; i++) {
    if (Dispatcher* dispatcher = (*dispatchers)[i].get()) {
      size_t max_size = 0;
      size_t max_platform_handles = 0;
      Dispatcher::TransportDataAccess::StartSerialize(
              dispatcher, channel, &max_size, &max_platform_handles);

      DCHECK_LE(max_size, kMaxSerializedDispatcherSize);
      estimated_size += MessageInTransit::RoundUpMessageAlignment(max_size);
      DCHECK_LE(estimated_size, kMaxBufferSize);

      DCHECK_LE(max_platform_handles,
                kMaxSerializedDispatcherPlatformHandles);
      estimated_num_platform_handles += max_platform_handles;
      DCHECK_LE(estimated_num_platform_handles, kMaxPlatformHandles);

#if DCHECK_IS_ON
      all_max_sizes[i] = max_size;
      all_max_platform_handles[i] = max_platform_handles;
#endif
    }
  }

  size_t size_per_platform_handle = 0;
  if (estimated_num_platform_handles > 0) {
    size_per_platform_handle = channel->GetSerializedPlatformHandleSize();
    DCHECK_LE(size_per_platform_handle, kMaxSizePerPlatformHandle);
    estimated_size += estimated_num_platform_handles * size_per_platform_handle;
    estimated_size = MessageInTransit::RoundUpMessageAlignment(estimated_size);
    DCHECK_LE(estimated_size, kMaxBufferSize);
  }

  buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(estimated_size, MessageInTransit::kMessageAlignment)));
  // Entirely clear out the secondary buffer, since then we won't have to worry
  // about clearing padding or unused space (e.g., if a dispatcher fails to
  // serialize).
  memset(buffer_.get(), 0, estimated_size);

  if (estimated_num_platform_handles > 0) {
    DCHECK(!platform_handles_);
    platform_handles_.reset(new embedder::PlatformHandleVector());
  }

  Header* header = reinterpret_cast<Header*>(buffer_.get());
  header->num_handles = static_cast<uint32_t>(num_handles);
  // (Okay to leave |platform_handle_table_offset|, |num_platform_handles|, and
  // |unused| be zero; we'll set the former two later if necessary.)

  HandleTableEntry* handle_table = reinterpret_cast<HandleTableEntry*>(
      buffer_.get() + handle_table_start_offset);
  size_t current_offset = serialized_dispatcher_start_offset;
  for (size_t i = 0; i < num_handles; i++) {
    Dispatcher* dispatcher = (*dispatchers)[i].get();
    if (!dispatcher) {
      COMPILE_ASSERT(Dispatcher::kTypeUnknown == 0,
                     value_of_Dispatcher_kTypeUnknown_must_be_zero);
      continue;
    }

#if DCHECK_IS_ON
    size_t old_platform_handles_size =
        platform_handles_ ? platform_handles_->size() : 0;
#endif

    void* destination = buffer_.get() + current_offset;
    size_t actual_size = 0;
    if (Dispatcher::TransportDataAccess::EndSerializeAndClose(
            dispatcher, channel, destination, &actual_size,
            platform_handles_.get())) {
      handle_table[i].type = static_cast<int32_t>(dispatcher->GetType());
      handle_table[i].offset = static_cast<uint32_t>(current_offset);
      handle_table[i].size = static_cast<uint32_t>(actual_size);
      // (Okay to not set |unused| since we cleared the entire buffer.)

#if DCHECK_IS_ON
      DCHECK_LE(actual_size, all_max_sizes[i]);
      DCHECK_LE(platform_handles_ ? (platform_handles_->size() -
                                         old_platform_handles_size) : 0,
                all_max_platform_handles[i]);
#endif
    } else {
      // Nothing to do on failure, since |buffer_| was cleared, and
      // |kTypeUnknown| is zero. The handle was simply closed.
      LOG(ERROR) << "Failed to serialize handle to remote message pipe";
    }

    current_offset += MessageInTransit::RoundUpMessageAlignment(actual_size);
    DCHECK_LE(current_offset, estimated_size);
    DCHECK_LE(platform_handles_ ? platform_handles_->size() : 0,
              estimated_num_platform_handles);
  }

  if (platform_handles_ && platform_handles_->size() > 0) {
    header->platform_handle_table_offset =
        static_cast<uint32_t>(current_offset);
    header->num_platform_handles =
        static_cast<uint32_t>(platform_handles_->size());
    current_offset += platform_handles_->size() * size_per_platform_handle;
    current_offset = MessageInTransit::RoundUpMessageAlignment(current_offset);
  }

  // There's no aligned realloc, so it's no good way to release unused space (if
  // we overshot our estimated space requirements).
  buffer_size_ = current_offset;

  // |dispatchers_| will be destroyed as it goes out of scope.
}

#if defined(OS_POSIX)
TransportData::TransportData(
    embedder::ScopedPlatformHandleVectorPtr platform_handles)
    : buffer_size_(sizeof(Header)),
      platform_handles_(platform_handles.Pass()) {
  buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(buffer_size_, MessageInTransit::kMessageAlignment)));
  memset(buffer_.get(), 0, buffer_size_);
}
#endif  // defined(OS_POSIX)

TransportData::~TransportData() {
}

// static
const char* TransportData::ValidateBuffer(
    size_t serialized_platform_handle_size,
    const void* buffer,
    size_t buffer_size) {
  DCHECK(buffer);
  DCHECK_GT(buffer_size, 0u);

  // Always make sure that the buffer size is sane; if it's not, someone's
  // messing with us.
  if (buffer_size < sizeof(Header) || buffer_size > kMaxBufferSize ||
      buffer_size % MessageInTransit::kMessageAlignment != 0)
    return "Invalid message secondary buffer size";

  const Header* header = static_cast<const Header*>(buffer);
  const size_t num_handles = header->num_handles;

#if !defined(OS_POSIX)
  // On POSIX, we send control messages with platform handles (but no handles)
  // attached (see the comments for
  // |TransportData(embedder::ScopedPlatformHandleVectorPtr)|. (This check isn't
  // important security-wise anyway.)
  if (num_handles == 0)
    return "Message has no handles attached, but secondary buffer present";
#endif

  // Sanity-check |num_handles| (before multiplying it against anything).
  if (num_handles > kMaxMessageNumHandles)
    return "Message handle payload too large";

  if (buffer_size < sizeof(Header) + num_handles * sizeof(HandleTableEntry))
    return "Message secondary buffer too small";

  if (header->num_platform_handles == 0) {
    // Then |platform_handle_table_offset| should also be zero.
    if (header->platform_handle_table_offset != 0) {
      return
          "Message has no handles attached, but platform handle table present";
    }
  } else {
    // |num_handles| has already been validated, so the multiplication is okay.
    if (header->num_platform_handles >
            num_handles * kMaxSerializedDispatcherPlatformHandles)
      return "Message has too many platform handles attached";

    static const char kInvalidPlatformHandleTableOffset[] =
        "Message has invalid platform handle table offset";
    // This doesn't check that the platform handle table doesn't alias other
    // stuff, but it doesn't matter, since it's all read-only.
    if (header->platform_handle_table_offset %
            MessageInTransit::kMessageAlignment != 0)
      return kInvalidPlatformHandleTableOffset;

    // ">" instead of ">=" since the size per handle may be zero.
    if (header->platform_handle_table_offset > buffer_size)
      return kInvalidPlatformHandleTableOffset;

    // We already checked |platform_handle_table_offset| and
    // |num_platform_handles|, so the addition and multiplication are okay.
    if (header->platform_handle_table_offset +
            header->num_platform_handles * serialized_platform_handle_size >
            buffer_size)
      return kInvalidPlatformHandleTableOffset;
  }

  const HandleTableEntry* handle_table =
      reinterpret_cast<const HandleTableEntry*>(
          static_cast<const char*>(buffer) + sizeof(Header));
  static const char kInvalidSerializedDispatcher[] =
      "Message contains invalid serialized dispatcher";
  for (size_t i = 0; i < num_handles; i++) {
    size_t offset = handle_table[i].offset;
    if (offset % MessageInTransit::kMessageAlignment != 0)
      return kInvalidSerializedDispatcher;

    size_t size = handle_table[i].size;
    if (size > kMaxSerializedDispatcherSize || size > buffer_size)
      return kInvalidSerializedDispatcher;

    // Note: This is an overflow-safe check for |offset + size > buffer_size|
    // (we know that |size <= buffer_size| from the previous check).
    if (offset > buffer_size - size)
      return kInvalidSerializedDispatcher;
  }

  return NULL;
}

// static
void TransportData::GetPlatformHandleTable(const void* transport_data_buffer,
                                           size_t* num_platform_handles,
                                           const void** platform_handle_table) {
  DCHECK(transport_data_buffer);
  DCHECK(num_platform_handles);
  DCHECK(platform_handle_table);

  const Header* header = static_cast<const Header*>(transport_data_buffer);
  *num_platform_handles = header->num_platform_handles;
  *platform_handle_table = static_cast<const char*>(transport_data_buffer) +
      header->platform_handle_table_offset;
}

// static
scoped_ptr<DispatcherVector> TransportData::DeserializeDispatchers(
    const void* buffer,
    size_t buffer_size,
    embedder::ScopedPlatformHandleVectorPtr platform_handles,
    Channel* channel) {
  DCHECK(buffer);
  DCHECK_GT(buffer_size, 0u);
  DCHECK(channel);

  const Header* header = static_cast<const Header*>(buffer);
  const size_t num_handles = header->num_handles;
  scoped_ptr<DispatcherVector> dispatchers(new DispatcherVector(num_handles));

  const HandleTableEntry* handle_table =
      reinterpret_cast<const HandleTableEntry*>(
          static_cast<const char*>(buffer) + sizeof(Header));
  for (size_t i = 0; i < num_handles; i++) {
    size_t offset = handle_table[i].offset;
    size_t size = handle_table[i].size;
    // Should already have been checked by |ValidateBuffer()|:
    DCHECK_EQ(offset % MessageInTransit::kMessageAlignment, 0u);
    DCHECK_LE(offset, buffer_size);
    DCHECK_LE(offset + size, buffer_size);

    const void* source = static_cast<const char*>(buffer) + offset;
    (*dispatchers)[i] = Dispatcher::TransportDataAccess::Deserialize(
        channel, handle_table[i].type, source, size, platform_handles.get());
  }

  return dispatchers.Pass();
}

}  // namespace system
}  // namespace mojo
