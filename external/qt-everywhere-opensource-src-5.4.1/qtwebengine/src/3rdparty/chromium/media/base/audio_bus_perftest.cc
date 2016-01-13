// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  scoped_ptr<T> interleaved(new T[frame_size]);
  const int byte_size = sizeof(*interleaved);

  base::TimeTicks start = base::TimeTicks::HighResNow();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    bus->ToInterleaved(bus->frames(), byte_size, interleaved.get());
  }
  double total_time_milliseconds =
      (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  perf_test::PrintResult(
      "audio_bus_to_interleaved", "", trace_name,
      total_time_milliseconds / kBenchmarkIterations, "ms", true);

  start = base::TimeTicks::HighResNow();
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    bus->FromInterleaved(interleaved.get(), bus->frames(), byte_size);
  }
  total_time_milliseconds =
      (base::TimeTicks::HighResNow() - start).InMillisecondsF();
  perf_test::PrintResult(
      "audio_bus_from_interleaved", "", trace_name,
      total_time_milliseconds / kBenchmarkIterations, "ms", true);
}

// Benchmark the FromInterleaved() and ToInterleaved() methods.
TEST(AudioBusPerfTest, Interleave) {
  scoped_ptr<AudioBus> bus = AudioBus::Create(2, 48000 * 120);
  FakeAudioRenderCallback callback(0.2);
  callback.Render(bus.get(), 0);

  RunInterleaveBench<int8>(bus.get(), "int8");
  RunInterleaveBench<int16>(bus.get(), "int16");
  RunInterleaveBench<int32>(bus.get(), "int32");
}

} // namespace media
