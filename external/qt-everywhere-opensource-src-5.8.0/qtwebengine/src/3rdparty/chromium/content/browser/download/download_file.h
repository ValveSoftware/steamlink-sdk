// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_interrupt_reasons.h"

class GURL;

namespace content {

class DownloadManager;

// These objects live exclusively on the file thread and handle the writing
// operations for one download. These objects live only for the duration that
// the download is 'in progress': once the download has been completed or
// cancelled, the DownloadFile is destroyed.
class CONTENT_EXPORT DownloadFile {
 public:
  // Callback used with Initialize.  On a successful initialize, |reason| will
  // be DOWNLOAD_INTERRUPT_REASON_NONE; on a failed initialize, it will be
  // set to the reason for the failure.
  typedef base::Callback<void(DownloadInterruptReason reason)>
      InitializeCallback;

  // Callback used with Rename*().  On a successful rename |reason| will be
  // DOWNLOAD_INTERRUPT_REASON_NONE and |path| the path the rename
  // was done to.  On a failed rename, |reason| will contain the
  // error.
  typedef base::Callback<void(DownloadInterruptReason reason,
                              const base::FilePath& path)>
      RenameCompletionCallback;

  virtual ~DownloadFile() {}

  // Upon completion, |callback| will be called on the UI
  // thread as per the comment above, passing DOWNLOAD_INTERRUPT_REASON_NONE
  // on success, or a network download interrupt reason on failure.
  virtual void Initialize(const InitializeCallback& callback) = 0;

  // Rename the download file to |full_path|.  If that file exists
  // |full_path| will be uniquified by suffixing " (<number>)" to the
  // file name before the extension.
  virtual void RenameAndUniquify(const base::FilePath& full_path,
                                 const RenameCompletionCallback& callback) = 0;

  // Rename the download file to |full_path| and annotate it with
  // "Mark of the Web" information about its source.  No uniquification
  // will be performed.
  virtual void RenameAndAnnotate(const base::FilePath& full_path,
                                 const std::string& client_guid,
                                 const GURL& source_url,
                                 const GURL& referrer_url,
                                 const RenameCompletionCallback& callback) = 0;

  // Detach the file so it is not deleted on destruction.
  virtual void Detach() = 0;

  // Abort the download and automatically close the file.
  virtual void Cancel() = 0;

  virtual const base::FilePath& FullPath() const = 0;
  virtual bool InProgress() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_FILE_H_
