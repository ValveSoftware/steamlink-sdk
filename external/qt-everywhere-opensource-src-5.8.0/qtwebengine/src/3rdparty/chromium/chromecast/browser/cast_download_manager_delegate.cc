// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_download_manager_delegate.h"

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "content/public/browser/download_danger_type.h"
#include "content/public/browser/download_item.h"

namespace chromecast {
namespace shell {

CastDownloadManagerDelegate::CastDownloadManagerDelegate() {}

CastDownloadManagerDelegate::~CastDownloadManagerDelegate() {}

void CastDownloadManagerDelegate::GetNextId(
      const content::DownloadIdCallback& callback) {
  // See default behavior of DownloadManagerImpl::GetNextId()
  static uint32_t next_id = content::DownloadItem::kInvalidId + 1;
  callback.Run(next_id++);
}

bool CastDownloadManagerDelegate::DetermineDownloadTarget(
    content::DownloadItem* item,
    const content::DownloadTargetCallback& callback) {
  // Running the DownloadTargetCallback with an empty FilePath signals
  // that the download should be cancelled.
  base::FilePath empty;
  callback.Run(
      empty,
      content::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT,
      empty);
  return true;
}

bool CastDownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
  return false;
}

bool CastDownloadManagerDelegate::ShouldCompleteDownload(
    content::DownloadItem* item,
    const base::Closure& callback) {
  return false;
}

bool CastDownloadManagerDelegate::ShouldOpenDownload(
    content::DownloadItem* item,
    const content::DownloadOpenDelayedCallback& callback) {
  return false;
}

}  // namespace shell
}  // namespace chromecast