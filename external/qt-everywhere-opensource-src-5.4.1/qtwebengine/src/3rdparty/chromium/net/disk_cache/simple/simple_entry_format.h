// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_H_


#include "base/basictypes.h"
#include "base/port.h"
#include "net/base/net_export.h"

namespace base {
class Time;
}

namespace disk_cache {

const uint64 kSimpleInitialMagicNumber = GG_UINT64_C(0xfcfb6d1ba7725c30);
const uint64 kSimpleFinalMagicNumber = GG_UINT64_C(0xf4fa6f45970d41d8);
const uint64 kSimpleSparseRangeMagicNumber = GG_UINT64_C(0xeb97bf016553676b);

// A file containing stream 0 and stream 1 in the Simple cache consists of:
//   - a SimpleFileHeader.
//   - the key.
//   - the data from stream 1.
//   - a SimpleFileEOF record for stream 1.
//   - the data from stream 0.
//   - a SimpleFileEOF record for stream 0.

// A file containing stream 2 in the Simple cache consists of:
//   - a SimpleFileHeader.
//   - the key.
//   - the data.
//   - at the end, a SimpleFileEOF record.
static const int kSimpleEntryFileCount = 2;
static const int kSimpleEntryStreamCount = 3;

struct NET_EXPORT_PRIVATE SimpleFileHeader {
  SimpleFileHeader();

  uint64 initial_magic_number;
  uint32 version;
  uint32 key_length;
  uint32 key_hash;
};

struct NET_EXPORT_PRIVATE SimpleFileEOF {
  enum Flags {
    FLAG_HAS_CRC32 = (1U << 0),
  };

  SimpleFileEOF();

  uint64 final_magic_number;
  uint32 flags;
  uint32 data_crc32;
  // |stream_size| is only used in the EOF record for stream 0.
  uint32 stream_size;
};

struct SimpleFileSparseRangeHeader {
  SimpleFileSparseRangeHeader();

  uint64 sparse_range_magic_number;
  int64 offset;
  int64 length;
  uint32 data_crc32;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_H_
