// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_CONSTANTS_H_
#define MOJO_SYSTEM_CONSTANTS_H_

#include <stddef.h>

namespace mojo {
namespace system {

// Maximum number of open (Mojo) handles.
// TODO(vtl): This doesn't count "live" handles, some of which may live in
// messages.
const size_t kMaxHandleTableSize = 1000000;

// Maximum number of active memory mappings.
const size_t kMaxMappingTableSize = 1000000;

const size_t kMaxWaitManyNumHandles = kMaxHandleTableSize;

const size_t kMaxMessageNumBytes = 4 * 1024 * 1024;

const size_t kMaxMessageNumHandles = 10000;

// Maximum capacity of a data pipe, in bytes. This value must fit into a
// |uint32_t|.
// WARNING: If you bump it closer to 2^32, you must audit all the code to check
// that we don't overflow (2^31 would definitely be risky; up to 2^30 is
// probably okay).
const size_t kMaxDataPipeCapacityBytes = 256 * 1024 * 1024;  // 256 MB.

const size_t kDefaultDataPipeCapacityBytes = 1024 * 1024;  // 1 MB.

// Alignment for the "start" of the data buffer used by data pipes. (The
// alignment of elements will depend on this and the element size.)
const size_t kDataPipeBufferAlignmentBytes = 16;

// TODO(vtl): Set this hard limit appropriately (e.g., higher on 64-bit). (This
// will also entail some auditing to make sure I'm not messing up my checks
// anywhere.)
const size_t kMaxSharedMemoryNumBytes = 1024 * 1024 * 1024;  // 1 GB.

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_CONSTANTS_H_
