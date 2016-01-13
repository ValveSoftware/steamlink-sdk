// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time/time.h"
#include "media/base/sinc_resampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterations = 50000000;

static const double kSampleRateRatio = 192000.0 / 44100.0;
static const double kKernelInterpolationFactor = 0.5;

// Helper function to provide no input to SincResampler's Convolve benchmark.
static void DoNothing(int frames, float* destination) {}

// Define platform independent function name for Convolve* tests.
#if defined(ARCH_CPU_X86_FAMILY)
#define CONVOLVE_FUNC Convolve_SSE
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(USE_NEON)
#define CONVOLVE_FUNC Convolve_NEON
#endif

static void RunConvolveBenchmark(
    SincResampler* resampler,
    float (*convolve_fn)(const float*, const float*, const float*, double),
    bool aligned,
    const std::string& trace_name) {
  base::TimeTicks start = base::TimeTicks::HighResNow();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    convolve_fn(resampler->get_kernel_for_testing() + (aligned ? 0 : 1),
                resampler->get_kernel_for_testing(),
                resampler->get_kernel_for_testing(),
                kKernelInterpolationFactor);
  }
  double total_time_milliseconds =
      (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  perf_test::PrintResult("sinc_resampler_convolve",
                         "",
                         trace_name,
                         kBenchmarkIterations / total_time_milliseconds,
                         "runs/ms",
                         true);
}

// Benchmark for the various Convolve() methods.  Make sure to build with
// branding=Chrome so that DCHECKs are compiled out when benchmarking.
TEST(SincResamplerPerfTest, Convolve) {
  SincResampler resampler(kSampleRateRatio,
                          SincResampler::kDefaultRequestSize,
                          base::Bind(&DoNothing));

  RunConvolveBenchmark(
      &resampler, SincResampler::Convolve_C, true, "unoptimized_aligned");

#if defined(CONVOLVE_FUNC)
  RunConvolveBenchmark(
      &resampler, SincResampler::CONVOLVE_FUNC, true, "optimized_aligned");
  RunConvolveBenchmark(
      &resampler, SincResampler::CONVOLVE_FUNC, false, "optimized_unaligned");
#endif
}

#undef CONVOLVE_FUNC

} // namespace media
