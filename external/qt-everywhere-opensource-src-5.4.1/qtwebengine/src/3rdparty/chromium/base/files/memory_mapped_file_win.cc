// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/memory_mapped_file.h"

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/threading/thread_restrictions.h"

namespace base {

MemoryMappedFile::MemoryMappedFile() : data_(NULL), length_(0), image_(false) {
}

bool MemoryMappedFile::InitializeAsImageSection(const FilePath& file_name) {
  image_ = true;
  return Initialize(file_name);
}

bool MemoryMappedFile::MapFileToMemory() {
  ThreadRestrictions::AssertIOAllowed();

  if (!file_.IsValid())
    return false;

  int64 len = file_.GetLength();
  if (len <= 0 || len > kint32max)
    return false;
  length_ = static_cast<size_t>(len);

  int flags = image_ ? SEC_IMAGE | PAGE_READONLY : PAGE_READONLY;

  file_mapping_.Set(::CreateFileMapping(file_.GetPlatformFile(), NULL,
                                        flags, 0, 0, NULL));
  if (!file_mapping_.IsValid())
    return false;

  data_ = static_cast<uint8*>(::MapViewOfFile(file_mapping_.Get(),
                                              FILE_MAP_READ, 0, 0, 0));
  return data_ != NULL;
}

void MemoryMappedFile::CloseHandles() {
  if (data_)
    ::UnmapViewOfFile(data_);
  if (file_mapping_.IsValid())
    file_mapping_.Close();
  if (file_.IsValid())
    file_.Close();

  data_ = NULL;
  length_ = 0;
}

}  // namespace base
