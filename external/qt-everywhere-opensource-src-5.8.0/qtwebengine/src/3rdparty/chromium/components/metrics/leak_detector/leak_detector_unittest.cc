// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector.h"

#include <algorithm>
#include <set>

#include "base/allocator/allocator_extension.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/proto/memory_leak_report.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Default values for LeakDetector params. See header file for the meaning of
// each parameter.
const float kDefaultSamplingRate = 1.0f;
const size_t kDefaultMaxCallStackUnwindDepth = 4;
const uint64_t kDefaultAnalysisIntervalBytes = 32 * 1024 * 1024;  // 32 MiB.
const uint32_t kDefaultSizeSuspicionThreshold = 4;
const uint32_t kDefaultCallStackSuspicionThreshold = 4;

// Observer class that receives leak reports and stores them in |reports_|.
// Only one copy of each unique report will be stored.
class TestObserver : public LeakDetector::Observer {
 public:
  // Contains a comparator function used to compare MemoryLeakReportProtos for
  // uniqueness.
  struct Comparator {
    bool operator()(const MemoryLeakReportProto& a,
                    const MemoryLeakReportProto& b) const {
      if (a.size_bytes() != b.size_bytes())
        return a.size_bytes() < b.size_bytes();

      return std::lexicographical_compare(a.call_stack().begin(),
                                          a.call_stack().end(),
                                          b.call_stack().begin(),
                                          b.call_stack().end());
    }
  };

  TestObserver() {}

  void OnLeaksFound(
      const std::vector<MemoryLeakReportProto>& reports) override {
    reports_.insert(reports.begin(), reports.end());
  }

  const std::set<MemoryLeakReportProto, Comparator>& reports() const {
    return reports_;
  }

 private:
  // Container for all leak reports received through OnLeaksFound(). Stores only
  // one copy of each unique report.
  std::set<MemoryLeakReportProto, Comparator> reports_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class LeakDetectorTest : public ::testing::Test {
 public:
  LeakDetectorTest() : detector_(LeakDetector::GetInstance()) {
    MemoryLeakReportProto::Params params;
    params.set_sampling_rate(kDefaultSamplingRate);
    params.set_max_stack_depth(kDefaultMaxCallStackUnwindDepth);
    params.set_analysis_interval_bytes(kDefaultAnalysisIntervalBytes);
    params.set_size_suspicion_threshold(kDefaultSizeSuspicionThreshold);
    params.set_call_stack_suspicion_threshold(
        kDefaultCallStackSuspicionThreshold);

    EXPECT_TRUE(base::ThreadTaskRunnerHandle::IsSet());
    detector_->Init(params, base::ThreadTaskRunnerHandle::Get());
  }

 protected:
  // Points to the instance of LeakDetector returned by GetInstance().
  LeakDetector* detector_;

 private:
  // For supporting content::BrowserThread operations.
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(LeakDetectorTest);
};

TEST_F(LeakDetectorTest, NotifyObservers) {
  // Generate two sets of leak reports.
  std::vector<MemoryLeakReportProto> reports1(3);
  reports1[0].set_size_bytes(8);
  for (uint64_t entry : {1, 2, 3, 4}) {
    reports1[0].add_call_stack(entry);
  }
  reports1[1].set_size_bytes(16);
  for (uint64_t entry : {5, 6, 7, 8}) {
    reports1[1].add_call_stack(entry);
  }
  reports1[2].set_size_bytes(24);
  for (uint64_t entry : {9, 10, 11, 12}) {
    reports1[1].add_call_stack(entry);
  }

  std::vector<MemoryLeakReportProto> reports2(3);
  reports2[0].set_size_bytes(32);
  for (uint64_t entry : {1, 2, 4, 8}) {
    reports2[0].add_call_stack(entry);
  }
  reports2[1].set_size_bytes(40);
  for (uint64_t entry : {16, 32, 64, 128}) {
    reports2[1].add_call_stack(entry);
  }
  reports2[2].set_size_bytes(48);
  for (uint64_t entry : {256, 512, 1024, 2048}) {
    reports2[2].add_call_stack(entry);
  }

  // Register three observers;
  TestObserver obs1, obs2, obs3;
  detector_->AddObserver(&obs1);
  detector_->AddObserver(&obs2);
  detector_->AddObserver(&obs3);

  // Pass both sets of reports to the leak detector.
  detector_->NotifyObservers(reports1);
  detector_->NotifyObservers(reports2);

  // Check that all three observers got both sets of reports, passed in
  // separately.
  for (const TestObserver* obs : {&obs1, &obs2, &obs3}) {
    EXPECT_EQ(6U, obs->reports().size());
    for (const auto& report : {reports1[0], reports1[1], reports1[2],
                               reports2[0], reports2[1], reports2[2]}) {
      EXPECT_TRUE(obs->reports().find(report) != obs->reports().end());
    }
  }
}

}  // namespace metrics
