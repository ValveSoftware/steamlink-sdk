// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/bindings_serialization.h"

#include <assert.h>

#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/bounds_checker.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {
namespace internal {

namespace {

const size_t kAlignment = 8;

template<typename T>
T AlignImpl(T t) {
  return t + (kAlignment - (t % kAlignment)) % kAlignment;
}

}  // namespace

size_t Align(size_t size) {
  return AlignImpl(size);
}

char* AlignPointer(char* ptr) {
  return reinterpret_cast<char*>(AlignImpl(reinterpret_cast<uintptr_t>(ptr)));
}

bool IsAligned(const void* ptr) {
  return !(reinterpret_cast<uintptr_t>(ptr) % kAlignment);
}

void EncodePointer(const void* ptr, uint64_t* offset) {
  if (!ptr) {
    *offset = 0;
    return;
  }

  const char* p_obj = reinterpret_cast<const char*>(ptr);
  const char* p_slot = reinterpret_cast<const char*>(offset);
  assert(p_obj > p_slot);

  *offset = static_cast<uint64_t>(p_obj - p_slot);
}

const void* DecodePointerRaw(const uint64_t* offset) {
  if (!*offset)
    return NULL;
  return reinterpret_cast<const char*>(offset) + *offset;
}

bool ValidateEncodedPointer(const uint64_t* offset) {
  // Cast to uintptr_t so overflow behavior is well defined.
  return reinterpret_cast<uintptr_t>(offset) + *offset >=
      reinterpret_cast<uintptr_t>(offset);
}

void EncodeHandle(Handle* handle, std::vector<Handle>* handles) {
  if (handle->is_valid()) {
    handles->push_back(*handle);
    handle->set_value(static_cast<MojoHandle>(handles->size() - 1));
  } else {
    handle->set_value(kEncodedInvalidHandleValue);
  }
}

void DecodeHandle(Handle* handle, std::vector<Handle>* handles) {
  if (handle->value() == kEncodedInvalidHandleValue) {
    *handle = Handle();
    return;
  }
  assert(handle->value() < handles->size());
  // Just leave holes in the vector so we don't screw up other indices.
  *handle = FetchAndReset(&handles->at(handle->value()));
}

bool ValidateStructHeader(const void* data,
                          uint32_t min_num_bytes,
                          uint32_t min_num_fields,
                          BoundsChecker* bounds_checker) {
  assert(min_num_bytes >= sizeof(StructHeader));

  if (!IsAligned(data)) {
    ReportValidationError(VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }
  if (!bounds_checker->IsValidRange(data, sizeof(StructHeader))) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  const StructHeader* header = static_cast<const StructHeader*>(data);

  // TODO(yzshen): Currently our binding code cannot handle structs of smaller
  // size or with fewer fields than the version that it sees. That needs to be
  // changed in order to provide backward compatibility.
  if (header->num_bytes < min_num_bytes ||
      header->num_fields < min_num_fields) {
    ReportValidationError(VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  if (!bounds_checker->ClaimMemory(data, header->num_bytes)) {
    ReportValidationError(VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

}  // namespace internal
}  // namespace mojo
