// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_test_archiver.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

OfflinePageTestArchiver::OfflinePageTestArchiver(
    Observer* observer,
    const GURL& url,
    ArchiverResult result,
    int64_t size_to_report,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : observer_(observer),
      url_(url),
      result_(result),
      size_to_report_(size_to_report),
      create_archive_called_(false),
      delayed_(false),
      task_runner_(task_runner) {}

OfflinePageTestArchiver::~OfflinePageTestArchiver() {
  EXPECT_TRUE(create_archive_called_);
}

void OfflinePageTestArchiver::CreateArchive(
    const base::FilePath& archives_dir,
    int64_t archive_id,
    const CreateArchiveCallback& callback) {
  create_archive_called_ = true;
  callback_ = callback;
  archives_dir_ = archives_dir;
  if (!delayed_)
    CompleteCreateArchive();
}

void OfflinePageTestArchiver::CompleteCreateArchive() {
  DCHECK(!callback_.is_null());
  base::FilePath archive_path;
  if (filename_.empty()) {
    ASSERT_TRUE(base::CreateTemporaryFileInDir(archives_dir_, &archive_path));
  } else {
    archive_path = archives_dir_.Append(filename_);
    // This step ensures the file is created and closed immediately.
    base::File file(archive_path, base::File::FLAG_OPEN_ALWAYS);
  }
  observer_->SetLastPathCreatedByArchiver(archive_path);
  task_runner_->PostTask(FROM_HERE, base::Bind(callback_, this, result_, url_,
                                               archive_path, size_to_report_));
}

}  // namespace offline_pages
