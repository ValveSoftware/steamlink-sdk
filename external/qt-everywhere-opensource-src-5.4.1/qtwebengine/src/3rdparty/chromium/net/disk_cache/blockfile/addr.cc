// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/addr.h"

#include "base/logging.h"

namespace disk_cache {

int Addr::start_block() const {
  DCHECK(is_block_file());
  return value_ & kStartBlockMask;
}

int Addr::num_blocks() const {
  DCHECK(is_block_file() || !value_);
  return ((value_ & kNumBlocksMask) >> kNumBlocksOffset) + 1;
}

bool Addr::SetFileNumber(int file_number) {
  DCHECK(is_separate_file());
  if (file_number & ~kFileNameMask)
    return false;
  value_ = kInitializedMask | file_number;
  return true;
}

bool Addr::SanityCheckV2() const {
  if (!is_initialized())
    return !value_;

  if (file_type() > BLOCK_4K)
    return false;

  if (is_separate_file())
    return true;

  return !reserved_bits();
}

bool Addr::SanityCheckV3() const {
  if (!is_initialized())
    return !value_;

  // For actual entries, SanityCheckForEntryV3 should be used.
  if (file_type() > BLOCK_FILES)
    return false;

  if (is_separate_file())
    return true;

  return !reserved_bits();
}

bool Addr::SanityCheckForEntryV2() const {
  if (!SanityCheckV2() || !is_initialized())
    return false;

  if (is_separate_file() || file_type() != BLOCK_256)
    return false;

  return true;
}

bool Addr::SanityCheckForEntryV3() const {
  if (!is_initialized())
    return false;

  if (reserved_bits())
    return false;

  if (file_type() != BLOCK_ENTRIES && file_type() != BLOCK_EVICTED)
    return false;

  if (num_blocks() != 1)
    return false;

  return true;
}

bool Addr::SanityCheckForRankings() const {
  if (!SanityCheckV2() || !is_initialized())
    return false;

  if (is_separate_file() || file_type() != RANKINGS || num_blocks() != 1)
    return false;

  return true;
}

}  // namespace disk_cache
