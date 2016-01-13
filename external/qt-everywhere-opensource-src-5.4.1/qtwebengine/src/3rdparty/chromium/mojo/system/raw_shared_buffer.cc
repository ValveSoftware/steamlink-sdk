// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

#include "base/logging.h"
#include "mojo/embedder/platform_handle_utils.h"

namespace mojo {
namespace system {

// static
RawSharedBuffer* RawSharedBuffer::Create(size_t num_bytes) {
  DCHECK_GT(num_bytes, 0u);

  RawSharedBuffer* rv = new RawSharedBuffer(num_bytes);
  if (!rv->Init()) {
    // We can't just delete it directly, due to the "in destructor" (debug)
    // check.
    scoped_refptr<RawSharedBuffer> deleter(rv);
    return NULL;
  }

  return rv;
}

RawSharedBuffer* RawSharedBuffer::CreateFromPlatformHandle(
    size_t num_bytes,
    embedder::ScopedPlatformHandle platform_handle) {
  DCHECK_GT(num_bytes, 0u);

  RawSharedBuffer* rv = new RawSharedBuffer(num_bytes);
  if (!rv->InitFromPlatformHandle(platform_handle.Pass())) {
    // We can't just delete it directly, due to the "in destructor" (debug)
    // check.
    scoped_refptr<RawSharedBuffer> deleter(rv);
    return NULL;
  }

  return rv;
}

scoped_ptr<RawSharedBufferMapping> RawSharedBuffer::Map(size_t offset,
                                                        size_t length) {
  if (!IsValidMap(offset, length))
    return scoped_ptr<RawSharedBufferMapping>();

  return MapNoCheck(offset, length);
}

bool RawSharedBuffer::IsValidMap(size_t offset, size_t length) {
  if (offset > num_bytes_ || length == 0)
    return false;

  // Note: This is an overflow-safe check of |offset + length > num_bytes_|
  // (that |num_bytes >= offset| is verified above).
  if (length > num_bytes_ - offset)
    return false;

  return true;
}

scoped_ptr<RawSharedBufferMapping> RawSharedBuffer::MapNoCheck(size_t offset,
                                                               size_t length) {
  DCHECK(IsValidMap(offset, length));
  return MapImpl(offset, length);
}

embedder::ScopedPlatformHandle RawSharedBuffer::DuplicatePlatformHandle() {
  return embedder::DuplicatePlatformHandle(handle_.get());
}

embedder::ScopedPlatformHandle RawSharedBuffer::PassPlatformHandle() {
  DCHECK(HasOneRef());
  return handle_.Pass();
}

RawSharedBuffer::RawSharedBuffer(size_t num_bytes) : num_bytes_(num_bytes) {
}

RawSharedBuffer::~RawSharedBuffer() {
}

}  // namespace system
}  // namespace mojo
