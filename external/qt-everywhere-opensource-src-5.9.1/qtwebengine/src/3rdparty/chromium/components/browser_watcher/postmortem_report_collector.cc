// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/postmortem_report_collector.h"

#include <utility>

#include "base/debug/activity_analyzer.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browser_watcher/postmortem_minidump_writer.h"
#include "third_party/crashpad/crashpad/client/settings.h"
#include "third_party/crashpad/crashpad/util/misc/uuid.h"

using base::FilePath;

namespace browser_watcher {

using base::debug::ActivitySnapshot;
using base::debug::GlobalActivityAnalyzer;
using base::debug::ThreadActivityAnalyzer;
using crashpad::CrashReportDatabase;

PostmortemReportCollector::PostmortemReportCollector(
    const std::string& product_name,
    const std::string& version_number,
    const std::string& channel_name)
    : product_name_(product_name),
      version_number_(version_number),
      channel_name_(channel_name) {}

int PostmortemReportCollector::CollectAndSubmitForUpload(
    const base::FilePath& debug_info_dir,
    const base::FilePath::StringType& debug_file_pattern,
    const std::set<base::FilePath>& excluded_debug_files,
    crashpad::CrashReportDatabase* report_database) {
  DCHECK_NE(true, debug_info_dir.empty());
  DCHECK_NE(true, debug_file_pattern.empty());
  DCHECK_NE(nullptr, report_database);

  // Collect the list of files to harvest.
  std::vector<FilePath> debug_files = GetDebugStateFilePaths(
      debug_info_dir, debug_file_pattern, excluded_debug_files);

  // Determine the crashpad client id.
  crashpad::UUID client_id;
  crashpad::Settings* settings = report_database->GetSettings();
  if (settings) {
    // If GetSettings() or GetClientID() fails client_id will be left at its
    // default value, all zeroes, which is appropriate.
    settings->GetClientID(&client_id);
  }

  // Process each stability file.
  int success_cnt = 0;
  for (const FilePath& file : debug_files) {
    CollectionStatus status =
        CollectAndSubmit(client_id, file, report_database);
    // TODO(manzagop): consider making this a stability metric.
    UMA_HISTOGRAM_ENUMERATION("ActivityTracker.Collect.Status", status,
                              COLLECTION_STATUS_MAX);
    if (status == SUCCESS)
      ++success_cnt;
  }

  return success_cnt;
}

std::vector<FilePath> PostmortemReportCollector::GetDebugStateFilePaths(
    const base::FilePath& debug_info_dir,
    const base::FilePath::StringType& debug_file_pattern,
    const std::set<FilePath>& excluded_debug_files) {
  DCHECK_NE(true, debug_info_dir.empty());
  DCHECK_NE(true, debug_file_pattern.empty());

  std::vector<FilePath> paths;
  base::FileEnumerator enumerator(debug_info_dir, false /* recursive */,
                                  base::FileEnumerator::FILES,
                                  debug_file_pattern);
  FilePath path;
  for (path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    if (excluded_debug_files.find(path) == excluded_debug_files.end())
      paths.push_back(path);
  }
  return paths;
}

PostmortemReportCollector::CollectionStatus
PostmortemReportCollector::CollectAndSubmit(
    const crashpad::UUID& client_id,
    const FilePath& file,
    crashpad::CrashReportDatabase* report_database) {
  DCHECK_NE(nullptr, report_database);

  // Note: the code below involves two notions of report: chrome internal state
  // reports and the crashpad reports they get wrapped into.

  // Collect the data from the debug file to a proto.
  std::unique_ptr<StabilityReport> report_proto;
  CollectionStatus status = Collect(file, &report_proto);
  if (status != SUCCESS) {
    // The file was empty, or there was an error collecting the data. Detailed
    // logging happens within the Collect function.
    if (!base::DeleteFile(file, false))
      LOG(ERROR) << "Failed to delete " << file.value();
    return status;
  }
  DCHECK_NE(nullptr, report_proto.get());

  // Prepare a crashpad report.
  CrashReportDatabase::NewReport* new_report = nullptr;
  CrashReportDatabase::OperationStatus database_status =
      report_database->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    LOG(ERROR) << "PrepareNewCrashReport failed";
    return PREPARE_NEW_CRASH_REPORT_FAILED;
  }
  CrashReportDatabase::CallErrorWritingCrashReport
      call_error_writing_crash_report(report_database, new_report);

  // Write the report to a minidump.
  if (!WriteReportToMinidump(*report_proto, client_id, new_report->uuid,
                             reinterpret_cast<FILE*>(new_report->handle))) {
    return WRITE_TO_MINIDUMP_FAILED;
  }

  // If the file cannot be deleted, do not report its contents. Note this can
  // lead to under reporting and retries. However, under reporting is
  // preferable to the over reporting that would happen with a file that
  // cannot be deleted.
  // TODO(manzagop): metrics for the number of non-deletable files.
  if (!base::DeleteFile(file, false)) {
    LOG(ERROR) << "Failed to delete " << file.value();
    return DEBUG_FILE_DELETION_FAILED;
  }

  // Finalize the report wrt the report database. Note that this doesn't trigger
  // an immediate upload, but Crashpad will eventually upload the report (as of
  // writing, the delay is on the order of up to 15 minutes).
  call_error_writing_crash_report.Disarm();
  crashpad::UUID unused_report_id;
  database_status = report_database->FinishedWritingCrashReport(
      new_report, &unused_report_id);
  if (database_status != CrashReportDatabase::kNoError) {
    LOG(ERROR) << "FinishedWritingCrashReport failed";
    return FINISHED_WRITING_CRASH_REPORT_FAILED;
  }

  return SUCCESS;
}

PostmortemReportCollector::CollectionStatus PostmortemReportCollector::Collect(
    const base::FilePath& debug_state_file,
    std::unique_ptr<StabilityReport>* report) {
  DCHECK_NE(nullptr, report);
  report->reset();

  // Create a global analyzer.
  std::unique_ptr<GlobalActivityAnalyzer> global_analyzer =
      GlobalActivityAnalyzer::CreateWithFile(debug_state_file);
  if (!global_analyzer)
    return ANALYZER_CREATION_FAILED;

  // Early exit if there is no data.
  ThreadActivityAnalyzer* thread_analyzer = global_analyzer->GetFirstAnalyzer();
  if (!thread_analyzer) {
    // No data. This case happens in the case of a clean exit.
    return DEBUG_FILE_NO_DATA;
  }

  // Iterate through the thread analyzers, fleshing out the report.
  report->reset(new StabilityReport());
  // Note: a single process is instrumented.
  ProcessState* process_state = (*report)->add_process_states();

  for (; thread_analyzer != nullptr;
       thread_analyzer = global_analyzer->GetNextAnalyzer()) {
    // Only valid analyzers are expected per contract of GetFirstAnalyzer /
    // GetNextAnalyzer.
    DCHECK(thread_analyzer->IsValid());

    if (!process_state->has_process_id()) {
      process_state->set_process_id(
          thread_analyzer->activity_snapshot().process_id);
    }
    DCHECK_EQ(thread_analyzer->activity_snapshot().process_id,
              process_state->process_id());

    ThreadState* thread_state = process_state->add_threads();
    CollectThread(thread_analyzer->activity_snapshot(), thread_state);
  }

  return SUCCESS;
}

void PostmortemReportCollector::CollectThread(
    const base::debug::ActivitySnapshot& snapshot,
    ThreadState* thread_state) {
  DCHECK(thread_state);

  thread_state->set_thread_name(snapshot.thread_name);
  thread_state->set_thread_id(snapshot.thread_id);
  thread_state->set_activity_count(snapshot.activity_stack_depth);

  for (const base::debug::Activity& recorded : snapshot.activity_stack) {
    Activity* collected = thread_state->add_activities();
    switch (recorded.activity_type) {
      case base::debug::Activity::ACT_TASK_RUN:
        collected->set_type(Activity::ACT_TASK_RUN);
        collected->set_origin_address(recorded.origin_address);
        collected->set_task_sequence_id(recorded.data.task.sequence_id);
        break;
      case base::debug::Activity::ACT_LOCK_ACQUIRE:
        collected->set_type(Activity::ACT_LOCK_ACQUIRE);
        collected->set_lock_address(recorded.data.lock.lock_address);
        break;
      case base::debug::Activity::ACT_EVENT_WAIT:
        collected->set_type(Activity::ACT_EVENT_WAIT);
        collected->set_event_address(recorded.data.event.event_address);
        break;
      case base::debug::Activity::ACT_THREAD_JOIN:
        collected->set_type(Activity::ACT_THREAD_JOIN);
        collected->set_thread_id(recorded.data.thread.thread_id);
        break;
      case base::debug::Activity::ACT_PROCESS_WAIT:
        collected->set_type(Activity::ACT_PROCESS_WAIT);
        collected->set_process_id(recorded.data.process.process_id);
        break;
      default:
        break;
    }
  }
}

bool PostmortemReportCollector::WriteReportToMinidump(
    const StabilityReport& report,
    const crashpad::UUID& client_id,
    const crashpad::UUID& report_id,
    base::PlatformFile minidump_file) {
  MinidumpInfo minidump_info;
  minidump_info.client_id = client_id;
  minidump_info.report_id = report_id;
  // TODO(manzagop): replace this information, i.e. the reporter's attributes,
  // by that of the reportee. Doing so requires adding this information to the
  // stability report. In the meantime, there is a tolerable information
  // mismatch after upgrades.
  minidump_info.product_name = product_name();
  minidump_info.version_number = version_number();
  minidump_info.channel_name = channel_name();
#if defined(ARCH_CPU_X86)
  minidump_info.platform = std::string("Win32");
#elif defined(ARCH_CPU_X86_64)
  minidump_info.platform = std::string("Win64");
#endif

  return WritePostmortemDump(minidump_file, report, minidump_info);
}

}  // namespace browser_watcher
