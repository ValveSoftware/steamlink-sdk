// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_model_event_logger.h"

namespace offline_pages {

void OfflinePageModelEventLogger::RecordPageSaved(const std::string& name_space,
                                                  const std::string& url,
                                                  const std::string& id) {
  RecordActivity(url + " is saved at " + name_space + " with id " + id);
}

void OfflinePageModelEventLogger::RecordPageDeleted(const std::string& id) {
  RecordActivity("Page with ID " + id + " has been deleted");
}

void OfflinePageModelEventLogger::RecordStoreClearError() {
  RecordActivity("Offline store clear failed");
}

void OfflinePageModelEventLogger::RecordStoreCleared() {
  RecordActivity("Offline store cleared");
}

void OfflinePageModelEventLogger::RecordStoreReloadError() {
  RecordActivity("There was an error reloading the offline store");
}
}  // namespace offline_pages
