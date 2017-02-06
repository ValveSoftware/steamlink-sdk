// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/compositor_timing_history.h"

#include <stddef.h>
#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "cc/debug/rendering_stats_instrumentation.h"

namespace cc {

class CompositorTimingHistory::UMAReporter {
 public:
  virtual ~UMAReporter() {}

  // Throughput measurements
  virtual void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) = 0;
  virtual void AddBeginMainFrameIntervalNotCritical(
      base::TimeDelta interval) = 0;
  virtual void AddCommitInterval(base::TimeDelta interval) = 0;
  virtual void AddDrawInterval(base::TimeDelta interval) = 0;

  // Latency measurements
  virtual void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) = 0;
  virtual void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) = 0;
  virtual void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) = 0;
  virtual void AddCommitToReadyToActivateDuration(base::TimeDelta duration) = 0;
  virtual void AddPrepareTilesDuration(base::TimeDelta duration) = 0;
  virtual void AddActivateDuration(base::TimeDelta duration) = 0;
  virtual void AddDrawDuration(base::TimeDelta duration) = 0;
  virtual void AddSwapToAckLatency(base::TimeDelta duration) = 0;

  // Synchronization measurements
  virtual void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) = 0;
};

namespace {

// Using the 90th percentile will disable latency recovery
// if we are missing the deadline approximately ~6 times per
// second.
// TODO(brianderson): Fine tune the percentiles below.
const size_t kDurationHistorySize = 60;
const double kBeginMainFrameQueueDurationEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameQueueDurationNotCriticalEstimationPercentile = 90.0;
const double kBeginMainFrameStartToCommitEstimationPercentile = 90.0;
const double kCommitToReadyToActivateEstimationPercentile = 90.0;
const double kPrepareTilesEstimationPercentile = 90.0;
const double kActivateEstimationPercentile = 90.0;
const double kDrawEstimationPercentile = 90.0;

const int kUmaDurationMinMicros = 1;
const int64_t kUmaDurationMaxMicros = base::Time::kMicrosecondsPerSecond / 5;
const int kUmaDurationBucketCount = 100;

// This macro is deprecated since its bucket count uses too much bandwidth.
// It also has sub-optimal range and bucket distribution.
// TODO(brianderson): Delete this macro and associated UMAs once there is
// sufficient overlap with the re-bucketed UMAs.
#define UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(name, sample)                     \
  UMA_HISTOGRAM_CUSTOM_COUNTS(name, sample.InMicroseconds(),                \
                              kUmaDurationMinMicros, kUmaDurationMaxMicros, \
                              kUmaDurationBucketCount);

// ~90 VSync aligned UMA buckets.
const int kUMAVSyncBuckets[] = {
    // Powers of two from 0 to 2048 us @ 50% precision
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
    // Every 8 Hz from 256 Hz to 128 Hz @ 3-6% precision
    3906, 4032, 4167, 4310, 4464, 4630, 4808, 5000, 5208, 5435, 5682, 5952,
    6250, 6579, 6944, 7353,
    // Every 4 Hz from 128 Hz to 64 Hz @ 3-6% precision
    7813, 8065, 8333, 8621, 8929, 9259, 9615, 10000, 10417, 10870, 11364, 11905,
    12500, 13158, 13889, 14706,
    // Every 2 Hz from 64 Hz to 32 Hz @ 3-6% precision
    15625, 16129, 16667, 17241, 17857, 18519, 19231, 20000, 20833, 21739, 22727,
    23810, 25000, 26316, 27778, 29412,
    // Every 1 Hz from 32 Hz to 1 Hz @ 3-33% precision
    31250, 32258, 33333, 34483, 35714, 37037, 38462, 40000, 41667, 43478, 45455,
    47619, 50000, 52632, 55556, 58824, 62500, 66667, 71429, 76923, 83333, 90909,
    100000, 111111, 125000, 142857, 166667, 200000, 250000, 333333, 500000,
    // Powers of two from 1s to 32s @ 50% precision
    1000000, 2000000, 4000000, 8000000, 16000000, 32000000,
};

// ~50 UMA buckets with high precision from ~100 us to 1s.
const int kUMADurationBuckets[] = {
    // Powers of 2 from 1 us to 64 us @ 50% precision.
    1, 2, 4, 8, 16, 32, 64,
    // 1.25^20, 1.25^21, ..., 1.25^62 @ 20% precision.
    87, 108, 136, 169, 212, 265, 331, 414, 517, 646, 808, 1010, 1262, 1578,
    1972, 2465, 3081, 3852, 4815, 6019, 7523, 9404, 11755, 14694, 18367, 22959,
    28699, 35873, 44842, 56052, 70065, 87581, 109476, 136846, 171057, 213821,
    267276, 334096, 417619, 522024, 652530, 815663, 1019579,
    // Powers of 2 from 2s to 32s @ 50% precision.
    2000000, 4000000, 8000000, 16000000, 32000000,
};

#define UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(name, sample)             \
  do {                                                                     \
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(name, sample);                       \
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(                                      \
        name "2", sample.InMicroseconds(),                                 \
        std::vector<int>(kUMAVSyncBuckets,                                 \
                         kUMAVSyncBuckets + arraysize(kUMAVSyncBuckets))); \
  } while (false)

#define UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(name, sample)           \
  do {                                                              \
    UMA_HISTOGRAM_CUSTOM_TIMES_MICROS(name, sample);                \
    UMA_HISTOGRAM_CUSTOM_ENUMERATION(                               \
        name "2", sample.InMicroseconds(),                          \
        std::vector<int>(                                           \
            kUMADurationBuckets,                                    \
            kUMADurationBuckets + arraysize(kUMADurationBuckets))); \
  } while (false)

class RendererUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~RendererUMAReporter() override {}

  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.BeginMainFrameIntervalCritical", interval);
  }

  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.BeginMainFrameIntervalNotCritical", interval);
  }

  void AddCommitInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.CommitInterval", interval);
  }

  void AddDrawInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED("Scheduling.Renderer.DrawInterval",
                                             interval);
  }

  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginMainFrameQueueDurationCritical", duration);
  }

  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginMainFrameQueueDurationNotCritical", duration);
  }

  void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.BeginMainFrameStartToCommitDuration", duration);
  }

  void AddCommitToReadyToActivateDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.CommitToReadyToActivateDuration", duration);
  }

  void AddPrepareTilesDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Renderer.PrepareTilesDuration", duration);
  }

  void AddActivateDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Renderer.ActivateDuration",
                                        duration);
  }

  void AddDrawDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Renderer.DrawDuration",
                                        duration);
  }

  void AddSwapToAckLatency(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Renderer.SwapToAckLatency",
                                        duration);
  }

  void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Renderer.MainAndImplFrameTimeDelta", delta);
  }
};

class BrowserUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~BrowserUMAReporter() override {}

  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Browser.BeginMainFrameIntervalCritical", interval);
  }

  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Browser.BeginMainFrameIntervalNotCritical", interval);
  }

  void AddCommitInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Browser.CommitInterval", interval);
  }

  void AddDrawInterval(base::TimeDelta interval) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED("Scheduling.Browser.DrawInterval",
                                             interval);
  }

  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.BeginMainFrameQueueDurationCritical", duration);
  }

  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.BeginMainFrameQueueDurationNotCritical", duration);
  }

  void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.BeginMainFrameStartToCommitDuration", duration);
  }

  void AddCommitToReadyToActivateDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.CommitToReadyToActivateDuration", duration);
  }

  void AddPrepareTilesDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION(
        "Scheduling.Browser.PrepareTilesDuration", duration);
  }

  void AddActivateDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Browser.ActivateDuration",
                                        duration);
  }

  void AddDrawDuration(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Browser.DrawDuration",
                                        duration);
  }

  void AddSwapToAckLatency(base::TimeDelta duration) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_DURATION("Scheduling.Browser.SwapToAckLatency",
                                        duration);
  }

  void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) override {
    UMA_HISTOGRAM_CUSTOM_TIMES_VSYNC_ALIGNED(
        "Scheduling.Browser.MainAndImplFrameTimeDelta", delta);
  }
};

class NullUMAReporter : public CompositorTimingHistory::UMAReporter {
 public:
  ~NullUMAReporter() override {}
  void AddBeginMainFrameIntervalCritical(base::TimeDelta interval) override {}
  void AddBeginMainFrameIntervalNotCritical(base::TimeDelta interval) override {
  }
  void AddCommitInterval(base::TimeDelta interval) override {}
  void AddDrawInterval(base::TimeDelta interval) override {}
  void AddBeginMainFrameQueueDurationCriticalDuration(
      base::TimeDelta duration) override {}
  void AddBeginMainFrameQueueDurationNotCriticalDuration(
      base::TimeDelta duration) override {}
  void AddBeginMainFrameStartToCommitDuration(
      base::TimeDelta duration) override {}
  void AddCommitToReadyToActivateDuration(base::TimeDelta duration) override {}
  void AddPrepareTilesDuration(base::TimeDelta duration) override {}
  void AddActivateDuration(base::TimeDelta duration) override {}
  void AddDrawDuration(base::TimeDelta duration) override {}
  void AddSwapToAckLatency(base::TimeDelta duration) override {}
  void AddMainAndImplFrameTimeDelta(base::TimeDelta delta) override {}
};

}  // namespace

CompositorTimingHistory::CompositorTimingHistory(
    bool using_synchronous_renderer_compositor,
    UMACategory uma_category,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : using_synchronous_renderer_compositor_(
          using_synchronous_renderer_compositor),
      enabled_(false),
      did_send_begin_main_frame_(false),
      begin_main_frame_needed_continuously_(false),
      begin_main_frame_committing_continuously_(false),
      compositor_drawing_continuously_(false),
      begin_main_frame_queue_duration_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_critical_history_(kDurationHistorySize),
      begin_main_frame_queue_duration_not_critical_history_(
          kDurationHistorySize),
      begin_main_frame_start_to_commit_duration_history_(kDurationHistorySize),
      commit_to_ready_to_activate_duration_history_(kDurationHistorySize),
      prepare_tiles_duration_history_(kDurationHistorySize),
      activate_duration_history_(kDurationHistorySize),
      draw_duration_history_(kDurationHistorySize),
      begin_main_frame_on_critical_path_(false),
      uma_reporter_(CreateUMAReporter(uma_category)),
      rendering_stats_instrumentation_(rendering_stats_instrumentation) {}

CompositorTimingHistory::~CompositorTimingHistory() {
}

std::unique_ptr<CompositorTimingHistory::UMAReporter>
CompositorTimingHistory::CreateUMAReporter(UMACategory category) {
  switch (category) {
    case RENDERER_UMA:
      return base::WrapUnique(new RendererUMAReporter);
      break;
    case BROWSER_UMA:
      return base::WrapUnique(new BrowserUMAReporter);
      break;
    case NULL_UMA:
      return base::WrapUnique(new NullUMAReporter);
      break;
  }
  NOTREACHED();
  return base::WrapUnique<CompositorTimingHistory::UMAReporter>(nullptr);
}

void CompositorTimingHistory::AsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetDouble(
      "begin_main_frame_queue_critical_estimate_ms",
      BeginMainFrameQueueDurationCriticalEstimate().InMillisecondsF());
  state->SetDouble(
      "begin_main_frame_queue_not_critical_estimate_ms",
      BeginMainFrameQueueDurationNotCriticalEstimate().InMillisecondsF());
  state->SetDouble(
      "begin_main_frame_start_to_commit_estimate_ms",
      BeginMainFrameStartToCommitDurationEstimate().InMillisecondsF());
  state->SetDouble("commit_to_ready_to_activate_estimate_ms",
                   CommitToReadyToActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("prepare_tiles_estimate_ms",
                   PrepareTilesDurationEstimate().InMillisecondsF());
  state->SetDouble("activate_estimate_ms",
                   ActivateDurationEstimate().InMillisecondsF());
  state->SetDouble("draw_estimate_ms",
                   DrawDurationEstimate().InMillisecondsF());
}

base::TimeTicks CompositorTimingHistory::Now() const {
  return base::TimeTicks::Now();
}

void CompositorTimingHistory::SetRecordingEnabled(bool enabled) {
  enabled_ = enabled;
}

void CompositorTimingHistory::SetBeginMainFrameNeededContinuously(bool active) {
  if (active == begin_main_frame_needed_continuously_)
    return;
  begin_main_frame_end_time_prev_ = base::TimeTicks();
  begin_main_frame_needed_continuously_ = active;
}

void CompositorTimingHistory::SetBeginMainFrameCommittingContinuously(
    bool active) {
  if (active == begin_main_frame_committing_continuously_)
    return;
  new_active_tree_draw_end_time_prev_ = base::TimeTicks();
  begin_main_frame_committing_continuously_ = active;
}

void CompositorTimingHistory::SetCompositorDrawingContinuously(bool active) {
  if (active == compositor_drawing_continuously_)
    return;
  draw_end_time_prev_ = base::TimeTicks();
  compositor_drawing_continuously_ = active;
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationCriticalEstimate() const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta critical =
      begin_main_frame_queue_duration_critical_history_.Percentile(
          kBeginMainFrameQueueDurationCriticalEstimationPercentile);
  // Return the min since critical BeginMainFrames are likely fast if
  // the non critical ones are.
  return std::min(critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameQueueDurationNotCriticalEstimate()
    const {
  base::TimeDelta all = begin_main_frame_queue_duration_history_.Percentile(
      kBeginMainFrameQueueDurationEstimationPercentile);
  base::TimeDelta not_critical =
      begin_main_frame_queue_duration_not_critical_history_.Percentile(
          kBeginMainFrameQueueDurationNotCriticalEstimationPercentile);
  // Return the max since, non critical BeginMainFrames are likely slow if
  // the critical ones are.
  return std::max(not_critical, all);
}

base::TimeDelta
CompositorTimingHistory::BeginMainFrameStartToCommitDurationEstimate() const {
  return begin_main_frame_start_to_commit_duration_history_.Percentile(
      kBeginMainFrameStartToCommitEstimationPercentile);
}

base::TimeDelta
CompositorTimingHistory::CommitToReadyToActivateDurationEstimate() const {
  return commit_to_ready_to_activate_duration_history_.Percentile(
      kCommitToReadyToActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::PrepareTilesDurationEstimate() const {
  return prepare_tiles_duration_history_.Percentile(
      kPrepareTilesEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::ActivateDurationEstimate() const {
  return activate_duration_history_.Percentile(kActivateEstimationPercentile);
}

base::TimeDelta CompositorTimingHistory::DrawDurationEstimate() const {
  return draw_duration_history_.Percentile(kDrawEstimationPercentile);
}

void CompositorTimingHistory::DidCreateAndInitializeOutputSurface() {
  // After we get a new output surface, we won't get a spurious
  // swap ack from the old output surface.
  swap_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillBeginImplFrame(
    bool new_active_tree_is_likely) {
  // The check for whether a BeginMainFrame was sent anytime between two
  // BeginImplFrames protects us from not detecting a fast main thread that
  // does all it's work and goes idle in between BeginImplFrames.
  // For example, this may happen if an animation is being driven with
  // setInterval(17) or if input events just happen to arrive in the
  // middle of every frame.
  if (!new_active_tree_is_likely && !did_send_begin_main_frame_) {
    SetBeginMainFrameNeededContinuously(false);
    SetBeginMainFrameCommittingContinuously(false);
  }

  did_send_begin_main_frame_ = false;
}

void CompositorTimingHistory::WillFinishImplFrame(bool needs_redraw) {
  if (!needs_redraw)
    SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::BeginImplFrameNotExpectedSoon() {
  SetBeginMainFrameNeededContinuously(false);
  SetBeginMainFrameCommittingContinuously(false);
  SetCompositorDrawingContinuously(false);
}

void CompositorTimingHistory::WillBeginMainFrame(
    bool on_critical_path,
    base::TimeTicks main_frame_time) {
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_sent_time_);
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_frame_time_);

  begin_main_frame_on_critical_path_ = on_critical_path;
  begin_main_frame_sent_time_ = Now();
  begin_main_frame_frame_time_ = main_frame_time;

  did_send_begin_main_frame_ = true;
  SetBeginMainFrameNeededContinuously(true);
}

void CompositorTimingHistory::BeginMainFrameStarted(
    base::TimeTicks main_thread_start_time) {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);
  DCHECK_EQ(base::TimeTicks(), begin_main_frame_start_time_);
  begin_main_frame_start_time_ = main_thread_start_time;
}

void CompositorTimingHistory::BeginMainFrameAborted() {
  SetBeginMainFrameCommittingContinuously(false);
  DidBeginMainFrame();
  begin_main_frame_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidCommit() {
  DCHECK_EQ(base::TimeTicks(), pending_tree_main_frame_time_);
  SetBeginMainFrameCommittingContinuously(true);
  DidBeginMainFrame();
  pending_tree_main_frame_time_ = begin_main_frame_frame_time_;
  begin_main_frame_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidBeginMainFrame() {
  DCHECK_NE(base::TimeTicks(), begin_main_frame_sent_time_);

  begin_main_frame_end_time_ = Now();

  // If the BeginMainFrame start time isn't know, assume it was immediate
  // for scheduling purposes, but don't report it for UMA to avoid skewing
  // the results.
  bool begin_main_frame_start_time_is_valid =
      !begin_main_frame_start_time_.is_null();
  if (!begin_main_frame_start_time_is_valid)
    begin_main_frame_start_time_ = begin_main_frame_sent_time_;

  base::TimeDelta begin_main_frame_sent_to_commit_duration =
      begin_main_frame_end_time_ - begin_main_frame_sent_time_;
  base::TimeDelta begin_main_frame_queue_duration =
      begin_main_frame_start_time_ - begin_main_frame_sent_time_;
  base::TimeDelta begin_main_frame_start_to_commit_duration =
      begin_main_frame_end_time_ - begin_main_frame_start_time_;

  rendering_stats_instrumentation_->AddBeginMainFrameToCommitDuration(
      begin_main_frame_sent_to_commit_duration);

  if (begin_main_frame_start_time_is_valid) {
    if (begin_main_frame_on_critical_path_) {
      uma_reporter_->AddBeginMainFrameQueueDurationCriticalDuration(
          begin_main_frame_queue_duration);
    } else {
      uma_reporter_->AddBeginMainFrameQueueDurationNotCriticalDuration(
          begin_main_frame_queue_duration);
    }
  }

  uma_reporter_->AddBeginMainFrameStartToCommitDuration(
      begin_main_frame_start_to_commit_duration);

  if (enabled_) {
    begin_main_frame_queue_duration_history_.InsertSample(
        begin_main_frame_queue_duration);
    if (begin_main_frame_on_critical_path_) {
      begin_main_frame_queue_duration_critical_history_.InsertSample(
          begin_main_frame_queue_duration);
    } else {
      begin_main_frame_queue_duration_not_critical_history_.InsertSample(
          begin_main_frame_queue_duration);
    }
    begin_main_frame_start_to_commit_duration_history_.InsertSample(
        begin_main_frame_start_to_commit_duration);
  }

  if (begin_main_frame_needed_continuously_) {
    if (!begin_main_frame_end_time_prev_.is_null()) {
      base::TimeDelta commit_interval =
          begin_main_frame_end_time_ - begin_main_frame_end_time_prev_;
      if (begin_main_frame_on_critical_path_)
        uma_reporter_->AddBeginMainFrameIntervalCritical(commit_interval);
      else
        uma_reporter_->AddBeginMainFrameIntervalNotCritical(commit_interval);
    }
    begin_main_frame_end_time_prev_ = begin_main_frame_end_time_;
  }

  begin_main_frame_sent_time_ = base::TimeTicks();
  begin_main_frame_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillPrepareTiles() {
  DCHECK_EQ(base::TimeTicks(), prepare_tiles_start_time_);
  prepare_tiles_start_time_ = Now();
}

void CompositorTimingHistory::DidPrepareTiles() {
  DCHECK_NE(base::TimeTicks(), prepare_tiles_start_time_);

  base::TimeDelta prepare_tiles_duration = Now() - prepare_tiles_start_time_;
  uma_reporter_->AddPrepareTilesDuration(prepare_tiles_duration);
  if (enabled_)
    prepare_tiles_duration_history_.InsertSample(prepare_tiles_duration);

  prepare_tiles_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::ReadyToActivate() {
  // We only care about the first ready to activate signal
  // after a commit.
  if (begin_main_frame_end_time_ == base::TimeTicks())
    return;

  base::TimeDelta time_since_commit = Now() - begin_main_frame_end_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.

  base::TimeDelta commit_to_ready_to_activate_estimate =
      CommitToReadyToActivateDurationEstimate();
  uma_reporter_->AddCommitToReadyToActivateDuration(time_since_commit);
  rendering_stats_instrumentation_->AddCommitToActivateDuration(
      time_since_commit, commit_to_ready_to_activate_estimate);

  if (enabled_) {
    commit_to_ready_to_activate_duration_history_.InsertSample(
        time_since_commit);
  }

  begin_main_frame_end_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillActivate() {
  DCHECK_EQ(base::TimeTicks(), activate_start_time_);
  activate_start_time_ = Now();
}

void CompositorTimingHistory::DidActivate() {
  DCHECK_NE(base::TimeTicks(), activate_start_time_);
  base::TimeDelta activate_duration = Now() - activate_start_time_;

  uma_reporter_->AddActivateDuration(activate_duration);
  if (enabled_)
    activate_duration_history_.InsertSample(activate_duration);

  // The synchronous compositor doesn't necessarily draw every new active tree.
  if (!using_synchronous_renderer_compositor_)
    DCHECK_EQ(base::TimeTicks(), active_tree_main_frame_time_);
  active_tree_main_frame_time_ = pending_tree_main_frame_time_;

  activate_start_time_ = base::TimeTicks();
  pending_tree_main_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::WillDraw() {
  DCHECK_EQ(base::TimeTicks(), draw_start_time_);
  draw_start_time_ = Now();
}

void CompositorTimingHistory::DrawAborted() {
  active_tree_main_frame_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidDraw(bool used_new_active_tree,
                                      bool main_thread_missed_last_deadline,
                                      base::TimeTicks impl_frame_time) {
  DCHECK_NE(base::TimeTicks(), draw_start_time_);
  base::TimeTicks draw_end_time = Now();
  base::TimeDelta draw_duration = draw_end_time - draw_start_time_;

  // Before adding the new data point to the timing history, see what we would
  // have predicted for this frame. This allows us to keep track of the accuracy
  // of our predictions.
  base::TimeDelta draw_estimate = DrawDurationEstimate();
  rendering_stats_instrumentation_->AddDrawDuration(draw_duration,
                                                    draw_estimate);

  uma_reporter_->AddDrawDuration(draw_duration);

  if (enabled_) {
    draw_duration_history_.InsertSample(draw_duration);
  }

  SetCompositorDrawingContinuously(true);
  if (!draw_end_time_prev_.is_null()) {
    base::TimeDelta draw_interval = draw_end_time - draw_end_time_prev_;
    uma_reporter_->AddDrawInterval(draw_interval);
  }
  draw_end_time_prev_ = draw_end_time;

  if (used_new_active_tree) {
    DCHECK_NE(base::TimeTicks(), active_tree_main_frame_time_);
    base::TimeDelta main_and_impl_delta =
        impl_frame_time - active_tree_main_frame_time_;
    DCHECK_GE(main_and_impl_delta, base::TimeDelta());
    uma_reporter_->AddMainAndImplFrameTimeDelta(main_and_impl_delta);
    active_tree_main_frame_time_ = base::TimeTicks();

    if (begin_main_frame_committing_continuously_) {
      if (!new_active_tree_draw_end_time_prev_.is_null()) {
        base::TimeDelta draw_interval =
            draw_end_time - new_active_tree_draw_end_time_prev_;
        uma_reporter_->AddCommitInterval(draw_interval);
      }
      new_active_tree_draw_end_time_prev_ = draw_end_time;
    }
  }

  draw_start_time_ = base::TimeTicks();
}

void CompositorTimingHistory::DidSwapBuffers() {
  DCHECK_EQ(base::TimeTicks(), swap_start_time_);
  swap_start_time_ = Now();
}

void CompositorTimingHistory::DidSwapBuffersComplete() {
  DCHECK_NE(base::TimeTicks(), swap_start_time_);
  base::TimeDelta swap_to_ack_duration = Now() - swap_start_time_;
  uma_reporter_->AddSwapToAckLatency(swap_to_ack_duration);
  swap_start_time_ = base::TimeTicks();
}

}  // namespace cc
