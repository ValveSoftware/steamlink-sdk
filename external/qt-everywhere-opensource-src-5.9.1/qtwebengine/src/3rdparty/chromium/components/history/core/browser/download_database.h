// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/threading/platform_thread.h"
#include "components/history/core/browser/download_types.h"

namespace sql {
class Connection;
}

namespace history {

struct DownloadRow;

// Maintains a table of downloads.
class DownloadDatabase {
 public:
  // Must call InitDownloadTable before using any other functions.
  DownloadDatabase(DownloadInterruptReason download_interrupt_reason_none,
                   DownloadInterruptReason download_interrupt_reason_crash);
  virtual ~DownloadDatabase();

  uint32_t GetNextDownloadId();

  // Get all the downloads from the database.
  void QueryDownloads(std::vector<DownloadRow>* results);

  // Update the state of one download. Returns true if successful.
  // Does not update |url|, |start_time|; uses |id| only
  // to select the row in the database table to update.
  bool UpdateDownload(const DownloadRow& data);

  // Create a new database entry for one download and return true if the
  // creation succeeded, false otherwise.
  bool CreateDownload(const DownloadRow& info);

  // Remove |id| from the database.
  void RemoveDownload(uint32_t id);

  size_t CountDownloads();

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Connection& GetDB() = 0;

  // Returns true if able to successfully add mime types to the downloads table.
  bool MigrateMimeType();

  // Returns true if able to successfully rewrite the invalid values for the
  // |state| field from 3 to 4. Returns false if there was an error fixing the
  // database. See http://crbug.com/140687
  bool MigrateDownloadsState();

  // Returns true if able to successfully add the last interrupt reason and the
  // two target paths to downloads.
  bool MigrateDownloadsReasonPathsAndDangerType();

  // Returns true if able to successfully add the referrer column to the
  // downloads table.
  bool MigrateReferrer();

  // Returns true if able to successfully add the by_ext_id and by_ext_name
  // columns to the downloads table.
  bool MigrateDownloadedByExtension();

  // Returns true if able to successfully add the etag and last-modified columns
  // to the downloads table.
  bool MigrateDownloadValidators();

  // Returns true if able to add GUID, hash and HTTP method columns to the
  // download table.
  bool MigrateHashHttpMethodAndGenerateGuids();

  // Returns true if able to add tab_url and tab_referrer_url columns to the
  // download table.
  bool MigrateDownloadTabUrl();

  // Returns true if able to add the site_url column to the download
  // table.
  bool MigrateDownloadSiteInstanceUrl();

  // Creates the downloads table if needed.
  bool InitDownloadTable();

  // Used to quickly clear the downloads. First you would drop it, then you
  // would re-initialize it.
  bool DropDownloadTable();

 private:
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendDBTest,
                           ConfirmDownloadInProgressCleanup);

  // Fixes state of the download entries. Sometimes entries with IN_PROGRESS
  // state are not updated during browser shutdown (particularly when crashing).
  // On the next start such entries are considered interrupted with
  // interrupt reason |DOWNLOAD_INTERRUPT_REASON_CRASH|.  This function
  // fixes such entries.
  void EnsureInProgressEntriesCleanedUp();

  bool EnsureColumnExists(const std::string& name, const std::string& type);

  void RemoveDownloadURLs(uint32_t id);

  bool owning_thread_set_;
  base::PlatformThreadId owning_thread_;

  // Initialized to false on construction, and checked in all functional
  // routines post-migration in the database for a possible call to
  // CleanUpInProgressEntries().  This allows us to avoid
  // doing the cleanup until after any DB migration and unless we are
  // actually use the downloads database.
  bool in_progress_entry_cleanup_completed_;

  // Those constants are defined in the embedder and injected into the
  // database in the constructor. They represent the interrupt reason
  // to use for respectively an undefined value and in case of a crash.
  DownloadInterruptReason download_interrupt_reason_none_;
  DownloadInterruptReason download_interrupt_reason_crash_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDatabase);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_DOWNLOAD_DATABASE_H_
