// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EDK_SYSTEM_MAPPING_TABLE_H_
#define MOJO_EDK_SYSTEM_MAPPING_TABLE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "mojo/edk/system/system_impl_export.h"
#include "mojo/public/c/system/types.h"

namespace mojo {

namespace edk {
class Core;
class PlatformSharedBufferMapping;

// Test-only function (defined/used in embedder/test_embedder.cc). Declared here
// so it can be friended.
namespace internal {
bool ShutdownCheckNoLeaks(Core*);
}

// This class provides the (global) table of memory mappings (owned by |Core|),
// which maps mapping base addresses to |PlatformSharedBufferMapping|s.
//
// This class is NOT thread-safe; locking is left to |Core|.
class MOJO_SYSTEM_IMPL_EXPORT MappingTable {
 public:
  MappingTable();
  ~MappingTable();

  // Tries to add a mapping. (Takes ownership of the mapping in all cases; on
  // failure, it will be destroyed.)
  MojoResult AddMapping(std::unique_ptr<PlatformSharedBufferMapping> mapping);
  MojoResult RemoveMapping(void* address);

 private:
  friend bool internal::ShutdownCheckNoLeaks(Core*);

  using AddressToMappingMap =
      base::hash_map<void*, PlatformSharedBufferMapping*>;
  AddressToMappingMap address_to_mapping_map_;

  DISALLOW_COPY_AND_ASSIGN(MappingTable);
};

}  // namespace edk
}  // namespace mojo

#endif  // MOJO_EDK_SYSTEM_MAPPING_TABLE_H_
