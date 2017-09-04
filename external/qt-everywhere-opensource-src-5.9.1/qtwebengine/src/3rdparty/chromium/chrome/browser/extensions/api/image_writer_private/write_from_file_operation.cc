// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/write_from_file_operation.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

WriteFromFileOperation::WriteFromFileOperation(
    base::WeakPtr<OperationManager> manager,
    const ExtensionId& extension_id,
    const base::FilePath& user_file_path,
    const std::string& device_path)
    : Operation(manager, extension_id, device_path) {
  image_path_ = user_file_path;
}

WriteFromFileOperation::~WriteFromFileOperation() {}

void WriteFromFileOperation::StartImpl() {
  if (!base::PathExists(image_path_) || base::DirectoryExists(image_path_)) {
    DLOG(ERROR) << "Source must exist and not be a directory.";
    Error(error::kImageInvalid);
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &WriteFromFileOperation::Unzip,
          this,
          base::Bind(
              &WriteFromFileOperation::Write,
              this,
              base::Bind(&WriteFromFileOperation::VerifyWrite,
                         this,
                         base::Bind(&WriteFromFileOperation::Finish, this)))));
}

}  // namespace image_writer
}  // namespace extensions
