// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterations = 20;

template <typename T>
void RunInterleaveBench(AudioBus* bus, const std::string& trace_name) {
  const int frame_size = bus->frames() * bus->channels();
  std::unique_ptr<T[]> interleaved(new T[frame_size]);
  const int byte_size = sizeof(T);

  base::TimeTicks start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    bus->ToInterleaved(bus->frames(), byte_size, interleaved.get());
  }
  double total_time_milliseconds =
      (base::TimeTicks::Now() - start).InMillisecondsF();
  perf_test::PrintResult(
      "audio_bus_to_interleaved", "", trace_name,
      total_time_milliseconds / kBenchmarkIterations, "ms", true);

  start = base::TimeTicks::Now();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    bus->FromInterleaved(interleaved.get(), bus->frames(), byte_size);
  }
  total_time_milliseconds =
      (base::TimeTicks::Now() - start).InMillisecondsF();
  perf_test::PrintResult(
      "audio_bus_from_interleaved", "", trace_name,
      total_time_milliseconds / kBenchmarkIterations, "ms", true);
}

// Benchmark the FromInterleaved() and ToInterleaved() methods.
TEST(AudioBusPerfTest, Interleave) {
  std::unique_ptr<AudioBus> bus = AudioBus::Create(2, 48000 * 120);
  FakeAudioRenderCallback callback(0.2);
  callback.Render(bus.get(), 0, 0);

  RunInterleaveBench<int8_t>(bus.get(), "int8_t");
  RunInterleaveBench<int16_t>(bus.get(), "int16_t");
  RunInterleaveBench<int32_t>(bus.get(), "int32_t");
}

} // namespace media
