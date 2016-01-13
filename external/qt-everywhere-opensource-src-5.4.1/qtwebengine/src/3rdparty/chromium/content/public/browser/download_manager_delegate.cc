// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager_delegate.h"

#include "content/public/browser/download_item.h"

namespace content {

void DownloadManagerDelegate::GetNextId(const DownloadIdCallback& callback) {
  callback.Run(content::DownloadItem::kInvalidId);
}

bool DownloadManagerDelegate::DetermineDownloadTarget(
    DownloadItem* item,
    const DownloadTargetCallback& callback) {
  return false;
}

bool DownloadManagerDelegate::ShouldOpenFileBasedOnExtension(
    const base::FilePath& path) {
  return false;
}

bool DownloadManagerDelegate::ShouldCompleteDownload(
    DownloadItem* item,
    const base::Closure& callback) {
  return true;
}

bool DownloadManagerDelegate::ShouldOpenDownload(
    DownloadItem* item, const DownloadOpenDelayedCallback& callback) {
  return true;
}

bool DownloadManagerDelegate::GenerateFileHash() {
  return false;
}

std::string
DownloadManagerDelegate::ApplicationClientIdForFileScanning() const {
  return std::string();
}

DownloadManagerDelegate::~DownloadManagerDelegate() {}

}  // namespace content
