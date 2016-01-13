// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/base_file.h"

#include "base/file_util.h"
#include "content/public/browser/download_interrupt_reasons.h"

namespace content {

DownloadInterruptReason BaseFile::MoveFileAndAdjustPermissions(
    const base::FilePath& new_path) {
  // Similarly, on Unix, we're moving a temp file created with permissions 600
  // to |new_path|. Here, we try to fix up the destination file with appropriate
  // permissions.
  struct stat st;
  // First check the file existence and create an empty file if it doesn't
  // exist.
  if (!base::PathExists(new_path)) {
    int write_error = base::WriteFile(new_path, "", 0);
    if (write_error < 0)
      return LogSystemError("WriteFile", errno);
  }
  int stat_error = stat(new_path.value().c_str(), &st);
  bool stat_succeeded = (stat_error == 0);
  if (!stat_succeeded)
    LogSystemError("stat", errno);

  // TODO(estade): Move() falls back to copying and deleting when a simple
  // rename fails. Copying sucks for large downloads. crbug.com/8737
  if (!base::Move(full_path_, new_path))
    return LogSystemError("Move", errno);

  if (stat_succeeded) {
    // On Windows file systems (FAT, NTFS), chmod fails.  This is OK.
    int chmod_error = chmod(new_path.value().c_str(), st.st_mode);
    if (chmod_error < 0)
      LogSystemError("chmod", errno);
  }
  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

}  // namespace content
