// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_EVENT_LOGGER_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_EVENT_LOGGER_H_

#include "components/offline_pages/offline_event_logger.h"

namespace offline_pages {

class OfflinePageModelEventLogger : public OfflineEventLogger {
 public:
  // Records that a page has been saved for |name_space| with |url|
  // and |offline_id|.
  void RecordPageSaved(const std::string& name_space,
                       const std::string& url,
                       const std::string& offline_id);

  // Records that a page with |offline_id| has been deleted.
  void RecordPageDeleted(const std::string& offline_id);

  // Records that the offline store has been cleared.
  void RecordStoreCleared();

  // Records that there was an error when clearing the offline store.
  void RecordStoreClearError();

  // Records that there was an error when reloading the offline store.
  void RecordStoreReloadError();
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_MODEL_EVENT_LOGGER_H_
