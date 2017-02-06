// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_file.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// TODO(asanka): SaveFile should use the target directory of the save package as
//               the default download directory when initializing |file_|.
//               Unfortunately, as it is, constructors of SaveFile don't always
//               have access to the SavePackage at this point.
SaveFile::SaveFile(const SaveFileCreateInfo* info, bool calculate_hash)
    : file_(net::BoundNetLog()), info_(info) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);

  DCHECK(info);
  DCHECK(info->path.empty());
}

SaveFile::~SaveFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
}

DownloadInterruptReason SaveFile::Initialize() {
  return file_.Initialize(base::FilePath(), base::FilePath(), base::File(), 0,
                          std::string(), std::unique_ptr<crypto::SecureHash>());
}

DownloadInterruptReason SaveFile::AppendDataToFile(const char* data,
                                                   size_t data_len) {
  return file_.AppendDataToFile(data, data_len);
}

DownloadInterruptReason SaveFile::Rename(const base::FilePath& full_path) {
  return file_.Rename(full_path);
}

void SaveFile::Detach() {
  file_.Detach();
}

void SaveFile::Cancel() {
  file_.Cancel();
}

void SaveFile::Finish() {
  file_.Finish();
}

void SaveFile::AnnotateWithSourceInformation() {
  // TODO(gbillock): If this method is called, it should set the
  // file_.SetClientGuid() method first.
  NOTREACHED();
}

base::FilePath SaveFile::FullPath() const {
  return file_.full_path();
}

bool SaveFile::InProgress() const {
  return file_.in_progress();
}

int64_t SaveFile::BytesSoFar() const {
  return file_.bytes_so_far();
}

std::string SaveFile::DebugString() const {
  return file_.DebugString();
}

}  // namespace content
