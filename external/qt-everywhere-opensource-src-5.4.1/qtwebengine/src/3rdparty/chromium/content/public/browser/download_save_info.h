// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_SAVE_INFO_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_SAVE_INFO_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "content/common/content_export.h"

namespace content {

// Holds the information about how to save a download file.
// In the case of download continuation, |file_path| is set to the current file
// name, |offset| is set to the point where we left off, and |hash_state| will
// hold the state of the hash algorithm where we left off.
struct CONTENT_EXPORT DownloadSaveInfo {
  DownloadSaveInfo();
  ~DownloadSaveInfo();

  // If non-empty, contains the full target path of the download that has been
  // determined prior to download initiation. This is considered to be a trusted
  // path.
  base::FilePath file_path;

  // If non-empty, contains an untrusted filename suggestion. This can't contain
  // a path (only a filename), and is only effective if |file_path| is empty.
  base::string16 suggested_name;

  // If valid, contains the source data stream for the file contents.
  base::File file;

  // The file offset at which to start the download.  May be 0.
  int64 offset;

  // The state of the hash at the start of the download.  May be empty.
  std::string hash_state;

  // If |prompt_for_save_location| is true, and |file_path| is empty, then
  // the user will be prompted for a location to save the download. Otherwise,
  // the location will be determined automatically using |file_path| as a
  // basis if |file_path| is not empty.
  // |prompt_for_save_location| defaults to false.
  bool prompt_for_save_location;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadSaveInfo);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_SAVE_INFO_H_
