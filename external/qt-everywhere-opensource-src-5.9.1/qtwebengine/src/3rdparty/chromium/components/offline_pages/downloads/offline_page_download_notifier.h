// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_NOTIFIER_H_
#define COMPONENTS_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_NOTIFIER_H_

#include "components/offline_pages/downloads/download_ui_item.h"

namespace offline_pages {

struct OfflinePageDownloadNotifier {
 public:
  virtual ~OfflinePageDownloadNotifier() = default;

  // Reports that |item| has completed successfully.
  virtual void NotifyDownloadSuccessful(const DownloadUIItem& item) = 0;

  // Reports that |item| has completed unsuccessfully.
  virtual void NotifyDownloadFailed(const DownloadUIItem& item) = 0;

  // Reports that |item| is active and possibly making progress.
  virtual void NotifyDownloadProgress(const DownloadUIItem& item) = 0;

  // Reports that |item| has been paused (and so it is not active).
  virtual void NotifyDownloadPaused(const DownloadUIItem& item) = 0;

  // Reports that any progress on |item| has been interrupted. It is pending
  // or available for another attempt when conditions allows.
  virtual void NotifyDownloadInterrupted(const DownloadUIItem& item) = 0;

  // Reports that |item| has been canceled.
  virtual void NotifyDownloadCanceled(const DownloadUIItem& item) = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_DOWNLOADS_OFFLINE_PAGE_DOWNLOAD_NOTIFIER_H_
