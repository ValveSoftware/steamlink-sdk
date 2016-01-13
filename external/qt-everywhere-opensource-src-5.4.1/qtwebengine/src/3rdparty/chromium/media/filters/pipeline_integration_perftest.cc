// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"
#include "media/filters/pipeline_integration_test_base.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterationsAudio = 200;
static const int kBenchmarkIterationsVideo = 20;

static void RunPlaybackBenchmark(const std::string& filename,
                                 const std::string& name,
                                 int iterations,
                                 bool audio_only) {
  double time_seconds = 0.0;

  for (int i = 0; i < iterations; ++i) {
    PipelineIntegrationTestBase pipeline;

    ASSERT_TRUE(pipeline.Start(GetTestDataFilePath(filename),
                               PIPELINE_OK,
                               PipelineIntegrationTestBase::kClockless));

    base::TimeTicks start = base::TimeTicks::HighResNow();
    pipeline.Play();

    ASSERT_TRUE(pipeline.WaitUntilOnEnded());

    // Call Stop() to ensure that the rendering is complete.
    pipeline.Stop();

    if (audio_only) {
      time_seconds += pipeline.GetAudioTime().InSecondsF();
    } else {
      time_seconds += (base::TimeTicks::HighResNow() - start).InSecondsF();
    }
  }

  perf_test::PrintResult(name,
                         "",
                         filename,
                         iterations / time_seconds,
                         "runs/s",
                         true);
}

static void RunVideoPlaybackBenchmark(const std::string& filename,
                                      const std::string name) {
  RunPlaybackBenchmark(filename, name, kBenchmarkIterationsVideo, false);
}

static void RunAudioPlaybackBenchmark(const std::string& filename,
                                      const std::string& name) {
  RunPlaybackBenchmark(filename, name, kBenchmarkIterationsAudio, true);
}

TEST(PipelineIntegrationPerfTest, AudioPlaybackBenchmark) {
  RunAudioPlaybackBenchmark("sfx_f32le.wav", "clockless_playback");
  RunAudioPlaybackBenchmark("sfx_s24le.wav", "clockless_playback");
  RunAudioPlaybackBenchmark("sfx_s16le.wav", "clockless_playback");
  RunAudioPlaybackBenchmark("sfx_u8.wav", "clockless_playback");
#if defined(USE_PROPRIETARY_CODECS)
  RunAudioPlaybackBenchmark("sfx.mp3", "clockless_playback");
#endif
}

TEST(PipelineIntegrationPerfTest, VP8PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear-640x360.webm",
                            "clockless_video_playback_vp8");
  RunVideoPlaybackBenchmark("bear-320x240.webm",
                            "clockless_video_playback_vp8");
}

TEST(PipelineIntegrationPerfTest, VP9PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear-vp9.webm", "clockless_video_playback_vp9");
}

TEST(PipelineIntegrationPerfTest, TheoraPlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear.ogv", "clockless_video_playback_theora");
}

#if defined(USE_PROPRIETARY_CODECS)
TEST(PipelineIntegrationPerfTest, MP4PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear-1280x720.mp4",
                            "clockless_video_playback_mp4");
}
#endif

}  // namespace media
