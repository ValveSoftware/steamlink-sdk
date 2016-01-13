// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/download/base_file.h"
#include "content/browser/download/save_types.h"

namespace content {
// SaveFile ----------------------------------------------------------------

// These objects live exclusively on the file thread and handle the writing
// operations for one save item. These objects live only for the duration that
// the saving job is 'in progress': once the saving job has been completed or
// canceled, the SaveFile is destroyed. One SaveFile object represents one item
// in a save session.
class SaveFile {
 public:
  explicit SaveFile(const SaveFileCreateInfo* info, bool calculate_hash);
  virtual ~SaveFile();

  // BaseFile delegated functions.
  DownloadInterruptReason Initialize();
  DownloadInterruptReason AppendDataToFile(const char* data, size_t data_len);
  DownloadInterruptReason Rename(const base::FilePath& full_path);
  void Detach();
  void Cancel();
  void Finish();
  void AnnotateWithSourceInformation();
  base::FilePath FullPath() const;
  bool InProgress() const;
  int64 BytesSoFar() const;
  bool GetHash(std::string* hash);
  std::string DebugString() const;

  // Accessors.
  int save_id() const { return info_->save_id; }
  int render_process_id() const { return info_->render_process_id; }
  int render_view_id() const { return info_->render_view_id; }
  int request_id() const { return info_->request_id; }
  SaveFileCreateInfo::SaveFileSource save_source() const {
    return info_->save_source;
  }

 private:
  BaseFile file_;
  scoped_ptr<const SaveFileCreateInfo> info_;

  DISALLOW_COPY_AND_ASSIGN(SaveFile);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_FILE_H_
