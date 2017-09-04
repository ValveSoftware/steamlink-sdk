// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Following an unclean shutdown, a stability report can be collected and
// submitted for upload to a reporter.

#ifndef COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_COLLECTOR_H_
#define COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_COLLECTOR_H_

#include <stdio.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/debug/activity_tracker.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/browser_watcher/stability_report.pb.h"
#include "third_party/crashpad/crashpad/client/crash_report_database.h"

namespace browser_watcher {

// Collects unclean shutdown information and creates Crashpad minidumps.
// TODO(manzagop): throttling, graceful handling of accumulating data.
// TODO(manzagop): UMA metrics and some error logging.
class PostmortemReportCollector {
 public:
  // DO NOT CHANGE VALUES. This is logged persistently in a histogram.
  enum CollectionStatus {
    NONE = 0,
    SUCCESS = 1,  // Successfully registered a report with Crashpad.
    ANALYZER_CREATION_FAILED = 2,
    DEBUG_FILE_NO_DATA = 3,
    PREPARE_NEW_CRASH_REPORT_FAILED = 4,
    WRITE_TO_MINIDUMP_FAILED = 5,
    DEBUG_FILE_DELETION_FAILED = 6,
    FINISHED_WRITING_CRASH_REPORT_FAILED = 7,
    COLLECTION_STATUS_MAX = 8
  };

  PostmortemReportCollector(const std::string& product_name,
                            const std::string& version_number,
                            const std::string& channel_name);
  virtual ~PostmortemReportCollector() = default;

  // Collects postmortem stability reports from files found in |debug_info_dir|,
  // relying on |debug_file_pattern| and |excluded_debug_files|. Reports are
  // then wrapped in Crashpad reports, manufactured via |report_database|.
  // Returns the number crash reports successfully registered with the reporter.
  // TODO(manzagop): consider mechanisms for partial collection if this is to be
  //     used on a critical path.
  int CollectAndSubmitForUpload(
      const base::FilePath& debug_info_dir,
      const base::FilePath::StringType& debug_file_pattern,
      const std::set<base::FilePath>& excluded_debug_files,
      crashpad::CrashReportDatabase* report_database);

  const std::string& product_name() const { return product_name_; }
  const std::string& version_number() const { return version_number_; }
  const std::string& channel_name() const { return channel_name_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorTest,
                           GetDebugStateFilePaths);
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorTest, CollectEmptyFile);
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorTest, CollectRandomFile);
  FRIEND_TEST_ALL_PREFIXES(PostmortemReportCollectorCollectionTest,
                           CollectSuccess);

  // Virtual for unittesting.
  virtual std::vector<base::FilePath> GetDebugStateFilePaths(
      const base::FilePath& debug_info_dir,
      const base::FilePath::StringType& debug_file_pattern,
      const std::set<base::FilePath>& excluded_debug_files);

  CollectionStatus CollectAndSubmit(
      const crashpad::UUID& client_id,
      const base::FilePath& file,
      crashpad::CrashReportDatabase* report_database);

  // Virtual for unittesting.
  // TODO(manzagop): move this for reuse in live scenario.
  virtual CollectionStatus Collect(const base::FilePath& debug_state_file,
                                   std::unique_ptr<StabilityReport>* report);
  void CollectThread(const base::debug::ActivitySnapshot& snapshot,
                     ThreadState* thread_state);

  virtual bool WriteReportToMinidump(const StabilityReport& report,
                                     const crashpad::UUID& client_id,
                                     const crashpad::UUID& report_id,
                                     base::PlatformFile minidump_file);

  std::string product_name_;
  std::string version_number_;
  std::string channel_name_;

  DISALLOW_COPY_AND_ASSIGN(PostmortemReportCollector);
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_POSTMORTEM_REPORT_COLLECTOR_H_
