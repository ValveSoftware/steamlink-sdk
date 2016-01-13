// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/lazy_instance.h"
#include "base/logging.h"

namespace base {
namespace {

const struct TypeNamePair {
  DiscardableMemoryType type;
  const char* name;
} kTypeNamePairs[] = {
  { DISCARDABLE_MEMORY_TYPE_ASHMEM, "ashmem" },
  { DISCARDABLE_MEMORY_TYPE_MAC, "mac" },
  { DISCARDABLE_MEMORY_TYPE_EMULATED, "emulated" },
  { DISCARDABLE_MEMORY_TYPE_MALLOC, "malloc" }
};

DiscardableMemoryType g_preferred_type = DISCARDABLE_MEMORY_TYPE_NONE;

struct DefaultPreferredType {
  DefaultPreferredType() : value(DISCARDABLE_MEMORY_TYPE_NONE) {
    std::vector<DiscardableMemoryType> supported_types;
    DiscardableMemory::GetSupportedTypes(&supported_types);
    DCHECK(!supported_types.empty());
    value = supported_types[0];
  }
  DiscardableMemoryType value;
};
LazyInstance<DefaultPreferredType>::Leaky g_default_preferred_type =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
DiscardableMemoryType DiscardableMemory::GetNamedType(
    const std::string& name) {
  for (size_t i = 0; i < arraysize(kTypeNamePairs); ++i) {
    if (name == kTypeNamePairs[i].name)
      return kTypeNamePairs[i].type;
  }

  return DISCARDABLE_MEMORY_TYPE_NONE;
}

// static
const char* DiscardableMemory::GetTypeName(DiscardableMemoryType type) {
  for (size_t i = 0; i < arraysize(kTypeNamePairs); ++i) {
    if (type == kTypeNamePairs[i].type)
      return kTypeNamePairs[i].name;
  }

  return "unknown";
}

// static
void DiscardableMemory::SetPreferredType(DiscardableMemoryType type) {
  // NONE is a reserved value and not a valid default type.
  DCHECK_NE(DISCARDABLE_MEMORY_TYPE_NONE, type);

  // Make sure this function is only called once before the first call
  // to GetPreferredType().
  DCHECK_EQ(DISCARDABLE_MEMORY_TYPE_NONE, g_preferred_type);

  g_preferred_type = type;
}

// static
DiscardableMemoryType DiscardableMemory::GetPreferredType() {
  if (g_preferred_type == DISCARDABLE_MEMORY_TYPE_NONE)
    g_preferred_type = g_default_preferred_type.Get().value;

  return g_preferred_type;
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
  return CreateLockedMemoryWithType(GetPreferredType(), size);
}

}  // namespace base
