// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_system_usage_cache.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "storage/browser/fileapi/timed_task_helper.h"

namespace storage {

namespace {
const int64_t kCloseDelaySeconds = 5;
const size_t kMaxHandleCacheSize = 2;
}  // namespace

FileSystemUsageCache::FileSystemUsageCache(
    base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner),
      weak_factory_(this) {
}

FileSystemUsageCache::~FileSystemUsageCache() {
  task_runner_ = NULL;
  CloseCacheFiles();
}

const base::FilePath::CharType FileSystemUsageCache::kUsageFileName[] =
    FILE_PATH_LITERAL(".usage");
const char FileSystemUsageCache::kUsageFileHeader[] = "FSU5";
const int FileSystemUsageCache::kUsageFileHeaderSize = 4;

// Pickle::{Read,Write}Bool treat bool as int
const int FileSystemUsageCache::kUsageFileSize =
    sizeof(base::Pickle::Header) + FileSystemUsageCache::kUsageFileHeaderSize +
    sizeof(int) + sizeof(int32_t) + sizeof(int64_t);  // NOLINT

bool FileSystemUsageCache::GetUsage(const base::FilePath& usage_file_path,
                                    int64_t* usage_out) {
  TRACE_EVENT0("FileSystem", "UsageCache::GetUsage");
  DCHECK(CalledOnValidThread());
  DCHECK(usage_out);
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  *usage_out = usage;
  return true;
}

bool FileSystemUsageCache::GetDirty(const base::FilePath& usage_file_path,
                                    uint32_t* dirty_out) {
  TRACE_EVENT0("FileSystem", "UsageCache::GetDirty");
  DCHECK(CalledOnValidThread());
  DCHECK(dirty_out);
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  *dirty_out = dirty;
  return true;
}

bool FileSystemUsageCache::IncrementDirty(
    const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::IncrementDirty");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  bool new_handle = !HasCacheFileHandle(usage_file_path);
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;

  bool success = Write(usage_file_path, is_valid, dirty + 1, usage);
  if (success && dirty == 0 && new_handle)
    FlushFile(usage_file_path);
  return success;
}

bool FileSystemUsageCache::DecrementDirty(
    const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::DecrementDirty");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage) || dirty == 0)
    return false;

  return Write(usage_file_path, is_valid, dirty - 1, usage);
}

bool FileSystemUsageCache::Invalidate(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::Invalidate");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;

  return Write(usage_file_path, false, dirty, usage);
}

bool FileSystemUsageCache::IsValid(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::IsValid");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  return is_valid;
}

bool FileSystemUsageCache::AtomicUpdateUsageByDelta(
    const base::FilePath& usage_file_path,
    int64_t delta) {
  TRACE_EVENT0("FileSystem", "UsageCache::AtomicUpdateUsageByDelta");
  DCHECK(CalledOnValidThread());
  bool is_valid = true;
  uint32_t dirty = 0;
  int64_t usage = 0;
  if (!Read(usage_file_path, &is_valid, &dirty, &usage))
    return false;
  return Write(usage_file_path, is_valid, dirty, usage + delta);
}

bool FileSystemUsageCache::UpdateUsage(const base::FilePath& usage_file_path,
                                       int64_t fs_usage) {
  TRACE_EVENT0("FileSystem", "UsageCache::UpdateUsage");
  DCHECK(CalledOnValidThread());
  return Write(usage_file_path, true, 0, fs_usage);
}

bool FileSystemUsageCache::Exists(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::Exists");
  DCHECK(CalledOnValidThread());
  return base::PathExists(usage_file_path);
}

bool FileSystemUsageCache::Delete(const base::FilePath& usage_file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::Delete");
  DCHECK(CalledOnValidThread());
  CloseCacheFiles();
  return base::DeleteFile(usage_file_path, false);
}

void FileSystemUsageCache::CloseCacheFiles() {
  TRACE_EVENT0("FileSystem", "UsageCache::CloseCacheFiles");
  DCHECK(CalledOnValidThread());
  STLDeleteValues(&cache_files_);
  timer_.reset();
}

bool FileSystemUsageCache::Read(const base::FilePath& usage_file_path,
                                bool* is_valid,
                                uint32_t* dirty_out,
                                int64_t* usage_out) {
  TRACE_EVENT0("FileSystem", "UsageCache::Read");
  DCHECK(CalledOnValidThread());
  DCHECK(is_valid);
  DCHECK(dirty_out);
  DCHECK(usage_out);
  char buffer[kUsageFileSize];
  const char *header;
  if (usage_file_path.empty() ||
      !ReadBytes(usage_file_path, buffer, kUsageFileSize))
    return false;
  base::Pickle read_pickle(buffer, kUsageFileSize);
  base::PickleIterator iter(read_pickle);
  uint32_t dirty = 0;
  int64_t usage = 0;

  if (!iter.ReadBytes(&header, kUsageFileHeaderSize) ||
      !iter.ReadBool(is_valid) ||
      !iter.ReadUInt32(&dirty) ||
      !iter.ReadInt64(&usage))
    return false;

  if (header[0] != kUsageFileHeader[0] ||
      header[1] != kUsageFileHeader[1] ||
      header[2] != kUsageFileHeader[2] ||
      header[3] != kUsageFileHeader[3])
    return false;

  *dirty_out = dirty;
  *usage_out = usage;
  return true;
}

bool FileSystemUsageCache::Write(const base::FilePath& usage_file_path,
                                 bool is_valid,
                                 int32_t dirty,
                                 int64_t usage) {
  TRACE_EVENT0("FileSystem", "UsageCache::Write");
  DCHECK(CalledOnValidThread());
  base::Pickle write_pickle;
  write_pickle.WriteBytes(kUsageFileHeader, kUsageFileHeaderSize);
  write_pickle.WriteBool(is_valid);
  write_pickle.WriteUInt32(dirty);
  write_pickle.WriteInt64(usage);

  if (!WriteBytes(usage_file_path,
                  static_cast<const char*>(write_pickle.data()),
                  write_pickle.size())) {
    Delete(usage_file_path);
    return false;
  }
  return true;
}

base::File* FileSystemUsageCache::GetFile(const base::FilePath& file_path) {
  DCHECK(CalledOnValidThread());
  if (cache_files_.size() >= kMaxHandleCacheSize)
    CloseCacheFiles();
  ScheduleCloseTimer();

  base::File* new_file = NULL;
  std::pair<CacheFiles::iterator, bool> inserted =
      cache_files_.insert(std::make_pair(file_path, new_file));
  if (!inserted.second)
    return inserted.first->second;

  new_file = new base::File(file_path,
                            base::File::FLAG_OPEN_ALWAYS |
                            base::File::FLAG_READ |
                            base::File::FLAG_WRITE);
  if (!new_file->IsValid()) {
    cache_files_.erase(inserted.first);
    delete new_file;
    return NULL;
  }

  inserted.first->second = new_file;
  return new_file;
}

bool FileSystemUsageCache::ReadBytes(const base::FilePath& file_path,
                                     char* buffer,
                                     int64_t buffer_size) {
  DCHECK(CalledOnValidThread());
  base::File* file = GetFile(file_path);
  if (!file)
    return false;
  return file->Read(0, buffer, buffer_size) == buffer_size;
}

bool FileSystemUsageCache::WriteBytes(const base::FilePath& file_path,
                                      const char* buffer,
                                      int64_t buffer_size) {
  DCHECK(CalledOnValidThread());
  base::File* file = GetFile(file_path);
  if (!file)
    return false;
  return file->Write(0, buffer, buffer_size) == buffer_size;
}

bool FileSystemUsageCache::FlushFile(const base::FilePath& file_path) {
  TRACE_EVENT0("FileSystem", "UsageCache::FlushFile");
  DCHECK(CalledOnValidThread());
  base::File* file = GetFile(file_path);
  if (!file)
    return false;
  return file->Flush();
}

void FileSystemUsageCache::ScheduleCloseTimer() {
  DCHECK(CalledOnValidThread());
  if (!timer_)
    timer_.reset(new TimedTaskHelper(task_runner_.get()));

  if (timer_->IsRunning()) {
    timer_->Reset();
    return;
  }

  timer_->Start(FROM_HERE,
                base::TimeDelta::FromSeconds(kCloseDelaySeconds),
                base::Bind(&FileSystemUsageCache::CloseCacheFiles,
                           weak_factory_.GetWeakPtr()));
}

bool FileSystemUsageCache::CalledOnValidThread() {
  return !task_runner_.get() || task_runner_->RunsTasksOnCurrentThread();
}

bool FileSystemUsageCache::HasCacheFileHandle(const base::FilePath& file_path) {
  DCHECK(CalledOnValidThread());
  DCHECK_LE(cache_files_.size(), kMaxHandleCacheSize);
  return ContainsKey(cache_files_, file_path);
}

}  // namespace storage
