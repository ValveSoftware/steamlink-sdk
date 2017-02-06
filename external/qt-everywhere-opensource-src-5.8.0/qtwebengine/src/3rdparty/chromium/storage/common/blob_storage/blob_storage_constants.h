// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_
#define STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

namespace storage {

// TODO(michaeln): use base::SysInfo::AmountOfPhysicalMemoryMB() in some
// way to come up with a better limit.
const int64_t kBlobStorageMaxMemoryUsage = 500 * 1024 * 1024;  // Half a gig.
const size_t kBlobStorageIPCThresholdBytes = 250 * 1024;
const size_t kBlobStorageMaxSharedMemoryBytes = 10 * 1024 * 1024;
const uint64_t kBlobStorageMaxFileSizeBytes = 100 * 1024 * 1024;
const uint64_t kBlobStorageMinFileSizeBytes = 1 * 1024 * 1024;
const size_t kBlobStorageMaxBlobMemorySize =
    kBlobStorageMaxMemoryUsage - kBlobStorageMinFileSizeBytes;

enum class IPCBlobItemRequestStrategy {
  UNKNOWN = 0,
  IPC,
  SHARED_MEMORY,
  FILE,
  LAST = FILE
};

// These items cannot be reordered or renumbered because they're recorded to
// UMA. New items must be added immediately before LAST, and LAST must be set to
// the the last item.
enum class IPCBlobCreationCancelCode {
  UNKNOWN = 0,
  OUT_OF_MEMORY = 1,
  // We couldn't create or write to a file. File system error, like a full disk.
  FILE_WRITE_FAILED = 2,
  // The renderer was destroyed while data was in transit.
  SOURCE_DIED_IN_TRANSIT = 3,
  // The renderer destructed the blob before it was done transferring, and there
  // were no outstanding references (no one is waiting to read) to keep the
  // blob alive.
  BLOB_DEREFERENCED_WHILE_BUILDING = 4,
  // A blob that we referenced during construction is broken, or a browser-side
  // builder tries to build a blob with a blob reference that isn't finished
  // constructing.
  REFERENCED_BLOB_BROKEN = 5,
  LAST = REFERENCED_BLOB_BROKEN
};

}  // namespace storage

#endif  // STORAGE_COMMON_BLOB_STORAGE_BLOB_STORAGE_CONSTANTS_H_
