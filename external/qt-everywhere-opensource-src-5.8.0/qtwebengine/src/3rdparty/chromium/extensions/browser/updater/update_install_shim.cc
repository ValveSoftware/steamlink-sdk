// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/update_install_shim.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

UpdateInstallShim::UpdateInstallShim(std::string extension_id,
                                     const base::FilePath& extension_root,
                                     const UpdateInstallShimCallback& callback)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      callback_(callback) {}

void UpdateInstallShim::OnUpdateError(int error) {
  VLOG(1) << "OnUpdateError (" << extension_id_ << ") " << error;
}

bool UpdateInstallShim::Install(const base::DictionaryValue& manifest,
                                const base::FilePath& unpack_path) {
  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir())
    return false;

  // The UpdateClient code will delete unpack_path if it still exists after
  // this method is done, so we rename it on top of our temp dir.
  if (!base::DeleteFile(temp_dir.path(), true) ||
      !base::Move(unpack_path, temp_dir.path())) {
    LOG(ERROR) << "Trying to install update for " << extension_id_
               << "and failed to move " << unpack_path.value() << " to  "
               << temp_dir.path().value();
    return false;
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&UpdateInstallShim::RunCallbackOnUIThread, this,
                 temp_dir.Take()));
  return true;
}

bool UpdateInstallShim::GetInstalledFile(const std::string& file,
                                         base::FilePath* installed_file) {
  base::FilePath relative_path = base::FilePath::FromUTF8Unsafe(file);
  if (relative_path.IsAbsolute() || relative_path.ReferencesParent())
    return false;
  *installed_file = extension_root_.Append(relative_path);
  if (!extension_root_.IsParent(*installed_file) ||
      !base::PathExists(*installed_file)) {
    VLOG(1) << "GetInstalledFile failed to find " << installed_file->value();
    installed_file->clear();
    return false;
  }
  return true;
}

bool UpdateInstallShim::Uninstall() {
  NOTREACHED();
  return false;
}

UpdateInstallShim::~UpdateInstallShim() {}

void UpdateInstallShim::RunCallbackOnUIThread(const base::FilePath& temp_dir) {
  if (callback_.is_null()) {
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE, base::Bind(base::IgnoreResult(&base::DeleteFile), temp_dir,
                              true /*recursive */));
    return;
  }
  callback_.Run(extension_id_, temp_dir);
}

}  // namespace extensions
