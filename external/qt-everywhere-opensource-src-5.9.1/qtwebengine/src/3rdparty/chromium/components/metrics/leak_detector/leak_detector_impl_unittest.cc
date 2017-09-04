// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/leak_detector_impl.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <complex>
#include <memory>
#include <new>
#include <set>
#include <vector>

#include "base/macros.h"
#include "components/metrics/leak_detector/custom_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace leak_detector {

namespace {

// Makes working with complex numbers easier.
using Complex = std::complex<double>;

using InternalLeakReport = LeakDetectorImpl::LeakReport;
using AllocationBreakdown = LeakDetectorImpl::LeakReport::AllocationBreakdown;

// The mapping location in memory for a fictional executable.
const uintptr_t kMappingAddr = 0x800000;
const size_t kMappingSize = 0x200000;

// Some call stacks within the fictional executable.
// * - outside the mapping range, e.g. JIT code.
const uintptr_t kRawStack0[] = {
    0x800100, 0x900000, 0x880080, 0x810000,
};
const uintptr_t kRawStack1[] = {
    0x940000, 0x980000,
    0xdeadbeef,  // *
    0x9a0000,
};
const uintptr_t kRawStack2[] = {
    0x8f0d00, 0x803abc, 0x9100a0,
};
const uintptr_t kRawStack3[] = {
    0x90fcde,
    0x900df00d,  // *
    0x801000,   0x880088,
    0xdeadcafe,  // *
    0x9f0000,   0x8700a0, 0x96037c,
};
const uintptr_t kRawStack4[] = {
    0x8c0000, 0x85d00d, 0x921337,
    0x780000,  // *
};
const uintptr_t kRawStack5[] = {
    0x990000, 0x888888, 0x830ac0, 0x8e0000,
    0xc00000,  // *
};

// This struct makes it easier to pass call stack info to
// LeakDetectorImplTest::Alloc().
struct TestCallStack {
  const uintptr_t* stack;  // A reference to the original stack data.
  size_t depth;
};

const TestCallStack kStack0 = {kRawStack0, arraysize(kRawStack0)};
const TestCallStack kStack1 = {kRawStack1, arraysize(kRawStack1)};
const TestCallStack kStack2 = {kRawStack2, arraysize(kRawStack2)};
const TestCallStack kStack3 = {kRawStack3, arraysize(kRawStack3)};
const TestCallStack kStack4 = {kRawStack4, arraysize(kRawStack4)};
const TestCallStack kStack5 = {kRawStack5, arraysize(kRawStack5)};

// The interval between consecutive analyses (LeakDetectorImpl::TestForLeaks),
// in number of bytes allocated. e.g. if |kAllocedSizeAnalysisInterval| = 1024
// then call TestForLeaks() every 1024 bytes of allocation that occur.
const size_t kAllocedSizeAnalysisInterval = 8192;

// Suspicion thresholds used by LeakDetectorImpl for size and call stacks.
const uint32_t kSizeSuspicionThreshold = 4;
const uint32_t kCallStackSuspicionThreshold = 4;

// Because it takes N+1 analyses to reach a suspicion threshold of N (the
// suspicion score is only calculated based on deltas from the previous
// analysis), the actual number of analyses it takes to generate a report for
// the first time is:
const uint32_t kMinNumAnalysesToGenerateReport =
    kSizeSuspicionThreshold + 1 + kCallStackSuspicionThreshold + 1;

// Returns the offset within [kMappingAddr, kMappingAddr + kMappingSize) if
// |addr| falls in that range. Otherwise, returns |UINTPTR_MAX|.
uintptr_t GetOffsetInMapping(uintptr_t addr) {
  if (addr >= kMappingAddr && addr < kMappingAddr + kMappingSize)
    return addr - kMappingAddr;
  return UINTPTR_MAX;
}

// Copied from leak_detector_impl.cc. Converts a size to a size class index.
// Any size in the range [index * 4, index * 4 + 3] falls into that size class.
uint32_t SizeToIndex(size_t size) {
  return size / sizeof(uint32_t);
}

// Returns true if the |alloc_breakdown_history_| field of the two LeakReports
// |a| and |b| are the same.
bool CompareReportAllocHistory(const InternalLeakReport& a,
                               const InternalLeakReport& b) {
  auto alloc_breakdown_compare_func = [](AllocationBreakdown a,
                                         AllocationBreakdown b) -> bool {
    return std::equal(a.counts_by_size.begin(), a.counts_by_size.end(),
                      b.counts_by_size.begin()) &&
           a.count_for_call_stack == b.count_for_call_stack;
  };
  return std::equal(
      a.alloc_breakdown_history().begin(), a.alloc_breakdown_history().end(),
      b.alloc_breakdown_history().begin(), alloc_breakdown_compare_func);
}

}  // namespace

// This test suite will test the ability of LeakDetectorImpl to catch leaks in
// a program. Individual tests can run leaky code locally.
//
// The leaky code must call Alloc() and Free() for heap memory management. It
// should not call See comments on those
// functions for more details.
class LeakDetectorImplTest : public ::testing::Test {
 public:
  LeakDetectorImplTest()
      : total_num_allocs_(0),
        total_num_frees_(0),
        total_alloced_size_(0),
        next_analysis_total_alloced_size_(kAllocedSizeAnalysisInterval),
        num_reports_generated_(0) {}

  void SetUp() override {
    CustomAllocator::Initialize();

    detector_.reset(new LeakDetectorImpl(kMappingAddr, kMappingSize,
                                         kSizeSuspicionThreshold,
                                         kCallStackSuspicionThreshold));
  }

  void TearDown() override {
    // Free any memory that was leaked by test cases. Do not use Free() because
    // that will try to modify |alloced_ptrs_|.
    for (void* ptr : alloced_ptrs_)
      delete[] reinterpret_cast<char*>(ptr);
    alloced_ptrs_.clear();

    // Must destroy all objects that use CustomAllocator before shutting down.
    detector_.reset();
    stored_reports_.clear();

    EXPECT_TRUE(CustomAllocator::Shutdown());
  }

 protected:
  template <typename T>
  using InternalVector = LeakDetectorImpl::InternalVector<T>;

  // Alloc and free functions that allocate and free heap memory and
  // automatically pass alloc/free info to |detector_|. They emulate the
  // alloc/free hook functions that would call into LeakDetectorImpl in
  // real-life usage. They also keep track of individual allocations locally, so
  // any leaked memory could be cleaned up.
  //
  // |stack| is just a nominal call stack object to identify the call site. It
  // doesn't have to contain the stack trace of the actual call stack.
  void* Alloc(size_t size, const TestCallStack& stack) {
    void* ptr = new char[size];
    detector_->RecordAlloc(ptr, size, stack.depth,
                           reinterpret_cast<const void* const*>(stack.stack));

    EXPECT_TRUE(alloced_ptrs_.find(ptr) == alloced_ptrs_.end());
    alloced_ptrs_.insert(ptr);

    ++total_num_allocs_;
    total_alloced_size_ += size;
    if (total_alloced_size_ >= next_analysis_total_alloced_size_) {
      InternalVector<InternalLeakReport> reports;
      detector_->TestForLeaks(&reports, 1024);
      for (const InternalLeakReport& report : reports) {
        auto iter = stored_reports_.find(report);
        if (iter == stored_reports_.end()) {
          stored_reports_.insert(report);
        } else {
          // InternalLeakReports are uniquely identified by |alloc_size_bytes_|
          // and |call_stack_|. See InternalLeakReport::operator<().
          // If a report with the same size and call stack already exists,
          // overwrite it with the new report, which has a newer history.
          stored_reports_.erase(iter);
          stored_reports_.insert(report);
        }
      }
      num_reports_generated_ += reports.size();

      // Determine when the next leak analysis should occur.
      while (total_alloced_size_ >= next_analysis_total_alloced_size_)
        next_analysis_total_alloced_size_ += kAllocedSizeAnalysisInterval;
    }
    return ptr;
  }

  // See comment for Alloc().
  void Free(void* ptr) {
    auto find_ptr_iter = alloced_ptrs_.find(ptr);
    EXPECT_FALSE(find_ptr_iter == alloced_ptrs_.end());
    if (find_ptr_iter == alloced_ptrs_.end())
      return;
    alloced_ptrs_.erase(find_ptr_iter);
    ++total_num_frees_;

    detector_->RecordFree(ptr);

    delete[] reinterpret_cast<char*>(ptr);
  }

  // TEST CASE: Simple program that leaks memory regularly. Pass in
  // enable_leaks=true to trigger some memory leaks.
  void SimpleLeakyFunction(bool enable_leaks);

  // TEST CASE: Julia set fractal computation. Pass in enable_leaks=true to
  // trigger some memory leaks.
  void JuliaSet(bool enable_leaks);

  // Instance of the class being tested.
  std::unique_ptr<LeakDetectorImpl> detector_;

  // Number of pointers allocated and freed so far.
  size_t total_num_allocs_;
  size_t total_num_frees_;

  // Keeps count of total size allocated by Alloc().
  size_t total_alloced_size_;

  // The cumulative allocation size at which to trigger the TestForLeaks() call.
  size_t next_analysis_total_alloced_size_;

  // Stores all pointers to memory allocated by by Alloc() so we can manually
  // free the leaked pointers at the end. This also serves as redundant
  // bookkeepping: it stores all pointers that have been allocated but not yet
  // freed.
  std::set<void*> alloced_ptrs_;

  // Store leak reports here. Use a set so duplicate reports are not stored.
  std::set<InternalLeakReport> stored_reports_;

  // Keeps track of the actual number of reports (duplicate or not) that were
  // generated by |detector_|.
  size_t num_reports_generated_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LeakDetectorImplTest);
};

void LeakDetectorImplTest::SimpleLeakyFunction(bool enable_leaks) {
  std::vector<uint32_t*> ptrs(7);

  const int kNumOuterIterations = 32;
  for (int j = 0; j < kNumOuterIterations; ++j) {
    // The inner loop allocates 256 bytes. Run it 32 times so that 8192 bytes
    // (|kAllocedSizeAnalysisInterval|) are allocated for each iteration of the
    // outer loop.
    const int kNumInnerIterations = 32;
    static_assert(kNumInnerIterations * 256 == kAllocedSizeAnalysisInterval,
                  "Inner loop iterations do not allocate the correct number of "
                  "bytes.");
    for (int i = 0; i < kNumInnerIterations; ++i) {
      size_t alloc_size_at_beginning = total_alloced_size_;

      ptrs[0] = new(Alloc(16, kStack0)) uint32_t;
      ptrs[1] = new(Alloc(32, kStack1)) uint32_t;
      ptrs[2] = new(Alloc(48, kStack2)) uint32_t;
      // Allocate two 32-byte blocks and record them as from the same call site.
      ptrs[3] = new(Alloc(32, kStack3)) uint32_t;
      ptrs[4] = new(Alloc(32, kStack3)) uint32_t;
      // Allocate two 48-byte blocks and record them as from the same call site.
      ptrs[5] = new(Alloc(48, kStack4)) uint32_t;
      ptrs[6] = new(Alloc(48, kStack4)) uint32_t;

      // Now free these pointers.
      Free(ptrs[0]);
      if (!enable_leaks)  // Leak with size=32, call_stack=kStack1.
        Free(ptrs[1]);
      if (!enable_leaks)  // Leak with size=48, call_stack=kStack2.
        Free(ptrs[2]);
      Free(ptrs[3]);
      Free(ptrs[4]);
      Free(ptrs[5]);
      Free(ptrs[6]);

      // Make sure that the above code actually allocates 256 bytes.
      EXPECT_EQ(alloc_size_at_beginning + 256, total_alloced_size_);
    }
  }
}

void LeakDetectorImplTest::JuliaSet(bool enable_leaks) {
  // The center region of the complex plane that is the basis for our Julia set
  // computations is a circle of radius kRadius.
  constexpr double kRadius = 2;

  // To track points in the complex plane, we will use a rectangular grid in the
  // range defined by [-kRadius, kRadius] along both axes.
  constexpr double kRangeMin = -kRadius;
  constexpr double kRangeMax = kRadius;

  // Divide each axis into intervals, each of which is associated with a point
  // on that axis at its center.
  constexpr double kIntervalInverse = 64;
  constexpr double kInterval = 1.0 / kIntervalInverse;
  constexpr int kNumPoints = (kRangeMax - kRangeMin) / kInterval + 1;

  // Contains some useful functions for converting between points on the complex
  // plane and in a gridlike data structure.
  struct ComplexPlane {
    static int GetXGridIndex(const Complex& value) {
      return (value.real() + kInterval / 2 - kRangeMin) / kInterval;
    }
    static int GetYGridIndex(const Complex& value) {
      return (value.imag() + kInterval / 2 - kRangeMin) / kInterval;
    }
    static int GetArrayIndex(const Complex& value) {
      return GetXGridIndex(value) + GetYGridIndex(value) * kNumPoints;
    }
    static Complex GetComplexForGridPoint(size_t x, size_t y) {
      return Complex(kRangeMin + x * kInterval, kRangeMin + y * kInterval);
    }
  };

  // Make sure the choice of interval doesn't result in any loss of precision.
  ASSERT_EQ(1.0, kInterval * kIntervalInverse);

  // Create a grid for part of the complex plane, with each axis within the
  // range [kRangeMin, kRangeMax].
  constexpr size_t width = kNumPoints;
  constexpr size_t height = kNumPoints;
  std::vector<Complex*> grid(width * height);

  // Initialize an object for each point within the inner circle |z| < kRadius.
  for (size_t i = 0; i < width; ++i) {
    for (size_t j = 0; j < height; ++j) {
      Complex point = ComplexPlane::GetComplexForGridPoint(i, j);
      // Do not store any values outside the inner circle.
      if (abs(point) <= kRadius) {
        grid[i + j * width] =
            new (Alloc(sizeof(Complex), kStack0)) Complex(point);
      }
    }
  }
  EXPECT_LE(alloced_ptrs_.size(), width * height);

  // Create a new grid for the result of the transformation.
  std::vector<Complex*> next_grid(width * height, nullptr);

  // Number of times to run the Julia set iteration. This is not the same as the
  // number of analyses performed by LeakDetectorImpl, which is determined by
  // the total number of bytes allocated divided by
  // |kAllocedSizeAnalysisInterval|.
  const int kNumIterations = 20;
  for (int n = 0; n < kNumIterations; ++n) {
    for (int i = 0; i < kNumPoints; ++i) {
      for (int j = 0; j < kNumPoints; ++j) {
        if (!grid[i + j * width])
          continue;

        // NOTE: The below code is NOT an efficient way to compute a Julia set.
        // This is only to test the leak detector with some nontrivial code.

        // A simple polynomial function for generating Julia sets is:
        //   f(z) = z^n + c

        // But in this algorithm, we need the inverse:
        //   fInv(z) = (z - c)^(1/n)

        // Here, let's use n=5 and c=0.544.
        const Complex c(0.544, 0);
        const Complex& z = *grid[i + j * width];

        // This is the principal root.
        Complex root = pow(z - c, 0.2);

        // Discard the result if it is too far out from the center of the plane.
        if (abs(root) > kRadius)
          continue;

        // The below code only allocates Complex objects of the same size. The
        // leak detector expects various sizes, so increase the allocation size
        // by a different amount at each call site.

        // Nth root produces N results.
        // Place all root results on |next_grid|.

        // First, place the principal root.
        if (!next_grid[ComplexPlane::GetArrayIndex(root)]) {
          next_grid[ComplexPlane::GetArrayIndex(root)] =
              new (Alloc(sizeof(Complex) + 24, kStack1)) Complex(root);
        }

        double magnitude = abs(root);
        double angle = arg(root);
        // To generate other roots, rotate the principal root by increments of
        // 1/N of a full circle.
        const double kAngleIncrement = M_PI * 2 / 5;

        // Second root.
        root = std::polar(magnitude, angle + kAngleIncrement);
        if (!next_grid[ComplexPlane::GetArrayIndex(root)]) {
          next_grid[ComplexPlane::GetArrayIndex(root)] =
              new (Alloc(sizeof(Complex) + 40, kStack2)) Complex(root);
        }

        // In some of the sections below, setting |enable_leaks| to true will
        // trigger a memory leak by overwriting the old Complex pointer value
        // without freeing it. Due to the nature of complex roots being confined
        // to equal sections of the complex plane, each new pointer will
        // displace an old pointer that was allocated from the same line of
        // code.

        // Third root.
        root = std::polar(magnitude, angle + kAngleIncrement * 2);
        // *** LEAK ***
        if (enable_leaks || !next_grid[ComplexPlane::GetArrayIndex(root)]) {
          next_grid[ComplexPlane::GetArrayIndex(root)] =
              new (Alloc(sizeof(Complex) + 40, kStack3)) Complex(root);
        }

        // Fourth root.
        root = std::polar(magnitude, angle + kAngleIncrement * 3);
        // *** LEAK ***
        if (enable_leaks || !next_grid[ComplexPlane::GetArrayIndex(root)]) {
          next_grid[ComplexPlane::GetArrayIndex(root)] =
              new (Alloc(sizeof(Complex) + 52, kStack4)) Complex(root);
        }

        // Fifth root.
        root = std::polar(magnitude, angle + kAngleIncrement * 4);
        if (!next_grid[ComplexPlane::GetArrayIndex(root)]) {
          next_grid[ComplexPlane::GetArrayIndex(root)] =
              new (Alloc(sizeof(Complex) + 52, kStack5)) Complex(root);
        }
      }
    }

    // Clear the previously allocated points.
    for (Complex*& point : grid) {
      if (point) {
        Free(point);
        point = nullptr;
      }
    }

    // Now swap the two grids for the next iteration.
    grid.swap(next_grid);
  }

  // Clear the previously allocated points.
  for (Complex*& point : grid) {
    if (point) {
      Free(point);
      point = nullptr;
    }
  }
}

TEST_F(LeakDetectorImplTest, CheckTestFramework) {
  EXPECT_EQ(0U, total_num_allocs_);
  EXPECT_EQ(0U, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());

  // Allocate some memory.
  void* ptr0 = Alloc(12, kStack0);
  void* ptr1 = Alloc(16, kStack0);
  void* ptr2 = Alloc(24, kStack0);
  EXPECT_EQ(3U, total_num_allocs_);
  EXPECT_EQ(0U, total_num_frees_);
  EXPECT_EQ(3U, alloced_ptrs_.size());

  // Free one of the pointers.
  Free(ptr1);
  EXPECT_EQ(3U, total_num_allocs_);
  EXPECT_EQ(1U, total_num_frees_);
  EXPECT_EQ(2U, alloced_ptrs_.size());

  // Allocate some more memory.
  void* ptr3 = Alloc(72, kStack1);
  void* ptr4 = Alloc(104, kStack1);
  void* ptr5 = Alloc(96, kStack1);
  void* ptr6 = Alloc(24, kStack1);
  EXPECT_EQ(7U, total_num_allocs_);
  EXPECT_EQ(1U, total_num_frees_);
  EXPECT_EQ(6U, alloced_ptrs_.size());

  // Free more pointers.
  Free(ptr2);
  Free(ptr4);
  Free(ptr6);
  EXPECT_EQ(7U, total_num_allocs_);
  EXPECT_EQ(4U, total_num_frees_);
  EXPECT_EQ(3U, alloced_ptrs_.size());

  // Free remaining memory.
  Free(ptr0);
  Free(ptr3);
  Free(ptr5);
  EXPECT_EQ(7U, total_num_allocs_);
  EXPECT_EQ(7U, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());
}

TEST_F(LeakDetectorImplTest, SimpleLeakyFunctionNoLeak) {
  SimpleLeakyFunction(false /* enable_leaks */);

  // SimpleLeakyFunction() should have run cleanly without leaking.
  EXPECT_EQ(total_num_allocs_, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());
  EXPECT_EQ(0U, num_reports_generated_);
  EXPECT_EQ(0U, stored_reports_.size());
}

TEST_F(LeakDetectorImplTest, SimpleLeakyFunctionWithLeak) {
  SimpleLeakyFunction(true /* enable_leaks */);

  // SimpleLeakyFunction() should generated some leak reports.
  EXPECT_GT(total_num_allocs_, total_num_frees_);
  EXPECT_GT(alloced_ptrs_.size(), 0U);
  EXPECT_EQ(2U, num_reports_generated_);
  ASSERT_EQ(2U, stored_reports_.size());

  // The reports should be stored in order of size.

  // |report1| comes from the call site marked with kStack1, with size=32.
  const InternalLeakReport& report1 = *stored_reports_.begin();
  EXPECT_EQ(32U, report1.alloc_size_bytes());
  ASSERT_EQ(kStack1.depth, report1.call_stack().size());
  for (size_t i = 0; i < kStack1.depth; ++i) {
    EXPECT_EQ(GetOffsetInMapping(kStack1.stack[i]),
              report1.call_stack()[i]) << i;
  }

  // |report2| comes from the call site marked with kStack2, with size=48.
  const InternalLeakReport& report2 = *(++stored_reports_.begin());
  EXPECT_EQ(48U, report2.alloc_size_bytes());
  ASSERT_EQ(kStack2.depth, report2.call_stack().size());
  for (size_t i = 0; i < kStack2.depth; ++i) {
    EXPECT_EQ(GetOffsetInMapping(kStack2.stack[i]),
              report2.call_stack()[i]) << i;
  }

  // Check historical data recorded in the reports.
  // - Each inner loop iteration allocates a net of 1x 32 bytes and 1x 48 bytes.
  // - Each outer loop iteration allocates a net of 32x 32 bytes and 32x 48
  //   bytes.
  // - However, the leak analysis happens after the allocs but before the frees
  //   that come right after. So it should count the two extra allocs made at
  //   call sites |kStack3| and |kStack4|. The formula is |(i + 1) * 32 + 2|,
  //   where |i| is the iteration index.
  // - It takes |kMinNumAnalysesToGenerateReport| analyses for the first report
  //   to be generated. Subsequent analyises do not generate reports due to the
  //   cooldown mechanism.

  const auto& report1_history = report1.alloc_breakdown_history();
  EXPECT_EQ(kMinNumAnalysesToGenerateReport, report1_history.size());

  for (size_t i = 0; i < report1_history.size(); ++i) {
    const AllocationBreakdown& entry = report1_history[i];

    const InternalVector<uint32_t>& counts_by_size = entry.counts_by_size;
    ASSERT_GT(counts_by_size.size(), SizeToIndex(48));

    // Check the two leaky sizes, 32 and 48.
    uint32_t expected_leaky_count = (i + 1) * 32 + 2;
    EXPECT_EQ(expected_leaky_count, counts_by_size[SizeToIndex(32)]);
    EXPECT_EQ(expected_leaky_count, counts_by_size[SizeToIndex(48)]);

    // Not related to the leaks, but there should be a dangling 16-byte
    // allocation during each leak analysis, because it hasn't yet been freed.
    EXPECT_EQ(1U, counts_by_size[SizeToIndex(16)]);
  }

  // Check call site count over time.
  ASSERT_LT(kSizeSuspicionThreshold, report1_history.size());
  // Initially, there has been no call site tracking.
  for (size_t i = 0; i < kSizeSuspicionThreshold; ++i)
    EXPECT_EQ(0U, report1_history[i].count_for_call_stack);

  // Once |kSizeSuspicionThreshold| has been reached and call site tracking has
  // begun, the number of allocations for the suspected call site should
  // increase by 32 each frame. See comments above.
  uint32_t expected_call_stack_count = 0;
  for (size_t i = kSizeSuspicionThreshold; i < report1_history.size(); ++i) {
    EXPECT_EQ(expected_call_stack_count,
              report1_history[i].count_for_call_stack);
    expected_call_stack_count += 32;
  }

  // |report2| should have the same size history and call stack history as
  // |report1|.
  EXPECT_TRUE(CompareReportAllocHistory(report1, report2));
}

TEST_F(LeakDetectorImplTest, SimpleLeakyFunctionWithLeakThreeTimes) {
  // Run three iterations of the leaky function.
  SimpleLeakyFunction(true /* enable_leaks */);
  SimpleLeakyFunction(true /* enable_leaks */);
  SimpleLeakyFunction(true /* enable_leaks */);

  // SimpleLeakyFunction() should have generated three times as many leak
  // reports, because the number of iterations is the same as the cooldown of
  // LeakDetectorImpl. But the number of unique reports stored is still two.
  EXPECT_EQ(6U, num_reports_generated_);
  ASSERT_EQ(2U, stored_reports_.size());

  // The reports should be stored in order of size.

  // |report1| comes from the call site marked with kStack1, with size=32.
  const InternalLeakReport& report1 = *stored_reports_.begin();
  EXPECT_EQ(32U, report1.alloc_size_bytes());
  ASSERT_EQ(kStack1.depth, report1.call_stack().size());
  for (size_t i = 0; i < kStack1.depth; ++i) {
    EXPECT_EQ(GetOffsetInMapping(kStack1.stack[i]), report1.call_stack()[i])
        << i;
  }

  // |report2| comes from the call site marked with kStack2, with size=48.
  const InternalLeakReport& report2 = *(++stored_reports_.begin());
  EXPECT_EQ(48U, report2.alloc_size_bytes());
  ASSERT_EQ(kStack2.depth, report2.call_stack().size());
  for (size_t i = 0; i < kStack2.depth; ++i) {
    EXPECT_EQ(GetOffsetInMapping(kStack2.stack[i]), report2.call_stack()[i])
        << i;
  }

  const auto& report1_history = report1.alloc_breakdown_history();
  EXPECT_EQ(32U, report1_history.size());

  for (size_t i = 1; i < report1_history.size(); ++i) {
    const InternalVector<uint32_t>& counts_by_size =
        report1_history[i].counts_by_size;
    const InternalVector<uint32_t>& prev_counts_by_size =
        report1_history[i - 1].counts_by_size;
    ASSERT_GT(counts_by_size.size(), SizeToIndex(48));

    // Check the two leaky sizes, 32 and 48. At this point, the exact counts
    // could be computed but the computations are too complex for a unit test.
    // Instead, check that the counts increase by 32 from the previous count.
    // Same goes for checking call site counts later.
    EXPECT_GT(counts_by_size[SizeToIndex(32)], 0U);
    EXPECT_GT(counts_by_size[SizeToIndex(48)], 0U);
    EXPECT_EQ(prev_counts_by_size[SizeToIndex(32)] + 32,
              counts_by_size[SizeToIndex(32)]);
    EXPECT_EQ(prev_counts_by_size[SizeToIndex(48)] + 32,
              counts_by_size[SizeToIndex(48)]);

    // Not related to the leaks, but there should be a dangling 16-byte
    // allocation during each leak analysis, because it hasn't yet been freed.
    EXPECT_EQ(1U, counts_by_size[SizeToIndex(16)]);
  }

  // Check call site count over time.
  ASSERT_LT(kSizeSuspicionThreshold, report1_history.size());
  // Sufficient time has passed since the first report was generated. The entire
  // alloc history should contain call site counts.
  for (size_t i = 1; i < report1_history.size(); ++i) {
    EXPECT_GT(report1_history[i].count_for_call_stack, 0U);
    EXPECT_EQ(report1_history[i - 1].count_for_call_stack + 32,
              report1_history[i].count_for_call_stack);
  }

  // |report2| should have the same size history and call stack history as
  // |report1|.
  EXPECT_TRUE(CompareReportAllocHistory(report1, report2));
}

TEST_F(LeakDetectorImplTest, JuliaSetNoLeak) {
  JuliaSet(false /* enable_leaks */);

  // JuliaSet() should have run cleanly without leaking.
  EXPECT_EQ(total_num_allocs_, total_num_frees_);
  EXPECT_EQ(0U, alloced_ptrs_.size());
  EXPECT_EQ(0U, num_reports_generated_);
  ASSERT_EQ(0U, stored_reports_.size());
}

TEST_F(LeakDetectorImplTest, JuliaSetWithLeak) {
  JuliaSet(true /* enable_leaks */);

  // JuliaSet() should have leaked some memory from two call sites.
  EXPECT_GT(total_num_allocs_, total_num_frees_);
  EXPECT_GT(alloced_ptrs_.size(), 0U);
  EXPECT_GT(num_reports_generated_, 0U);

  // There should be one unique leak report generated for each leaky call site.
  ASSERT_EQ(2U, stored_reports_.size());

  // The reports should be stored in order of size.

  // |report1| comes from the call site in JuliaSet() corresponding to
  // |kStack3|.
  const InternalLeakReport& report1 = *stored_reports_.begin();
  EXPECT_EQ(sizeof(Complex) + 40, report1.alloc_size_bytes());
  ASSERT_EQ(kStack3.depth, report1.call_stack().size());
  for (size_t i = 0; i < kStack3.depth; ++i) {
    EXPECT_EQ(GetOffsetInMapping(kStack3.stack[i]),
              report1.call_stack()[i]) << i;
  }

  // |report2| comes from the call site in JuliaSet() corresponding to
  // |kStack4|.
  const InternalLeakReport& report2 = *(++stored_reports_.begin());
  EXPECT_EQ(sizeof(Complex) + 52, report2.alloc_size_bytes());
  ASSERT_EQ(kStack4.depth, report2.call_stack().size());
  for (size_t i = 0; i < kStack4.depth; ++i) {
    EXPECT_EQ(GetOffsetInMapping(kStack4.stack[i]),
              report2.call_stack()[i]) << i;
  }

  // Check |report1|'s historical data.
  const auto& report1_history = report1.alloc_breakdown_history();
  // Computing the exact number of leak analyses is not trivial, but we know it
  // must be at least |kSizeSuspicionThreshold + kCallStackSuspicionThreshold|
  // in order to have generated a report.
  EXPECT_GT(report1_history.size(),
            kSizeSuspicionThreshold + kCallStackSuspicionThreshold);

  // Make sure that the final allocation counts for the leaky sizes are larger
  // than that of the non-leaky size by at least an order of magnitude.
  const AllocationBreakdown& final_entry = *report1_history.rbegin();
  const InternalVector<uint32_t>& counts_by_size = final_entry.counts_by_size;
  uint32_t size_0_index = SizeToIndex(sizeof(Complex) + 24);
  uint32_t size_1_index = SizeToIndex(sizeof(Complex) + 40);
  uint32_t size_2_index = SizeToIndex(sizeof(Complex) + 52);
  ASSERT_LT(size_0_index, counts_by_size.size());
  ASSERT_LT(size_1_index, counts_by_size.size());
  ASSERT_LT(size_2_index, counts_by_size.size());

  EXPECT_GT(counts_by_size[size_1_index], counts_by_size[size_0_index] * 10);
  EXPECT_GT(counts_by_size[size_2_index], counts_by_size[size_0_index] * 10);

  // |report1| and |report2| do not necessarily have the same allocation history
  // due to the different rates at which they were generated.
}

}  // namespace leak_detector
}  // namespace metrics
