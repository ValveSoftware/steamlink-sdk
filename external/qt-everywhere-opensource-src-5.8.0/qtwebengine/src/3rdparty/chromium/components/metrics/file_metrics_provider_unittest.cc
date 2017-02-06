// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/file_metrics_provider.h"

#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kMetricsName[] = "TestMetrics";
const char kMetricsFilename[] = "file.metrics";
}  // namespace

namespace metrics {

class HistogramFlattenerDeltaRecorder : public base::HistogramFlattener {
 public:
  HistogramFlattenerDeltaRecorder() {}

  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override {
    // Only remember locally created histograms; they have exactly 2 chars.
    if (histogram.histogram_name().length() == 2)
      recorded_delta_histogram_names_.push_back(histogram.histogram_name());
  }

  void InconsistencyDetected(base::HistogramBase::Inconsistency problem)
      override {
    ASSERT_TRUE(false);
  }

  void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override {
    ASSERT_TRUE(false);
  }

  void InconsistencyDetectedInLoggedCount(int amount) override {
    ASSERT_TRUE(false);
  }

  std::vector<std::string> GetRecordedDeltaHistogramNames() {
    return recorded_delta_histogram_names_;
  }

 private:
  std::vector<std::string> recorded_delta_histogram_names_;

  DISALLOW_COPY_AND_ASSIGN(HistogramFlattenerDeltaRecorder);
};


class FileMetricsProviderTest : public testing::TestWithParam<bool> {
 protected:
  const size_t kSmallFileSize = 64 << 10;  // 64 KiB
  const size_t kLargeFileSize =  2 << 20;  //  2 MiB

  enum : int { kMaxCreateHistograms = 10 };

  FileMetricsProviderTest()
      : create_large_files_(GetParam()),
        task_runner_(new base::TestSimpleTaskRunner()),
        thread_task_runner_handle_(task_runner_),
        statistics_recorder_(
            base::StatisticsRecorder::CreateTemporaryForTesting()),
        prefs_(new TestingPrefServiceSimple) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    FileMetricsProvider::RegisterPrefs(prefs_->registry(), kMetricsName);
  }

  ~FileMetricsProviderTest() override {
    // Clear out any final remaining tasks.
    task_runner_->RunUntilIdle();
    // If a global histogram allocator exists at this point then it likely
    // acquired histograms that will continue to point to the released
    // memory and potentially cause use-after-free memory corruption.
    DCHECK(!base::GlobalHistogramAllocator::Get());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  base::FilePath temp_dir() { return temp_dir_.path(); }
  base::FilePath metrics_file() {
    return temp_dir_.path().AppendASCII(kMetricsFilename);
  }

  FileMetricsProvider* provider() {
    if (!provider_)
      provider_.reset(new FileMetricsProvider(task_runner_, prefs()));
    return provider_.get();
  }

  void OnDidCreateMetricsLog() {
    provider()->OnDidCreateMetricsLog();
  }

  bool HasInitialStabilityMetrics() {
    return provider()->HasInitialStabilityMetrics();
  }

  void MergeHistogramDeltas() {
    provider()->MergeHistogramDeltas();
  }

  void RecordInitialHistogramSnapshots(
      base::HistogramSnapshotManager* snapshot_manager) {
    provider()->RecordInitialHistogramSnapshots(snapshot_manager);
  }

  size_t GetSnapshotHistogramCount() {
    // Merge the data from the allocator into the StatisticsRecorder.
    provider()->MergeHistogramDeltas();

    // Flatten what is known to see what has changed since the last time.
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    // "true" to the begin() includes histograms held in persistent storage.
    snapshot_manager.PrepareDeltas(
        base::StatisticsRecorder::begin(true), base::StatisticsRecorder::end(),
        base::Histogram::kNoFlags, base::Histogram::kNoFlags);
    return flattener.GetRecordedDeltaHistogramNames().size();
  }

  void CreateGlobalHistograms(int histogram_count) {
    DCHECK_GT(kMaxCreateHistograms, histogram_count);

    // Create both sparse and normal histograms in the allocator.
    created_histograms_[0] = base::SparseHistogram::FactoryGet("h0", 0);
    created_histograms_[0]->Add(0);
    for (int i = 1; i < histogram_count; ++i) {
      created_histograms_[i] = base::Histogram::FactoryGet(
          base::StringPrintf("h%d", i), 1, 100, 10, 0);
      created_histograms_[i]->Add(i);
    }
  }

  void RunTasks() {
    // Run pending tasks twice: Once for IPC calls, once for replies. Don't
    // use RunUntilIdle() because that can do more work than desired.
    task_runner_->RunPendingTasks();
    task_runner_->RunPendingTasks();
  }

  void WriteMetricsFile(const base::FilePath& path,
                        base::PersistentHistogramAllocator* metrics) {
    base::File writer(path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    // Use DCHECK so the stack-trace will indicate where this was called.
    DCHECK(writer.IsValid()) << path.value();
    size_t file_size = create_large_files_ ? metrics->size() : metrics->used();
    int written = writer.Write(0, (const char*)metrics->data(), file_size);
    DCHECK_EQ(static_cast<int>(file_size), written);
  }

  void WriteMetricsFileAtTime(const base::FilePath& path,
                              base::PersistentHistogramAllocator* metrics,
                              base::Time write_time) {
    WriteMetricsFile(path, metrics);
    base::TouchFile(path, write_time, write_time);
  }

  std::unique_ptr<base::PersistentHistogramAllocator>
  CreateMetricsFileWithHistograms(int histogram_count) {
    // Get this first so it isn't created inside the persistent allocator.
    base::GlobalHistogramAllocator::GetCreateHistogramResultHistogram();

    base::GlobalHistogramAllocator::CreateWithLocalMemory(
        create_large_files_ ? kLargeFileSize : kSmallFileSize,
        0, kMetricsName);

    CreateGlobalHistograms(histogram_count);

    std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
        base::GlobalHistogramAllocator::ReleaseForTesting();
    WriteMetricsFile(metrics_file(), histogram_allocator.get());
    return histogram_allocator;
  }

  base::HistogramBase* GetCreatedHistogram(int index) {
    DCHECK_GT(kMaxCreateHistograms, index);
    return created_histograms_[index];
  }

  const bool create_large_files_;

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle thread_task_runner_handle_;

  std::unique_ptr<base::StatisticsRecorder> statistics_recorder_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<FileMetricsProvider> provider_;
  base::HistogramBase* created_histograms_[kMaxCreateHistograms];

  DISALLOW_COPY_AND_ASSIGN(FileMetricsProviderTest);
};

// Run all test cases with both small and large files.
INSTANTIATE_TEST_CASE_P(SmallAndLargeFiles,
                        FileMetricsProviderTest,
                        testing::Bool());


TEST_P(FileMetricsProviderTest, AccessMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));

  base::Time metrics_time = base::Time::Now() - base::TimeDelta::FromMinutes(5);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      CreateMetricsFileWithHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::TouchFile(metrics_file(), metrics_time, metrics_time);

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(metrics_file(),
                             FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
                             FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
                             kMetricsName);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());

  // File should have been deleted but recreate it to test behavior should
  // the file not be deleteable by this process.
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator.get(),
                         metrics_time);

  // Second full run on the same file should produce nothing.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // Recreate the file to indicate that it is "new" and must be recorded.
  metrics_time = metrics_time + base::TimeDelta::FromMinutes(1);
  WriteMetricsFileAtTime(metrics_file(), histogram_allocator.get(),
                         metrics_time);

  // This run should again have "new" histograms.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_FALSE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessDirectory) {
  ASSERT_FALSE(PathExists(metrics_file()));

  // Get this first so it isn't created inside the persistent allocator.
  base::GlobalHistogramAllocator::GetCreateHistogramResultHistogram();

  base::GlobalHistogramAllocator::CreateWithLocalMemory(
      64 << 10, 0, kMetricsName);
  base::GlobalHistogramAllocator* allocator =
      base::GlobalHistogramAllocator::Get();
  base::HistogramBase* histogram;

  // Create files starting with a timestamp a few minutes back.
  base::Time base_time = base::Time::Now() - base::TimeDelta::FromMinutes(10);

  // Create some files in an odd order. The files are "touched" back in time to
  // ensure that each file has a later timestamp on disk than the previous one.
  base::ScopedTempDir metrics_files;
  EXPECT_TRUE(metrics_files.CreateUniqueTempDir());
  WriteMetricsFileAtTime(metrics_files.path().AppendASCII(".foo.pma"),
                         allocator, base_time);
  WriteMetricsFileAtTime(metrics_files.path().AppendASCII("_bar.pma"),
                         allocator, base_time);

  histogram = base::Histogram::FactoryGet("h1", 1, 100, 10, 0);
  histogram->Add(1);
  WriteMetricsFileAtTime(metrics_files.path().AppendASCII("a1.pma"), allocator,
                         base_time + base::TimeDelta::FromMinutes(1));

  histogram = base::Histogram::FactoryGet("h2", 1, 100, 10, 0);
  histogram->Add(2);
  WriteMetricsFileAtTime(metrics_files.path().AppendASCII("c2.pma"), allocator,
                         base_time + base::TimeDelta::FromMinutes(2));

  histogram = base::Histogram::FactoryGet("h3", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.path().AppendASCII("b3.pma"), allocator,
                         base_time + base::TimeDelta::FromMinutes(3));

  histogram = base::Histogram::FactoryGet("h4", 1, 100, 10, 0);
  histogram->Add(3);
  WriteMetricsFileAtTime(metrics_files.path().AppendASCII("d4.pma"), allocator,
                         base_time + base::TimeDelta::FromMinutes(4));

  base::TouchFile(metrics_files.path().AppendASCII("b3.pma"),
                  base_time + base::TimeDelta::FromMinutes(5),
                  base_time + base::TimeDelta::FromMinutes(5));

  WriteMetricsFileAtTime(metrics_files.path().AppendASCII("baz"), allocator,
                         base_time + base::TimeDelta::FromMinutes(6));

  // The global allocator has to be detached here so that no metrics created
  // by code called below get stored in it as that would make for potential
  // use-after-free operations if that code is called again.
  base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(metrics_files.path(),
                             FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_DIR,
                             FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
                             kMetricsName);

  // Files could come out in the order: a1, c2, d4, b3. They are recognizeable
  // by the number of histograms contained within each.
  const uint32_t expect_order[] = {1, 2, 4, 3, 0};
  for (size_t i = 0; i < arraysize(expect_order); ++i) {
    // Record embedded snapshots via snapshot-manager.
    OnDidCreateMetricsLog();
    RunTasks();
    EXPECT_EQ(expect_order[i], GetSnapshotHistogramCount()) << i;
  }

  EXPECT_FALSE(base::PathExists(metrics_files.path().AppendASCII("a1.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.path().AppendASCII("c2.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.path().AppendASCII("b3.pma")));
  EXPECT_FALSE(base::PathExists(metrics_files.path().AppendASCII("d4.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.path().AppendASCII(".foo.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.path().AppendASCII("_bar.pma")));
  EXPECT_TRUE(base::PathExists(metrics_files.path().AppendASCII("baz")));
}

TEST_P(FileMetricsProviderTest, AccessReadWriteMetrics) {
  // Create a global histogram allocator that maps to a file.
  ASSERT_FALSE(PathExists(metrics_file()));
  base::GlobalHistogramAllocator::GetCreateHistogramResultHistogram();
  base::GlobalHistogramAllocator::CreateWithFile(
      metrics_file(),
      create_large_files_ ? kLargeFileSize : kSmallFileSize,
      0, kMetricsName);
  CreateGlobalHistograms(2);
  ASSERT_TRUE(PathExists(metrics_file()));
  base::HistogramBase* h0 = GetCreatedHistogram(0);
  base::HistogramBase* h1 = GetCreatedHistogram(1);
  DCHECK(h0);
  DCHECK(h1);
  std::unique_ptr<base::PersistentHistogramAllocator> histogram_allocator =
      base::GlobalHistogramAllocator::ReleaseForTesting();

  // Register the file and allow the "checker" task to run.
  provider()->RegisterSource(metrics_file(),
                             FileMetricsProvider::SOURCE_HISTOGRAMS_ACTIVE_FILE,
                             FileMetricsProvider::ASSOCIATE_CURRENT_RUN,
                             kMetricsName);

  // Record embedded snapshots via snapshot-manager.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(2U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Make sure a second call to the snapshot-recorder doesn't break anything.
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Change a histogram and ensure that it's counted.
  h0->Add(0);
  EXPECT_EQ(1U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));

  // Change the other histogram and verify.
  h1->Add(11);
  EXPECT_EQ(1U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
}

TEST_P(FileMetricsProviderTest, AccessInitialMetrics) {
  ASSERT_FALSE(PathExists(metrics_file()));
  CreateMetricsFileWithHistograms(2);

  // Register the file and allow the "checker" task to run.
  ASSERT_TRUE(PathExists(metrics_file()));
  provider()->RegisterSource(metrics_file(),
                             FileMetricsProvider::SOURCE_HISTOGRAMS_ATOMIC_FILE,
                             FileMetricsProvider::ASSOCIATE_PREVIOUS_RUN,
                             kMetricsName);

  // Record embedded snapshots via snapshot-manager.
  HasInitialStabilityMetrics();
  RunTasks();
  {
    HistogramFlattenerDeltaRecorder flattener;
    base::HistogramSnapshotManager snapshot_manager(&flattener);
    snapshot_manager.StartDeltas();
    RecordInitialHistogramSnapshots(&snapshot_manager);
    snapshot_manager.FinishDeltas();
    EXPECT_EQ(2U, flattener.GetRecordedDeltaHistogramNames().size());
  }
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_FALSE(base::PathExists(metrics_file()));

  // A run for normal histograms should produce nothing.
  CreateMetricsFileWithHistograms(2);
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_EQ(0U, GetSnapshotHistogramCount());
  EXPECT_TRUE(base::PathExists(metrics_file()));
  OnDidCreateMetricsLog();
  RunTasks();
  EXPECT_TRUE(base::PathExists(metrics_file()));
}

}  // namespace metrics
