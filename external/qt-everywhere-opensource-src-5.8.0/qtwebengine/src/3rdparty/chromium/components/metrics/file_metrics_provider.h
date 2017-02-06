// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/metrics/metrics_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class MemoryMappedFile;
class PersistentMemoryAllocator;
class TaskRunner;
}

namespace metrics {

// FileMetricsProvider gathers and logs histograms written to files on disk.
// Any number of files can be registered and will be polled once per upload
// cycle (at startup and periodically thereafter -- about every 30 minutes
// for desktop) for data to send.
class FileMetricsProvider : public MetricsProvider {
 public:
  enum SourceType {
    // "Atomic" files are a collection of histograms that are written
    // completely in a single atomic operation (typically a write followed
    // by an atomic rename) and the file is never updated again except to
    // be replaced by a completely new set of histograms. This is the only
    // option that can be used if the file is not writeable by *this*
    // process. Once the file has been read, an attempt will be made to
    // delete it thus providing some measure of safety should different
    // instantiations (such as by different users of a system-level install)
    // try to read it. In case the delete operation fails, this class
    // persistently tracks the last-modified time of the file so it will
    // not be read a second time.
    SOURCE_HISTOGRAMS_ATOMIC_FILE,

    // A directory of atomic PMA files. This handles a directory in which
    // files of metrics are atomically added. Only files ending with ".pma"
    // will be read. They are read according to their last-modified time and
    // never read more that once (unless they change). Only one file will
    // be read per reporting cycle. Filenames that start with a dot (.) or
    // an underscore (_) are ignored so temporary files (perhaps created by
    // the ImportantFileWriter) will not get read. Files that have been
    // read will be attempted to be deleted; should those files not be
    // deletable by this process, it is the reponsibility of the producer
    // to keep the directory pruned in some manner.
    SOURCE_HISTOGRAMS_ATOMIC_DIR,

    // "Active" files may be open by one or more other processes and updated
    // at any time with new samples or new histograms. Such files may also be
    // inactive for any period of time only to be opened again and have new
    // data written to them. The file should probably never be deleted because
    // there would be no guarantee that the data has been reported.
    // TODO(bcwhite): Enable when read/write mem-mapped files are supported.
    SOURCE_HISTOGRAMS_ACTIVE_FILE,
  };

  enum SourceAssociation {
    // Associates the metrics in the file with the current run of the browser.
    // The reporting will take place as part of the normal logging of
    // histograms.
    ASSOCIATE_CURRENT_RUN,

    // Associates the metrics in the file with the previous run of the browesr.
    // The reporting will take place as part of the "stability" histograms.
    // This is important when metrics are dumped as part of a crash of the
    // previous run. This can only be used with FILE_HISTOGRAMS_ATOMIC.
    ASSOCIATE_PREVIOUS_RUN,
  };

  FileMetricsProvider(const scoped_refptr<base::TaskRunner>& task_runner,
                      PrefService* local_state);
  ~FileMetricsProvider() override;

  // Indicates a file or directory to be monitored and how the file or files
  // within that directory are used. Because some metadata may need to persist
  // across process restarts, preferences entries are used based on the
  // |prefs_key| name. Call RegisterPrefs() with the same name to create the
  // necessary keys in advance. Set |prefs_key| empty if no persistence is
  // required.
  void RegisterSource(const base::FilePath& path,
                      SourceType type,
                      SourceAssociation source_association,
                      const base::StringPiece prefs_key);

  // Registers all necessary preferences for maintaining persistent state
  // about a monitored file across process restarts. The |prefs_key| is
  // typically the filename.
  static void RegisterPrefs(PrefRegistrySimple* prefs,
                            const base::StringPiece prefs_key);

 private:
  friend class FileMetricsProviderTest;

  // The different results that can occur accessing a file.
  enum AccessResult {
    // File was successfully mapped.
    ACCESS_RESULT_SUCCESS,

    // File does not exist.
    ACCESS_RESULT_DOESNT_EXIST,

    // File exists but not modified since last read.
    ACCESS_RESULT_NOT_MODIFIED,

    // File is not valid: is a directory or zero-size.
    ACCESS_RESULT_INVALID_FILE,

    // System could not map file into memory.
    ACCESS_RESULT_SYSTEM_MAP_FAILURE,

    // File had invalid contents.
    ACCESS_RESULT_INVALID_CONTENTS,

    // File could not be opened.
    ACCESS_RESULT_NO_OPEN,

    ACCESS_RESULT_MAX
  };

  // Information about sources being monitored; defined and used exclusively
  // inside the .cc file.
  struct SourceInfo;
  using SourceInfoList = std::list<std::unique_ptr<SourceInfo>>;

  // Looks for the next file to read within a directory. Returns true if a
  // file was found. This is part of CheckAndMapNewMetricSourcesOnTaskRunner
  // and so runs on an thread capable of I/O. The |source| structure will
  // be internally updated to indicate the next file to be read.
  static bool LocateNextFileInDirectory(SourceInfo* source);

  // Checks a list of sources (on a task-runner allowed to do I/O) and merge
  // any data found within them.
  static void CheckAndMergeMetricSourcesOnTaskRunner(SourceInfoList* sources);

  // Checks a single source and maps it into memory.
  static AccessResult CheckAndMapMetricSource(SourceInfo* source);

  // Merges all of the histograms from a |source| to the StatisticsRecorder.
  static void MergeHistogramDeltasFromSource(SourceInfo* source);

  // Records all histograms from a given source via a snapshot-manager.
  static void RecordHistogramSnapshotsFromSource(
      base::HistogramSnapshotManager* snapshot_manager,
      SourceInfo* source);

  // Creates a task to check all monitored sources for updates.
  void ScheduleSourcesCheck();

  // Takes a list of sources checked by an external task and determines what
  // to do with each.
  void RecordSourcesChecked(SourceInfoList* checked);

  // Schedules the deletion of a file in the background using the task-runner.
  void DeleteFileAsync(const base::FilePath& path);

  // Updates the persistent state information to show a source as being read.
  void RecordSourceAsRead(SourceInfo* source);

  // metrics::MetricsDataProvider:
  void OnDidCreateMetricsLog() override;
  bool HasInitialStabilityMetrics() override;
  void MergeHistogramDeltas() override;
  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) override;

  // A task-runner capable of performing I/O.
  scoped_refptr<base::TaskRunner> task_runner_;

  // A list of sources not currently active that need to be checked for changes.
  SourceInfoList sources_to_check_;

  // A list of currently active sources to be merged when required.
  SourceInfoList sources_mapped_;

  // A list of sources for a previous run. These are held separately because
  // they are not subject to the periodic background checking that handles
  // metrics for the current run.
  SourceInfoList sources_for_previous_run_;

  // The preferences-service used to store persistent state about sources.
  PrefService* pref_service_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<FileMetricsProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_FILE_METRICS_PROVIDER_H_
