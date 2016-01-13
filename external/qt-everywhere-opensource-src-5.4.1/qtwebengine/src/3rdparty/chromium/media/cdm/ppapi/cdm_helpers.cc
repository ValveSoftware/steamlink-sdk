// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/cdm_helpers.h"

#include <algorithm>
#include <utility>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/dev/buffer_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"

namespace media {

// static
PpbBuffer* PpbBuffer::Create(const pp::Buffer_Dev& buffer,
                             uint32_t buffer_id,
                             PpbBufferAllocator* allocator) {
  PP_DCHECK(buffer.data());
  PP_DCHECK(buffer.size());
  PP_DCHECK(buffer_id);
  PP_DCHECK(allocator);
  return new PpbBuffer(buffer, buffer_id, allocator);
}

void PpbBuffer::Destroy() {
  delete this;
}

uint32_t PpbBuffer::Capacity() const {
  return buffer_.size();
}

uint8_t* PpbBuffer::Data() {
  return static_cast<uint8_t*>(buffer_.data());
}

void PpbBuffer::SetSize(uint32_t size) {
  PP_DCHECK(size <= Capacity());
  if (size > Capacity()) {
    size_ = 0;
    return;
  }

  size_ = size;
}

pp::Buffer_Dev PpbBuffer::TakeBuffer() {
  PP_DCHECK(!buffer_.is_null());
  pp::Buffer_Dev buffer;
  std::swap(buffer, buffer_);
  buffer_id_ = 0;
  size_ = 0;
  return buffer;
}

PpbBuffer::PpbBuffer(pp::Buffer_Dev buffer,
                     uint32_t buffer_id,
                     PpbBufferAllocator* allocator)
    : buffer_(buffer), buffer_id_(buffer_id), size_(0), allocator_(allocator) {
}

PpbBuffer::~PpbBuffer() {
  PP_DCHECK(!buffer_id_ == buffer_.is_null());
  // If still owning the |buffer_|, release it in the |allocator_|.
  if (buffer_id_)
    allocator_->Release(buffer_id_);
}

cdm::Buffer* PpbBufferAllocator::Allocate(uint32_t capacity) {
  PP_DCHECK(pp::Module::Get()->core()->IsMainThread());

  if (!capacity)
    return NULL;

  pp::Buffer_Dev buffer;
  uint32_t buffer_id = 0;

  // Reuse a buffer in the free list if there is one that fits |capacity|.
  // Otherwise, create a new one.
  FreeBufferMap::iterator found = free_buffers_.lower_bound(capacity);
  if (found == free_buffers_.end()) {
    // TODO(xhwang): Report statistics about how many new buffers are allocated.
    buffer = AllocateNewBuffer(capacity);
    if (buffer.is_null())
      return NULL;
    buffer_id = next_buffer_id_++;
  } else {
    buffer = found->second.second;
    buffer_id = found->second.first;
    free_buffers_.erase(found);
  }

  allocated_buffers_.insert(std::make_pair(buffer_id, buffer));

  return PpbBuffer::Create(buffer, buffer_id, this);
}

void PpbBufferAllocator::Release(uint32_t buffer_id) {
  if (!buffer_id)
    return;

  AllocatedBufferMap::iterator found = allocated_buffers_.find(buffer_id);
  if (found == allocated_buffers_.end())
    return;

  pp::Buffer_Dev& buffer = found->second;
  free_buffers_.insert(
      std::make_pair(buffer.size(), std::make_pair(buffer_id, buffer)));

  allocated_buffers_.erase(found);
}

pp::Buffer_Dev PpbBufferAllocator::AllocateNewBuffer(uint32_t capacity) {
  // Always pad new allocated buffer so that we don't need to reallocate
  // buffers frequently if requested sizes fluctuate slightly.
  static const uint32_t kBufferPadding = 512;

  // Maximum number of free buffers we can keep when allocating new buffers.
  static const uint32_t kFreeLimit = 3;

  // Destroy the smallest buffer before allocating a new bigger buffer if the
  // number of free buffers exceeds a limit. This mechanism helps avoid ending
  // up with too many small buffers, which could happen if the size to be
  // allocated keeps increasing.
  if (free_buffers_.size() >= kFreeLimit)
    free_buffers_.erase(free_buffers_.begin());

  // Creation of pp::Buffer_Dev is expensive! It involves synchronous IPC calls.
  // That's why we try to avoid AllocateNewBuffer() as much as we can.
  return pp::Buffer_Dev(instance_, capacity + kBufferPadding);
}

VideoFrameImpl::VideoFrameImpl()
    : format_(cdm::kUnknownVideoFormat),
      frame_buffer_(NULL),
      timestamp_(0) {
  for (uint32_t i = 0; i < kMaxPlanes; ++i) {
    plane_offsets_[i] = 0;
    strides_[i] = 0;
  }
}

VideoFrameImpl::~VideoFrameImpl() {
  if (frame_buffer_)
    frame_buffer_->Destroy();
}

}  // namespace media
