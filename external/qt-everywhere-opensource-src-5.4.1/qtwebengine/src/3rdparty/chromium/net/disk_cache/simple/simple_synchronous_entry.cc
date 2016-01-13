// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/simple/simple_synchronous_entry.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <limits>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/location.h"
#include "base/sha1.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/simple/simple_backend_version.h"
#include "net/disk_cache/simple/simple_histogram_macros.h"
#include "net/disk_cache/simple/simple_util.h"
#include "third_party/zlib/zlib.h"

using base::File;
using base::FilePath;
using base::Time;

namespace {

// Used in histograms, please only add entries at the end.
enum OpenEntryResult {
  OPEN_ENTRY_SUCCESS = 0,
  OPEN_ENTRY_PLATFORM_FILE_ERROR = 1,
  OPEN_ENTRY_CANT_READ_HEADER = 2,
  OPEN_ENTRY_BAD_MAGIC_NUMBER = 3,
  OPEN_ENTRY_BAD_VERSION = 4,
  OPEN_ENTRY_CANT_READ_KEY = 5,
  // OPEN_ENTRY_KEY_MISMATCH = 6, Deprecated.
  OPEN_ENTRY_KEY_HASH_MISMATCH = 7,
  OPEN_ENTRY_SPARSE_OPEN_FAILED = 8,
  OPEN_ENTRY_MAX = 9,
};

// Used in histograms, please only add entries at the end.
enum WriteResult {
  WRITE_RESULT_SUCCESS = 0,
  WRITE_RESULT_PRETRUNCATE_FAILURE,
  WRITE_RESULT_WRITE_FAILURE,
  WRITE_RESULT_TRUNCATE_FAILURE,
  WRITE_RESULT_LAZY_STREAM_ENTRY_DOOMED,
  WRITE_RESULT_LAZY_CREATE_FAILURE,
  WRITE_RESULT_LAZY_INITIALIZE_FAILURE,
  WRITE_RESULT_MAX,
};

// Used in histograms, please only add entries at the end.
enum CheckEOFResult {
  CHECK_EOF_RESULT_SUCCESS,
  CHECK_EOF_RESULT_READ_FAILURE,
  CHECK_EOF_RESULT_MAGIC_NUMBER_MISMATCH,
  CHECK_EOF_RESULT_CRC_MISMATCH,
  CHECK_EOF_RESULT_MAX,
};

// Used in histograms, please only add entries at the end.
enum CloseResult {
  CLOSE_RESULT_SUCCESS,
  CLOSE_RESULT_WRITE_FAILURE,
};

void RecordSyncOpenResult(net::CacheType cache_type,
                          OpenEntryResult result,
                          bool had_index) {
  DCHECK_GT(OPEN_ENTRY_MAX, result);
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "SyncOpenResult", cache_type, result, OPEN_ENTRY_MAX);
  if (had_index) {
    SIMPLE_CACHE_UMA(ENUMERATION,
                     "SyncOpenResult_WithIndex", cache_type,
                     result, OPEN_ENTRY_MAX);
  } else {
    SIMPLE_CACHE_UMA(ENUMERATION,
                     "SyncOpenResult_WithoutIndex", cache_type,
                     result, OPEN_ENTRY_MAX);
  }
}

void RecordWriteResult(net::CacheType cache_type, WriteResult result) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "SyncWriteResult", cache_type, result, WRITE_RESULT_MAX);
}

void RecordCheckEOFResult(net::CacheType cache_type, CheckEOFResult result) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "SyncCheckEOFResult", cache_type,
                   result, CHECK_EOF_RESULT_MAX);
}

void RecordCloseResult(net::CacheType cache_type, CloseResult result) {
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "SyncCloseResult", cache_type, result, WRITE_RESULT_MAX);
}

bool CanOmitEmptyFile(int file_index) {
  DCHECK_LE(0, file_index);
  DCHECK_GT(disk_cache::kSimpleEntryFileCount, file_index);
  return file_index == disk_cache::simple_util::GetFileIndexFromStreamIndex(2);
}

}  // namespace

namespace disk_cache {

using simple_util::GetEntryHashKey;
using simple_util::GetFilenameFromEntryHashAndFileIndex;
using simple_util::GetSparseFilenameFromEntryHash;
using simple_util::GetDataSizeFromKeyAndFileSize;
using simple_util::GetFileSizeFromKeyAndDataSize;
using simple_util::GetFileIndexFromStreamIndex;

SimpleEntryStat::SimpleEntryStat(base::Time last_used,
                                 base::Time last_modified,
                                 const int32 data_size[],
                                 const int32 sparse_data_size)
    : last_used_(last_used),
      last_modified_(last_modified),
      sparse_data_size_(sparse_data_size) {
  memcpy(data_size_, data_size, sizeof(data_size_));
}

int SimpleEntryStat::GetOffsetInFile(const std::string& key,
                                     int offset,
                                     int stream_index) const {
  const int64 headers_size = sizeof(SimpleFileHeader) + key.size();
  const int64 additional_offset =
      stream_index == 0 ? data_size_[1] + sizeof(SimpleFileEOF) : 0;
  return headers_size + offset + additional_offset;
}

int SimpleEntryStat::GetEOFOffsetInFile(const std::string& key,
                                        int stream_index) const {
  return GetOffsetInFile(key, data_size_[stream_index], stream_index);
}

int SimpleEntryStat::GetLastEOFOffsetInFile(const std::string& key,
                                            int stream_index) const {
  const int file_index = GetFileIndexFromStreamIndex(stream_index);
  const int eof_data_offset =
      file_index == 0 ? data_size_[0] + data_size_[1] + sizeof(SimpleFileEOF)
                      : data_size_[2];
  return GetOffsetInFile(key, eof_data_offset, stream_index);
}

int SimpleEntryStat::GetFileSize(const std::string& key, int file_index) const {
  const int total_data_size =
      file_index == 0 ? data_size_[0] + data_size_[1] + sizeof(SimpleFileEOF)
                      : data_size_[2];
  return GetFileSizeFromKeyAndDataSize(key, total_data_size);
}

SimpleEntryCreationResults::SimpleEntryCreationResults(
    SimpleEntryStat entry_stat)
    : sync_entry(NULL),
      entry_stat(entry_stat),
      stream_0_crc32(crc32(0, Z_NULL, 0)),
      result(net::OK) {
}

SimpleEntryCreationResults::~SimpleEntryCreationResults() {
}

SimpleSynchronousEntry::CRCRecord::CRCRecord() : index(-1),
                                                 has_crc32(false),
                                                 data_crc32(0) {
}

SimpleSynchronousEntry::CRCRecord::CRCRecord(int index_p,
                                             bool has_crc32_p,
                                             uint32 data_crc32_p)
    : index(index_p),
      has_crc32(has_crc32_p),
      data_crc32(data_crc32_p) {}

SimpleSynchronousEntry::EntryOperationData::EntryOperationData(int index_p,
                                                               int offset_p,
                                                               int buf_len_p)
    : index(index_p),
      offset(offset_p),
      buf_len(buf_len_p) {}

SimpleSynchronousEntry::EntryOperationData::EntryOperationData(int index_p,
                                                               int offset_p,
                                                               int buf_len_p,
                                                               bool truncate_p,
                                                               bool doomed_p)
    : index(index_p),
      offset(offset_p),
      buf_len(buf_len_p),
      truncate(truncate_p),
      doomed(doomed_p) {}

SimpleSynchronousEntry::EntryOperationData::EntryOperationData(
    int64 sparse_offset_p,
    int buf_len_p)
    : sparse_offset(sparse_offset_p),
      buf_len(buf_len_p) {}

// static
void SimpleSynchronousEntry::OpenEntry(
    net::CacheType cache_type,
    const FilePath& path,
    const uint64 entry_hash,
    bool had_index,
    SimpleEntryCreationResults *out_results) {
  SimpleSynchronousEntry* sync_entry =
      new SimpleSynchronousEntry(cache_type, path, "", entry_hash);
  out_results->result =
      sync_entry->InitializeForOpen(had_index,
                                    &out_results->entry_stat,
                                    &out_results->stream_0_data,
                                    &out_results->stream_0_crc32);
  if (out_results->result != net::OK) {
    sync_entry->Doom();
    delete sync_entry;
    out_results->sync_entry = NULL;
    out_results->stream_0_data = NULL;
    return;
  }
  out_results->sync_entry = sync_entry;
}

// static
void SimpleSynchronousEntry::CreateEntry(
    net::CacheType cache_type,
    const FilePath& path,
    const std::string& key,
    const uint64 entry_hash,
    bool had_index,
    SimpleEntryCreationResults *out_results) {
  DCHECK_EQ(entry_hash, GetEntryHashKey(key));
  SimpleSynchronousEntry* sync_entry =
      new SimpleSynchronousEntry(cache_type, path, key, entry_hash);
  out_results->result = sync_entry->InitializeForCreate(
      had_index, &out_results->entry_stat);
  if (out_results->result != net::OK) {
    if (out_results->result != net::ERR_FILE_EXISTS)
      sync_entry->Doom();
    delete sync_entry;
    out_results->sync_entry = NULL;
    return;
  }
  out_results->sync_entry = sync_entry;
}

// static
int SimpleSynchronousEntry::DoomEntry(
    const FilePath& path,
    uint64 entry_hash) {
  const bool deleted_well = DeleteFilesForEntryHash(path, entry_hash);
  return deleted_well ? net::OK : net::ERR_FAILED;
}

// static
int SimpleSynchronousEntry::DoomEntrySet(
    const std::vector<uint64>* key_hashes,
    const FilePath& path) {
  const size_t did_delete_count = std::count_if(
      key_hashes->begin(), key_hashes->end(), std::bind1st(
          std::ptr_fun(SimpleSynchronousEntry::DeleteFilesForEntryHash), path));
  return (did_delete_count == key_hashes->size()) ? net::OK : net::ERR_FAILED;
}

void SimpleSynchronousEntry::ReadData(const EntryOperationData& in_entry_op,
                                      net::IOBuffer* out_buf,
                                      uint32* out_crc32,
                                      SimpleEntryStat* entry_stat,
                                      int* out_result) const {
  DCHECK(initialized_);
  DCHECK_NE(0, in_entry_op.index);
  const int64 file_offset =
      entry_stat->GetOffsetInFile(key_, in_entry_op.offset, in_entry_op.index);
  int file_index = GetFileIndexFromStreamIndex(in_entry_op.index);
  // Zero-length reads and reads to the empty streams of omitted files should
  // be handled in the SimpleEntryImpl.
  DCHECK_LT(0, in_entry_op.buf_len);
  DCHECK(!empty_file_omitted_[file_index]);
  File* file = const_cast<File*>(&files_[file_index]);
  int bytes_read =
      file->Read(file_offset, out_buf->data(), in_entry_op.buf_len);
  if (bytes_read > 0) {
    entry_stat->set_last_used(Time::Now());
    *out_crc32 = crc32(crc32(0L, Z_NULL, 0),
                       reinterpret_cast<const Bytef*>(out_buf->data()),
                       bytes_read);
  }
  if (bytes_read >= 0) {
    *out_result = bytes_read;
  } else {
    *out_result = net::ERR_CACHE_READ_FAILURE;
    Doom();
  }
}

void SimpleSynchronousEntry::WriteData(const EntryOperationData& in_entry_op,
                                       net::IOBuffer* in_buf,
                                       SimpleEntryStat* out_entry_stat,
                                       int* out_result) {
  DCHECK(initialized_);
  DCHECK_NE(0, in_entry_op.index);
  int index = in_entry_op.index;
  int file_index = GetFileIndexFromStreamIndex(index);
  int offset = in_entry_op.offset;
  int buf_len = in_entry_op.buf_len;
  bool truncate = in_entry_op.truncate;
  bool doomed = in_entry_op.doomed;
  const int64 file_offset = out_entry_stat->GetOffsetInFile(
      key_, in_entry_op.offset, in_entry_op.index);
  bool extending_by_write = offset + buf_len > out_entry_stat->data_size(index);

  if (empty_file_omitted_[file_index]) {
    // Don't create a new file if the entry has been doomed, to avoid it being
    // mixed up with a newly-created entry with the same key.
    if (doomed) {
      DLOG(WARNING) << "Rejecting write to lazily omitted stream "
                    << in_entry_op.index << " of doomed cache entry.";
      RecordWriteResult(cache_type_, WRITE_RESULT_LAZY_STREAM_ENTRY_DOOMED);
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    File::Error error;
    if (!MaybeCreateFile(file_index, FILE_REQUIRED, &error)) {
      RecordWriteResult(cache_type_, WRITE_RESULT_LAZY_CREATE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    CreateEntryResult result;
    if (!InitializeCreatedFile(file_index, &result)) {
      RecordWriteResult(cache_type_, WRITE_RESULT_LAZY_INITIALIZE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  DCHECK(!empty_file_omitted_[file_index]);

  if (extending_by_write) {
    // The EOF record and the eventual stream afterward need to be zeroed out.
    const int64 file_eof_offset =
        out_entry_stat->GetEOFOffsetInFile(key_, index);
    if (!files_[file_index].SetLength(file_eof_offset)) {
      RecordWriteResult(cache_type_, WRITE_RESULT_PRETRUNCATE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  if (buf_len > 0) {
    if (files_[file_index].Write(file_offset, in_buf->data(), buf_len) !=
        buf_len) {
      RecordWriteResult(cache_type_, WRITE_RESULT_WRITE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }
  if (!truncate && (buf_len > 0 || !extending_by_write)) {
    out_entry_stat->set_data_size(
        index, std::max(out_entry_stat->data_size(index), offset + buf_len));
  } else {
    out_entry_stat->set_data_size(index, offset + buf_len);
    int file_eof_offset = out_entry_stat->GetLastEOFOffsetInFile(key_, index);
    if (!files_[file_index].SetLength(file_eof_offset)) {
      RecordWriteResult(cache_type_, WRITE_RESULT_TRUNCATE_FAILURE);
      Doom();
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
  }

  RecordWriteResult(cache_type_, WRITE_RESULT_SUCCESS);
  base::Time modification_time = Time::Now();
  out_entry_stat->set_last_used(modification_time);
  out_entry_stat->set_last_modified(modification_time);
  *out_result = buf_len;
}

void SimpleSynchronousEntry::ReadSparseData(
    const EntryOperationData& in_entry_op,
    net::IOBuffer* out_buf,
    base::Time* out_last_used,
    int* out_result) {
  DCHECK(initialized_);
  int64 offset = in_entry_op.sparse_offset;
  int buf_len = in_entry_op.buf_len;

  char* buf = out_buf->data();
  int read_so_far = 0;

  // Find the first sparse range at or after the requested offset.
  SparseRangeIterator it = sparse_ranges_.lower_bound(offset);

  if (it != sparse_ranges_.begin()) {
    // Hop back one range and read the one overlapping with the start.
    --it;
    SparseRange* found_range = &it->second;
    DCHECK_EQ(it->first, found_range->offset);
    if (found_range->offset + found_range->length > offset) {
      DCHECK_LE(0, found_range->length);
      DCHECK_GE(kint32max, found_range->length);
      DCHECK_LE(0, offset - found_range->offset);
      DCHECK_GE(kint32max, offset - found_range->offset);
      int range_len_after_offset = found_range->length -
                                   (offset - found_range->offset);
      DCHECK_LE(0, range_len_after_offset);

      int len_to_read = std::min(buf_len, range_len_after_offset);
      if (!ReadSparseRange(found_range,
                           offset - found_range->offset,
                           len_to_read,
                           buf)) {
        *out_result = net::ERR_CACHE_READ_FAILURE;
        return;
      }
      read_so_far += len_to_read;
    }
    ++it;
  }

  // Keep reading until the buffer is full or there is not another contiguous
  // range.
  while (read_so_far < buf_len &&
         it != sparse_ranges_.end() &&
         it->second.offset == offset + read_so_far) {
    SparseRange* found_range = &it->second;
    DCHECK_EQ(it->first, found_range->offset);
    int range_len = (found_range->length > kint32max) ?
                    kint32max : found_range->length;
    int len_to_read = std::min(buf_len - read_so_far, range_len);
    if (!ReadSparseRange(found_range, 0, len_to_read, buf + read_so_far)) {
      *out_result = net::ERR_CACHE_READ_FAILURE;
      return;
    }
    read_so_far += len_to_read;
    ++it;
  }

  *out_result = read_so_far;
}

void SimpleSynchronousEntry::WriteSparseData(
    const EntryOperationData& in_entry_op,
    net::IOBuffer* in_buf,
    int64 max_sparse_data_size,
    SimpleEntryStat* out_entry_stat,
    int* out_result) {
  DCHECK(initialized_);
  int64 offset = in_entry_op.sparse_offset;
  int buf_len = in_entry_op.buf_len;

  const char* buf = in_buf->data();
  int written_so_far = 0;
  int appended_so_far = 0;

  if (!sparse_file_open() && !CreateSparseFile()) {
    *out_result = net::ERR_CACHE_WRITE_FAILURE;
    return;
  }

  int64 sparse_data_size = out_entry_stat->sparse_data_size();
  // This is a pessimistic estimate; it assumes the entire buffer is going to
  // be appended as a new range, not written over existing ranges.
  if (sparse_data_size + buf_len > max_sparse_data_size) {
    DVLOG(1) << "Truncating sparse data file (" << sparse_data_size << " + "
             << buf_len << " > " << max_sparse_data_size << ")";
    TruncateSparseFile();
  }

  SparseRangeIterator it = sparse_ranges_.lower_bound(offset);

  if (it != sparse_ranges_.begin()) {
    --it;
    SparseRange* found_range = &it->second;
    if (found_range->offset + found_range->length > offset) {
      DCHECK_LE(0, found_range->length);
      DCHECK_GE(kint32max, found_range->length);
      DCHECK_LE(0, offset - found_range->offset);
      DCHECK_GE(kint32max, offset - found_range->offset);
      int range_len_after_offset = found_range->length -
                                   (offset - found_range->offset);
      DCHECK_LE(0, range_len_after_offset);

      int len_to_write = std::min(buf_len, range_len_after_offset);
      if (!WriteSparseRange(found_range,
                            offset - found_range->offset,
                            len_to_write,
                            buf)) {
        *out_result = net::ERR_CACHE_WRITE_FAILURE;
        return;
      }
      written_so_far += len_to_write;
    }
    ++it;
  }

  while (written_so_far < buf_len &&
         it != sparse_ranges_.end() &&
         it->second.offset < offset + buf_len) {
    SparseRange* found_range = &it->second;
    if (offset + written_so_far < found_range->offset) {
      int len_to_append = found_range->offset - (offset + written_so_far);
      if (!AppendSparseRange(offset + written_so_far,
                             len_to_append,
                             buf + written_so_far)) {
        *out_result = net::ERR_CACHE_WRITE_FAILURE;
        return;
      }
      written_so_far += len_to_append;
      appended_so_far += len_to_append;
    }
    int range_len = (found_range->length > kint32max) ?
                    kint32max : found_range->length;
    int len_to_write = std::min(buf_len - written_so_far, range_len);
    if (!WriteSparseRange(found_range,
                          0,
                          len_to_write,
                          buf + written_so_far)) {
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    written_so_far += len_to_write;
    ++it;
  }

  if (written_so_far < buf_len) {
    int len_to_append = buf_len - written_so_far;
    if (!AppendSparseRange(offset + written_so_far,
                           len_to_append,
                           buf + written_so_far)) {
      *out_result = net::ERR_CACHE_WRITE_FAILURE;
      return;
    }
    written_so_far += len_to_append;
    appended_so_far += len_to_append;
  }

  DCHECK_EQ(buf_len, written_so_far);

  base::Time modification_time = Time::Now();
  out_entry_stat->set_last_used(modification_time);
  out_entry_stat->set_last_modified(modification_time);
  int32 old_sparse_data_size = out_entry_stat->sparse_data_size();
  out_entry_stat->set_sparse_data_size(old_sparse_data_size + appended_so_far);
  *out_result = written_so_far;
}

void SimpleSynchronousEntry::GetAvailableRange(
    const EntryOperationData& in_entry_op,
    int64* out_start,
    int* out_result) {
  DCHECK(initialized_);
  int64 offset = in_entry_op.sparse_offset;
  int len = in_entry_op.buf_len;

  SparseRangeIterator it = sparse_ranges_.lower_bound(offset);

  int64 start = offset;
  int avail_so_far = 0;

  if (it != sparse_ranges_.end() && it->second.offset < offset + len)
    start = it->second.offset;

  if ((it == sparse_ranges_.end() || it->second.offset > offset) &&
      it != sparse_ranges_.begin()) {
    --it;
    if (it->second.offset + it->second.length > offset) {
      start = offset;
      avail_so_far = (it->second.offset + it->second.length) - offset;
    }
    ++it;
  }

  while (start + avail_so_far < offset + len &&
         it != sparse_ranges_.end() &&
         it->second.offset == start + avail_so_far) {
    avail_so_far += it->second.length;
    ++it;
  }

  int len_from_start = len - (start - offset);
  *out_start = start;
  *out_result = std::min(avail_so_far, len_from_start);
}

void SimpleSynchronousEntry::CheckEOFRecord(int index,
                                            const SimpleEntryStat& entry_stat,
                                            uint32 expected_crc32,
                                            int* out_result) const {
  DCHECK(initialized_);
  uint32 crc32;
  bool has_crc32;
  int stream_size;
  *out_result =
      GetEOFRecordData(index, entry_stat, &has_crc32, &crc32, &stream_size);
  if (*out_result != net::OK) {
    Doom();
    return;
  }
  if (has_crc32 && crc32 != expected_crc32) {
    DVLOG(1) << "EOF record had bad crc.";
    *out_result = net::ERR_CACHE_CHECKSUM_MISMATCH;
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_CRC_MISMATCH);
    Doom();
    return;
  }
  RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_SUCCESS);
}

void SimpleSynchronousEntry::Close(
    const SimpleEntryStat& entry_stat,
    scoped_ptr<std::vector<CRCRecord> > crc32s_to_write,
    net::GrowableIOBuffer* stream_0_data) {
  DCHECK(stream_0_data);
  // Write stream 0 data.
  int stream_0_offset = entry_stat.GetOffsetInFile(key_, 0, 0);
  if (files_[0].Write(stream_0_offset, stream_0_data->data(),
                      entry_stat.data_size(0)) !=
      entry_stat.data_size(0)) {
    RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
    DVLOG(1) << "Could not write stream 0 data.";
    Doom();
  }

  for (std::vector<CRCRecord>::const_iterator it = crc32s_to_write->begin();
       it != crc32s_to_write->end(); ++it) {
    const int stream_index = it->index;
    const int file_index = GetFileIndexFromStreamIndex(stream_index);
    if (empty_file_omitted_[file_index])
      continue;

    SimpleFileEOF eof_record;
    eof_record.stream_size = entry_stat.data_size(stream_index);
    eof_record.final_magic_number = kSimpleFinalMagicNumber;
    eof_record.flags = 0;
    if (it->has_crc32)
      eof_record.flags |= SimpleFileEOF::FLAG_HAS_CRC32;
    eof_record.data_crc32 = it->data_crc32;
    int eof_offset = entry_stat.GetEOFOffsetInFile(key_, stream_index);
    // If stream 0 changed size, the file needs to be resized, otherwise the
    // next open will yield wrong stream sizes. On stream 1 and stream 2 proper
    // resizing of the file is handled in SimpleSynchronousEntry::WriteData().
    if (stream_index == 0 &&
        !files_[file_index].SetLength(eof_offset)) {
      RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
      DVLOG(1) << "Could not truncate stream 0 file.";
      Doom();
      break;
    }
    if (files_[file_index].Write(eof_offset,
                                 reinterpret_cast<const char*>(&eof_record),
                                 sizeof(eof_record)) !=
        sizeof(eof_record)) {
      RecordCloseResult(cache_type_, CLOSE_RESULT_WRITE_FAILURE);
      DVLOG(1) << "Could not write eof record.";
      Doom();
      break;
    }
  }
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    if (empty_file_omitted_[i])
      continue;

    files_[i].Close();
    const int64 file_size = entry_stat.GetFileSize(key_, i);
    SIMPLE_CACHE_UMA(CUSTOM_COUNTS,
                     "LastClusterSize", cache_type_,
                     file_size % 4096, 0, 4097, 50);
    const int64 cluster_loss = file_size % 4096 ? 4096 - file_size % 4096 : 0;
    SIMPLE_CACHE_UMA(PERCENTAGE,
                     "LastClusterLossPercent", cache_type_,
                     cluster_loss * 100 / (cluster_loss + file_size));
  }

  if (sparse_file_open())
    sparse_file_.Close();

  if (files_created_) {
    const int stream2_file_index = GetFileIndexFromStreamIndex(2);
    SIMPLE_CACHE_UMA(BOOLEAN, "EntryCreatedAndStream2Omitted", cache_type_,
                     empty_file_omitted_[stream2_file_index]);
  }
  RecordCloseResult(cache_type_, CLOSE_RESULT_SUCCESS);
  have_open_files_ = false;
  delete this;
}

SimpleSynchronousEntry::SimpleSynchronousEntry(net::CacheType cache_type,
                                               const FilePath& path,
                                               const std::string& key,
                                               const uint64 entry_hash)
    : cache_type_(cache_type),
      path_(path),
      entry_hash_(entry_hash),
      key_(key),
      have_open_files_(false),
      initialized_(false) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i)
    empty_file_omitted_[i] = false;
}

SimpleSynchronousEntry::~SimpleSynchronousEntry() {
  DCHECK(!(have_open_files_ && initialized_));
  if (have_open_files_)
    CloseFiles();
}

bool SimpleSynchronousEntry::MaybeOpenFile(
    int file_index,
    File::Error* out_error) {
  DCHECK(out_error);

  FilePath filename = GetFilenameFromFileIndex(file_index);
  int flags = File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE;
  files_[file_index].Initialize(filename, flags);
  *out_error = files_[file_index].error_details();

  if (CanOmitEmptyFile(file_index) && !files_[file_index].IsValid() &&
      *out_error == File::FILE_ERROR_NOT_FOUND) {
    empty_file_omitted_[file_index] = true;
    return true;
  }

  return files_[file_index].IsValid();
}

bool SimpleSynchronousEntry::MaybeCreateFile(
    int file_index,
    FileRequired file_required,
    File::Error* out_error) {
  DCHECK(out_error);

  if (CanOmitEmptyFile(file_index) && file_required == FILE_NOT_REQUIRED) {
    empty_file_omitted_[file_index] = true;
    return true;
  }

  FilePath filename = GetFilenameFromFileIndex(file_index);
  int flags = File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE;
  files_[file_index].Initialize(filename, flags);
  *out_error = files_[file_index].error_details();

  empty_file_omitted_[file_index] = false;

  return files_[file_index].IsValid();
}

bool SimpleSynchronousEntry::OpenFiles(
    bool had_index,
    SimpleEntryStat* out_entry_stat) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    File::Error error;
    if (!MaybeOpenFile(i, &error)) {
      // TODO(ttuttle,gavinp): Remove one each of these triplets of histograms.
      // We can calculate the third as the sum or difference of the other two.
      RecordSyncOpenResult(
          cache_type_, OPEN_ENTRY_PLATFORM_FILE_ERROR, had_index);
      SIMPLE_CACHE_UMA(ENUMERATION,
                       "SyncOpenPlatformFileError", cache_type_,
                       -error, -base::File::FILE_ERROR_MAX);
      if (had_index) {
        SIMPLE_CACHE_UMA(ENUMERATION,
                         "SyncOpenPlatformFileError_WithIndex", cache_type_,
                         -error, -base::File::FILE_ERROR_MAX);
      } else {
        SIMPLE_CACHE_UMA(ENUMERATION,
                         "SyncOpenPlatformFileError_WithoutIndex",
                         cache_type_,
                         -error, -base::File::FILE_ERROR_MAX);
      }
      while (--i >= 0)
        CloseFile(i);
      return false;
    }
  }

  have_open_files_ = true;

  base::TimeDelta entry_age = base::Time::Now() - base::Time::UnixEpoch();
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    if (empty_file_omitted_[i]) {
      out_entry_stat->set_data_size(i + 1, 0);
      continue;
    }

    File::Info file_info;
    bool success = files_[i].GetInfo(&file_info);
    base::Time file_last_modified;
    if (!success) {
      DLOG(WARNING) << "Could not get platform file info.";
      continue;
    }
    out_entry_stat->set_last_used(file_info.last_accessed);
    if (simple_util::GetMTime(path_, &file_last_modified))
      out_entry_stat->set_last_modified(file_last_modified);
    else
      out_entry_stat->set_last_modified(file_info.last_modified);

    base::TimeDelta stream_age =
        base::Time::Now() - out_entry_stat->last_modified();
    if (stream_age < entry_age)
      entry_age = stream_age;

    // Two things prevent from knowing the right values for |data_size|:
    // 1) The key is not known, hence its length is unknown.
    // 2) Stream 0 and stream 1 are in the same file, and the exact size for
    // each will only be known when reading the EOF record for stream 0.
    //
    // The size for file 0 and 1 is temporarily kept in
    // |data_size(1)| and |data_size(2)| respectively. Reading the key in
    // InitializeForOpen yields the data size for each file. In the case of
    // file hash_1, this is the total size of stream 2, and is assigned to
    // data_size(2). In the case of file 0, it is the combined size of stream
    // 0, stream 1 and one EOF record. The exact distribution of sizes between
    // stream 1 and stream 0 is only determined after reading the EOF record
    // for stream 0 in ReadAndValidateStream0.
    out_entry_stat->set_data_size(i + 1, file_info.size);
  }
  SIMPLE_CACHE_UMA(CUSTOM_COUNTS,
                   "SyncOpenEntryAge", cache_type_,
                   entry_age.InHours(), 1, 1000, 50);

  files_created_ = false;

  return true;
}

bool SimpleSynchronousEntry::CreateFiles(
    bool had_index,
    SimpleEntryStat* out_entry_stat) {
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    File::Error error;
    if (!MaybeCreateFile(i, FILE_NOT_REQUIRED, &error)) {
      // TODO(ttuttle,gavinp): Remove one each of these triplets of histograms.
      // We can calculate the third as the sum or difference of the other two.
      RecordSyncCreateResult(CREATE_ENTRY_PLATFORM_FILE_ERROR, had_index);
      SIMPLE_CACHE_UMA(ENUMERATION,
                       "SyncCreatePlatformFileError", cache_type_,
                       -error, -base::File::FILE_ERROR_MAX);
      if (had_index) {
        SIMPLE_CACHE_UMA(ENUMERATION,
                         "SyncCreatePlatformFileError_WithIndex", cache_type_,
                         -error, -base::File::FILE_ERROR_MAX);
      } else {
        SIMPLE_CACHE_UMA(ENUMERATION,
                         "SyncCreatePlatformFileError_WithoutIndex",
                         cache_type_,
                         -error, -base::File::FILE_ERROR_MAX);
      }
      while (--i >= 0)
        CloseFile(i);
      return false;
    }
  }

  have_open_files_ = true;

  base::Time creation_time = Time::Now();
  out_entry_stat->set_last_modified(creation_time);
  out_entry_stat->set_last_used(creation_time);
  for (int i = 0; i < kSimpleEntryStreamCount; ++i)
      out_entry_stat->set_data_size(i, 0);

  files_created_ = true;

  return true;
}

void SimpleSynchronousEntry::CloseFile(int index) {
  if (empty_file_omitted_[index]) {
    empty_file_omitted_[index] = false;
  } else {
    DCHECK(files_[index].IsValid());
    files_[index].Close();
  }

  if (sparse_file_open())
    CloseSparseFile();
}

void SimpleSynchronousEntry::CloseFiles() {
  for (int i = 0; i < kSimpleEntryFileCount; ++i)
    CloseFile(i);
}

int SimpleSynchronousEntry::InitializeForOpen(
    bool had_index,
    SimpleEntryStat* out_entry_stat,
    scoped_refptr<net::GrowableIOBuffer>* stream_0_data,
    uint32* out_stream_0_crc32) {
  DCHECK(!initialized_);
  if (!OpenFiles(had_index, out_entry_stat)) {
    DLOG(WARNING) << "Could not open platform files for entry.";
    return net::ERR_FAILED;
  }
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    if (empty_file_omitted_[i])
      continue;

    SimpleFileHeader header;
    int header_read_result =
        files_[i].Read(0, reinterpret_cast<char*>(&header), sizeof(header));
    if (header_read_result != sizeof(header)) {
      DLOG(WARNING) << "Cannot read header from entry.";
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_CANT_READ_HEADER, had_index);
      return net::ERR_FAILED;
    }

    if (header.initial_magic_number != kSimpleInitialMagicNumber) {
      // TODO(gavinp): This seems very bad; for now we log at WARNING, but we
      // should give consideration to not saturating the log with these if that
      // becomes a problem.
      DLOG(WARNING) << "Magic number did not match.";
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_BAD_MAGIC_NUMBER, had_index);
      return net::ERR_FAILED;
    }

    if (header.version != kSimpleEntryVersionOnDisk) {
      DLOG(WARNING) << "Unreadable version.";
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_BAD_VERSION, had_index);
      return net::ERR_FAILED;
    }

    scoped_ptr<char[]> key(new char[header.key_length]);
    int key_read_result = files_[i].Read(sizeof(header), key.get(),
                                         header.key_length);
    if (key_read_result != implicit_cast<int>(header.key_length)) {
      DLOG(WARNING) << "Cannot read key from entry.";
      RecordSyncOpenResult(cache_type_, OPEN_ENTRY_CANT_READ_KEY, had_index);
      return net::ERR_FAILED;
    }

    key_ = std::string(key.get(), header.key_length);
    if (i == 0) {
      // File size for stream 0 has been stored temporarily in data_size[1].
      int total_data_size =
          GetDataSizeFromKeyAndFileSize(key_, out_entry_stat->data_size(1));
      int ret_value_stream_0 = ReadAndValidateStream0(
          total_data_size, out_entry_stat, stream_0_data, out_stream_0_crc32);
      if (ret_value_stream_0 != net::OK)
        return ret_value_stream_0;
    } else {
      out_entry_stat->set_data_size(
          2, GetDataSizeFromKeyAndFileSize(key_, out_entry_stat->data_size(2)));
      if (out_entry_stat->data_size(2) < 0) {
        DLOG(WARNING) << "Stream 2 file is too small.";
        return net::ERR_FAILED;
      }
    }

    if (base::Hash(key.get(), header.key_length) != header.key_hash) {
      DLOG(WARNING) << "Hash mismatch on key.";
      RecordSyncOpenResult(
          cache_type_, OPEN_ENTRY_KEY_HASH_MISMATCH, had_index);
      return net::ERR_FAILED;
    }
  }

  int32 sparse_data_size = 0;
  if (!OpenSparseFileIfExists(&sparse_data_size)) {
    RecordSyncOpenResult(
        cache_type_, OPEN_ENTRY_SPARSE_OPEN_FAILED, had_index);
    return net::ERR_FAILED;
  }
  out_entry_stat->set_sparse_data_size(sparse_data_size);

  bool removed_stream2 = false;
  const int stream2_file_index = GetFileIndexFromStreamIndex(2);
  DCHECK(CanOmitEmptyFile(stream2_file_index));
  if (!empty_file_omitted_[stream2_file_index] &&
      out_entry_stat->data_size(2) == 0) {
    DVLOG(1) << "Removing empty stream 2 file.";
    CloseFile(stream2_file_index);
    DeleteFileForEntryHash(path_, entry_hash_, stream2_file_index);
    empty_file_omitted_[stream2_file_index] = true;
    removed_stream2 = true;
  }

  SIMPLE_CACHE_UMA(BOOLEAN, "EntryOpenedAndStream2Removed", cache_type_,
                   removed_stream2);

  RecordSyncOpenResult(cache_type_, OPEN_ENTRY_SUCCESS, had_index);
  initialized_ = true;
  return net::OK;
}

bool SimpleSynchronousEntry::InitializeCreatedFile(
    int file_index,
    CreateEntryResult* out_result) {
  SimpleFileHeader header;
  header.initial_magic_number = kSimpleInitialMagicNumber;
  header.version = kSimpleEntryVersionOnDisk;

  header.key_length = key_.size();
  header.key_hash = base::Hash(key_);

  int bytes_written = files_[file_index].Write(
      0, reinterpret_cast<char*>(&header), sizeof(header));
  if (bytes_written != sizeof(header)) {
    *out_result = CREATE_ENTRY_CANT_WRITE_HEADER;
    return false;
  }

  bytes_written = files_[file_index].Write(sizeof(header), key_.data(),
                                           key_.size());
  if (bytes_written != implicit_cast<int>(key_.size())) {
    *out_result = CREATE_ENTRY_CANT_WRITE_KEY;
    return false;
  }

  return true;
}

int SimpleSynchronousEntry::InitializeForCreate(
    bool had_index,
    SimpleEntryStat* out_entry_stat) {
  DCHECK(!initialized_);
  if (!CreateFiles(had_index, out_entry_stat)) {
    DLOG(WARNING) << "Could not create platform files.";
    return net::ERR_FILE_EXISTS;
  }
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    if (empty_file_omitted_[i])
      continue;

    CreateEntryResult result;
    if (!InitializeCreatedFile(i, &result)) {
      RecordSyncCreateResult(result, had_index);
      return net::ERR_FAILED;
    }
  }
  RecordSyncCreateResult(CREATE_ENTRY_SUCCESS, had_index);
  initialized_ = true;
  return net::OK;
}

int SimpleSynchronousEntry::ReadAndValidateStream0(
    int total_data_size,
    SimpleEntryStat* out_entry_stat,
    scoped_refptr<net::GrowableIOBuffer>* stream_0_data,
    uint32* out_stream_0_crc32) const {
  // Temporarily assign all the data size to stream 1 in order to read the
  // EOF record for stream 0, which contains the size of stream 0.
  out_entry_stat->set_data_size(0, 0);
  out_entry_stat->set_data_size(1, total_data_size - sizeof(SimpleFileEOF));

  bool has_crc32;
  uint32 read_crc32;
  int stream_0_size;
  int ret_value_crc32 = GetEOFRecordData(
      0, *out_entry_stat, &has_crc32, &read_crc32, &stream_0_size);
  if (ret_value_crc32 != net::OK)
    return ret_value_crc32;

  if (stream_0_size > out_entry_stat->data_size(1))
    return net::ERR_FAILED;

  // These are the real values of data size.
  out_entry_stat->set_data_size(0, stream_0_size);
  out_entry_stat->set_data_size(
      1, out_entry_stat->data_size(1) - stream_0_size);

  // Put stream 0 data in memory.
  *stream_0_data = new net::GrowableIOBuffer();
  (*stream_0_data)->SetCapacity(stream_0_size);
  int file_offset = out_entry_stat->GetOffsetInFile(key_, 0, 0);
  File* file = const_cast<File*>(&files_[0]);
  int bytes_read =
      file->Read(file_offset, (*stream_0_data)->data(), stream_0_size);
  if (bytes_read != stream_0_size)
    return net::ERR_FAILED;

  // Check the CRC32.
  uint32 expected_crc32 =
      stream_0_size == 0
          ? crc32(0, Z_NULL, 0)
          : crc32(crc32(0, Z_NULL, 0),
                  reinterpret_cast<const Bytef*>((*stream_0_data)->data()),
                  stream_0_size);
  if (has_crc32 && read_crc32 != expected_crc32) {
    DVLOG(1) << "EOF record had bad crc.";
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_CRC_MISMATCH);
    return net::ERR_FAILED;
  }
  *out_stream_0_crc32 = expected_crc32;
  RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_SUCCESS);
  return net::OK;
}

int SimpleSynchronousEntry::GetEOFRecordData(int index,
                                             const SimpleEntryStat& entry_stat,
                                             bool* out_has_crc32,
                                             uint32* out_crc32,
                                             int* out_data_size) const {
  SimpleFileEOF eof_record;
  int file_offset = entry_stat.GetEOFOffsetInFile(key_, index);
  int file_index = GetFileIndexFromStreamIndex(index);
  File* file = const_cast<File*>(&files_[file_index]);
  if (file->Read(file_offset, reinterpret_cast<char*>(&eof_record),
                 sizeof(eof_record)) !=
      sizeof(eof_record)) {
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_READ_FAILURE);
    return net::ERR_CACHE_CHECKSUM_READ_FAILURE;
  }

  if (eof_record.final_magic_number != kSimpleFinalMagicNumber) {
    RecordCheckEOFResult(cache_type_, CHECK_EOF_RESULT_MAGIC_NUMBER_MISMATCH);
    DVLOG(1) << "EOF record had bad magic number.";
    return net::ERR_CACHE_CHECKSUM_READ_FAILURE;
  }

  *out_has_crc32 = (eof_record.flags & SimpleFileEOF::FLAG_HAS_CRC32) ==
                   SimpleFileEOF::FLAG_HAS_CRC32;
  *out_crc32 = eof_record.data_crc32;
  *out_data_size = eof_record.stream_size;
  SIMPLE_CACHE_UMA(BOOLEAN, "SyncCheckEOFHasCrc", cache_type_, *out_has_crc32);
  return net::OK;
}

void SimpleSynchronousEntry::Doom() const {
  DeleteFilesForEntryHash(path_, entry_hash_);
}

// static
bool SimpleSynchronousEntry::DeleteFileForEntryHash(
    const FilePath& path,
    const uint64 entry_hash,
    const int file_index) {
  FilePath to_delete = path.AppendASCII(
      GetFilenameFromEntryHashAndFileIndex(entry_hash, file_index));
  return base::DeleteFile(to_delete, false);
}

// static
bool SimpleSynchronousEntry::DeleteFilesForEntryHash(
    const FilePath& path,
    const uint64 entry_hash) {
  bool result = true;
  for (int i = 0; i < kSimpleEntryFileCount; ++i) {
    if (!DeleteFileForEntryHash(path, entry_hash, i) && !CanOmitEmptyFile(i))
      result = false;
  }
  FilePath to_delete = path.AppendASCII(
      GetSparseFilenameFromEntryHash(entry_hash));
  base::DeleteFile(to_delete, false);
  return result;
}

void SimpleSynchronousEntry::RecordSyncCreateResult(CreateEntryResult result,
                                                    bool had_index) {
  DCHECK_GT(CREATE_ENTRY_MAX, result);
  SIMPLE_CACHE_UMA(ENUMERATION,
                   "SyncCreateResult", cache_type_, result, CREATE_ENTRY_MAX);
  if (had_index) {
    SIMPLE_CACHE_UMA(ENUMERATION,
                     "SyncCreateResult_WithIndex", cache_type_,
                     result, CREATE_ENTRY_MAX);
  } else {
    SIMPLE_CACHE_UMA(ENUMERATION,
                     "SyncCreateResult_WithoutIndex", cache_type_,
                     result, CREATE_ENTRY_MAX);
  }
}

FilePath SimpleSynchronousEntry::GetFilenameFromFileIndex(int file_index) {
  return path_.AppendASCII(
      GetFilenameFromEntryHashAndFileIndex(entry_hash_, file_index));
}

bool SimpleSynchronousEntry::OpenSparseFileIfExists(
    int32* out_sparse_data_size) {
  DCHECK(!sparse_file_open());

  FilePath filename = path_.AppendASCII(
      GetSparseFilenameFromEntryHash(entry_hash_));
  int flags = File::FLAG_OPEN | File::FLAG_READ | File::FLAG_WRITE;
  sparse_file_.Initialize(filename, flags);
  if (sparse_file_.IsValid())
    return ScanSparseFile(out_sparse_data_size);

  return sparse_file_.error_details() == File::FILE_ERROR_NOT_FOUND;
}

bool SimpleSynchronousEntry::CreateSparseFile() {
  DCHECK(!sparse_file_open());

  FilePath filename = path_.AppendASCII(
      GetSparseFilenameFromEntryHash(entry_hash_));
  int flags = File::FLAG_CREATE | File::FLAG_READ | File::FLAG_WRITE;
  sparse_file_.Initialize(filename, flags);
  if (!sparse_file_.IsValid())
    return false;

  return InitializeSparseFile();
}

void SimpleSynchronousEntry::CloseSparseFile() {
  DCHECK(sparse_file_open());
  sparse_file_.Close();
}

bool SimpleSynchronousEntry::TruncateSparseFile() {
  DCHECK(sparse_file_open());

  int64 header_and_key_length = sizeof(SimpleFileHeader) + key_.size();
  if (!sparse_file_.SetLength(header_and_key_length)) {
    DLOG(WARNING) << "Could not truncate sparse file";
    return false;
  }

  sparse_ranges_.clear();

  return true;
}

bool SimpleSynchronousEntry::InitializeSparseFile() {
  DCHECK(sparse_file_open());

  SimpleFileHeader header;
  header.initial_magic_number = kSimpleInitialMagicNumber;
  header.version = kSimpleVersion;
  header.key_length = key_.size();
  header.key_hash = base::Hash(key_);

  int header_write_result =
      sparse_file_.Write(0, reinterpret_cast<char*>(&header), sizeof(header));
  if (header_write_result != sizeof(header)) {
    DLOG(WARNING) << "Could not write sparse file header";
    return false;
  }

  int key_write_result = sparse_file_.Write(sizeof(header), key_.data(),
                                            key_.size());
  if (key_write_result != implicit_cast<int>(key_.size())) {
    DLOG(WARNING) << "Could not write sparse file key";
    return false;
  }

  sparse_ranges_.clear();
  sparse_tail_offset_ = sizeof(header) + key_.size();

  return true;
}

bool SimpleSynchronousEntry::ScanSparseFile(int32* out_sparse_data_size) {
  DCHECK(sparse_file_open());

  int32 sparse_data_size = 0;

  SimpleFileHeader header;
  int header_read_result =
      sparse_file_.Read(0, reinterpret_cast<char*>(&header), sizeof(header));
  if (header_read_result != sizeof(header)) {
    DLOG(WARNING) << "Could not read header from sparse file.";
    return false;
  }

  if (header.initial_magic_number != kSimpleInitialMagicNumber) {
    DLOG(WARNING) << "Sparse file magic number did not match.";
    return false;
  }

  if (header.version != kSimpleVersion) {
    DLOG(WARNING) << "Sparse file unreadable version.";
    return false;
  }

  sparse_ranges_.clear();

  int64 range_header_offset = sizeof(header) + key_.size();
  while (1) {
    SimpleFileSparseRangeHeader range_header;
    int range_header_read_result =
        sparse_file_.Read(range_header_offset,
                          reinterpret_cast<char*>(&range_header),
                          sizeof(range_header));
    if (range_header_read_result == 0)
      break;
    if (range_header_read_result != sizeof(range_header)) {
      DLOG(WARNING) << "Could not read sparse range header.";
      return false;
    }

    if (range_header.sparse_range_magic_number !=
        kSimpleSparseRangeMagicNumber) {
      DLOG(WARNING) << "Invalid sparse range header magic number.";
      return false;
    }

    SparseRange range;
    range.offset = range_header.offset;
    range.length = range_header.length;
    range.data_crc32 = range_header.data_crc32;
    range.file_offset = range_header_offset + sizeof(range_header);
    sparse_ranges_.insert(std::make_pair(range.offset, range));

    range_header_offset += sizeof(range_header) + range.length;

    DCHECK_LE(sparse_data_size, sparse_data_size + range.length);
    sparse_data_size += range.length;
  }

  *out_sparse_data_size = sparse_data_size;
  sparse_tail_offset_ = range_header_offset;

  return true;
}

bool SimpleSynchronousEntry::ReadSparseRange(const SparseRange* range,
                                             int offset, int len, char* buf) {
  DCHECK(range);
  DCHECK(buf);
  DCHECK_GE(range->length, offset);
  DCHECK_GE(range->length, offset + len);

  int bytes_read = sparse_file_.Read(range->file_offset + offset, buf, len);
  if (bytes_read < len) {
    DLOG(WARNING) << "Could not read sparse range.";
    return false;
  }

  // If we read the whole range and we have a crc32, check it.
  if (offset == 0 && len == range->length && range->data_crc32 != 0) {
    uint32 actual_crc32 = crc32(crc32(0L, Z_NULL, 0),
                                reinterpret_cast<const Bytef*>(buf),
                                len);
    if (actual_crc32 != range->data_crc32) {
      DLOG(WARNING) << "Sparse range crc32 mismatch.";
      return false;
    }
  }
  // TODO(ttuttle): Incremental crc32 calculation?

  return true;
}

bool SimpleSynchronousEntry::WriteSparseRange(SparseRange* range,
                                              int offset, int len,
                                              const char* buf) {
  DCHECK(range);
  DCHECK(buf);
  DCHECK_GE(range->length, offset);
  DCHECK_GE(range->length, offset + len);

  uint32 new_crc32 = 0;
  if (offset == 0 && len == range->length) {
    new_crc32 = crc32(crc32(0L, Z_NULL, 0),
                      reinterpret_cast<const Bytef*>(buf),
                      len);
  }

  if (new_crc32 != range->data_crc32) {
    range->data_crc32 = new_crc32;

    SimpleFileSparseRangeHeader header;
    header.sparse_range_magic_number = kSimpleSparseRangeMagicNumber;
    header.offset = range->offset;
    header.length = range->length;
    header.data_crc32 = range->data_crc32;

    int bytes_written = sparse_file_.Write(range->file_offset - sizeof(header),
                                           reinterpret_cast<char*>(&header),
                                           sizeof(header));
    if (bytes_written != implicit_cast<int>(sizeof(header))) {
      DLOG(WARNING) << "Could not rewrite sparse range header.";
      return false;
    }
  }

  int bytes_written = sparse_file_.Write(range->file_offset + offset, buf, len);
  if (bytes_written < len) {
    DLOG(WARNING) << "Could not write sparse range.";
    return false;
  }

  return true;
}

bool SimpleSynchronousEntry::AppendSparseRange(int64 offset,
                                               int len,
                                               const char* buf) {
  DCHECK_LE(0, offset);
  DCHECK_LT(0, len);
  DCHECK(buf);

  uint32 data_crc32 = crc32(crc32(0L, Z_NULL, 0),
                            reinterpret_cast<const Bytef*>(buf),
                            len);

  SimpleFileSparseRangeHeader header;
  header.sparse_range_magic_number = kSimpleSparseRangeMagicNumber;
  header.offset = offset;
  header.length = len;
  header.data_crc32 = data_crc32;

  int bytes_written = sparse_file_.Write(sparse_tail_offset_,
                                         reinterpret_cast<char*>(&header),
                                         sizeof(header));
  if (bytes_written != implicit_cast<int>(sizeof(header))) {
    DLOG(WARNING) << "Could not append sparse range header.";
    return false;
  }
  sparse_tail_offset_ += bytes_written;

  bytes_written = sparse_file_.Write(sparse_tail_offset_, buf, len);
  if (bytes_written < len) {
    DLOG(WARNING) << "Could not append sparse range data.";
    return false;
  }
  int64 data_file_offset = sparse_tail_offset_;
  sparse_tail_offset_ += bytes_written;

  SparseRange range;
  range.offset = offset;
  range.length = len;
  range.data_crc32 = data_crc32;
  range.file_offset = data_file_offset;
  sparse_ranges_.insert(std::make_pair(offset, range));

  return true;
}

}  // namespace disk_cache
