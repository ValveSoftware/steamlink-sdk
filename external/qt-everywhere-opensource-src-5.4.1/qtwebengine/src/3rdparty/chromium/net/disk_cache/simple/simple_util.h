// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_UTIL_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
class Time;
}

namespace disk_cache {

namespace simple_util {

NET_EXPORT_PRIVATE std::string ConvertEntryHashKeyToHexString(uint64 hash_key);

// |key| is the regular cache key, such as an URL.
// Returns the Hex ascii representation of the uint64 hash_key.
NET_EXPORT_PRIVATE std::string GetEntryHashKeyAsHexString(
    const std::string& key);

// |key| is the regular HTTP Cache key, which is a URL.
// Returns the hash of the key as uint64.
NET_EXPORT_PRIVATE uint64 GetEntryHashKey(const std::string& key);

// Parses the |hash_key| string into a uint64 buffer.
// |hash_key| string must be of the form: FFFFFFFFFFFFFFFF .
NET_EXPORT_PRIVATE bool GetEntryHashKeyFromHexString(
    const base::StringPiece& hash_key,
    uint64* hash_key_out);

// Given a |key| for a (potential) entry in the simple backend and the |index|
// of a stream on that entry, returns the filename in which that stream would be
// stored.
NET_EXPORT_PRIVATE std::string GetFilenameFromKeyAndFileIndex(
    const std::string& key,
    int file_index);

// Same as |GetFilenameFromKeyAndIndex| above, but using a hex string.
std::string GetFilenameFromEntryHashAndFileIndex(uint64 entry_hash,
                                                 int file_index);

// Given a |key| for an entry, returns the name of the sparse data file.
std::string GetSparseFilenameFromEntryHash(uint64 entry_hash);

// Given the size of a file holding a stream in the simple backend and the key
// to an entry, returns the number of bytes in the stream.
NET_EXPORT_PRIVATE int32 GetDataSizeFromKeyAndFileSize(const std::string& key,
                                                       int64 file_size);

// Given the size of a stream in the simple backend and the key to an entry,
// returns the number of bytes in the file.
NET_EXPORT_PRIVATE int64 GetFileSizeFromKeyAndDataSize(const std::string& key,
                                                       int32 data_size);

// Given the stream index, returns the number of the file the stream is stored
// in.
NET_EXPORT_PRIVATE int GetFileIndexFromStreamIndex(int stream_index);

// Fills |out_time| with the time the file last modified time. Unlike the
// functions in file.h, the time resolution is milliseconds.
NET_EXPORT_PRIVATE bool GetMTime(const base::FilePath& path,
                                 base::Time* out_mtime);
}  // namespace simple_backend

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_UTIL_H_
