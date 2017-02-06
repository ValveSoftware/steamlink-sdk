// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/contiguous_container.h"

#include <stddef.h>

#include <utility>

namespace cc {

// Default number of max-sized elements to allocate space for, if there is no
// initial buffer.
static const unsigned kDefaultInitialBufferSize = 32;

class ContiguousContainerBase::Buffer {
 public:
  explicit Buffer(size_t buffer_size)
      : data_(new char[buffer_size]), end_(begin()), capacity_(buffer_size) {}

  ~Buffer() {}

  size_t Capacity() const { return capacity_; }
  size_t UsedCapacity() const { return end_ - begin(); }
  size_t UnusedCapacity() const { return Capacity() - UsedCapacity(); }
  bool empty() const { return UsedCapacity() == 0; }

  void* Allocate(size_t object_size) {
    DCHECK_GE(UnusedCapacity(), object_size);
    void* result = end_;
    end_ += object_size;
    return result;
  }

  void DeallocateLastObject(void* object) {
    DCHECK_LE(begin(), object);
    DCHECK_LT(object, end_);
    end_ = static_cast<char*>(object);
  }

 private:
  char* begin() { return &data_[0]; }
  const char* begin() const { return &data_[0]; }

  // begin() <= end_ <= begin() + capacity_
  std::unique_ptr<char[]> data_;
  char* end_;
  size_t capacity_;
};

ContiguousContainerBase::ContiguousContainerBase(size_t max_object_size)
    : end_index_(0), max_object_size_(max_object_size) {}

ContiguousContainerBase::ContiguousContainerBase(size_t max_object_size,
                                                 size_t initial_size_bytes)
    : ContiguousContainerBase(max_object_size) {
  AllocateNewBufferForNextAllocation(
      std::max(max_object_size, initial_size_bytes));
}

ContiguousContainerBase::~ContiguousContainerBase() {}

size_t ContiguousContainerBase::GetCapacityInBytes() const {
  size_t capacity = 0;
  for (const auto& buffer : buffers_)
    capacity += buffer->Capacity();
  return capacity;
}

size_t ContiguousContainerBase::UsedCapacityInBytes() const {
  size_t used_capacity = 0;
  for (const auto& buffer : buffers_)
    used_capacity += buffer->UsedCapacity();
  return used_capacity;
}

size_t ContiguousContainerBase::MemoryUsageInBytes() const {
  return sizeof(*this) + GetCapacityInBytes() +
         elements_.capacity() * sizeof(elements_[0]);
}

void* ContiguousContainerBase::Allocate(size_t object_size) {
  DCHECK_LE(object_size, max_object_size_);

  Buffer* buffer_for_alloc = nullptr;
  if (!buffers_.empty()) {
    Buffer* end_buffer = buffers_[end_index_].get();
    if (end_buffer->UnusedCapacity() >= object_size)
      buffer_for_alloc = end_buffer;
    else if (end_index_ + 1 < buffers_.size())
      buffer_for_alloc = buffers_[++end_index_].get();
  }

  if (!buffer_for_alloc) {
    size_t new_buffer_size = buffers_.empty()
                                 ? kDefaultInitialBufferSize * max_object_size_
                                 : 2 * buffers_.back()->Capacity();
    buffer_for_alloc = AllocateNewBufferForNextAllocation(new_buffer_size);
  }

  void* element = buffer_for_alloc->Allocate(object_size);
  elements_.push_back(element);
  return element;
}

void ContiguousContainerBase::RemoveLast() {
  void* object = elements_.back();
  elements_.pop_back();

  Buffer* end_buffer = buffers_[end_index_].get();
  end_buffer->DeallocateLastObject(object);

  if (end_buffer->empty()) {
    if (end_index_ > 0)
      end_index_--;
    if (end_index_ + 2 < buffers_.size())
      buffers_.pop_back();
  }
}

void ContiguousContainerBase::Clear() {
  elements_.clear();
  buffers_.clear();
  end_index_ = 0;
}

void ContiguousContainerBase::Swap(ContiguousContainerBase& other) {
  elements_.swap(other.elements_);
  buffers_.swap(other.buffers_);
  std::swap(end_index_, other.end_index_);
  std::swap(max_object_size_, other.max_object_size_);
}

ContiguousContainerBase::Buffer*
ContiguousContainerBase::AllocateNewBufferForNextAllocation(
    size_t buffer_size) {
  DCHECK(buffers_.empty() || end_index_ == buffers_.size() - 1);
  std::unique_ptr<Buffer> new_buffer(new Buffer(buffer_size));
  Buffer* buffer_to_return = new_buffer.get();
  buffers_.push_back(std::move(new_buffer));
  end_index_ = buffers_.size() - 1;
  return buffer_to_return;
}

}  // namespace cc
