// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rasterize_and_record_benchmark.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/debug/lap_timer.h"
#include "cc/debug/rasterize_and_record_benchmark_impl.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/display_item_list.h"
#include "cc/playback/recording_source.h"
#include "cc/trees/layer_tree.h"
#include "cc/trees/layer_tree_host_common.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

namespace {

const int kDefaultRecordRepeatCount = 100;

// Parameters for LapTimer.
const int kTimeLimitMillis = 1;
const int kWarmupRuns = 0;
const int kTimeCheckInterval = 1;

const char* kModeSuffixes[RecordingSource::RECORDING_MODE_COUNT] = {
    "",
    "_painting_disabled",
    "_caching_disabled",
    "_construction_disabled",
    "_subsequence_caching_disabled",
    "_partial_invalidation"};

ContentLayerClient::PaintingControlSetting
RecordingModeToPaintingControlSetting(RecordingSource::RecordingMode mode) {
  switch (mode) {
    case RecordingSource::RECORD_NORMALLY:
      return ContentLayerClient::PAINTING_BEHAVIOR_NORMAL_FOR_TEST;
    case RecordingSource::RECORD_WITH_PAINTING_DISABLED:
      return ContentLayerClient::DISPLAY_LIST_PAINTING_DISABLED;
    case RecordingSource::RECORD_WITH_CACHING_DISABLED:
      return ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
    case RecordingSource::RECORD_WITH_CONSTRUCTION_DISABLED:
      return ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED;
    case RecordingSource::RECORD_WITH_SUBSEQUENCE_CACHING_DISABLED:
      return ContentLayerClient::SUBSEQUENCE_CACHING_DISABLED;
    case RecordingSource::RECORD_WITH_PARTIAL_INVALIDATION:
      return ContentLayerClient::PARTIAL_INVALIDATION;
    case RecordingSource::RECORDING_MODE_COUNT:
      NOTREACHED();
  }
  return ContentLayerClient::PAINTING_BEHAVIOR_NORMAL_FOR_TEST;
}

}  // namespace

RasterizeAndRecordBenchmark::RasterizeAndRecordBenchmark(
    std::unique_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback)
    : MicroBenchmark(callback),
      record_repeat_count_(kDefaultRecordRepeatCount),
      settings_(std::move(value)),
      main_thread_benchmark_done_(false),
      layer_tree_(nullptr),
      weak_ptr_factory_(this) {
  base::DictionaryValue* settings = nullptr;
  settings_->GetAsDictionary(&settings);
  if (!settings)
    return;

  if (settings->HasKey("record_repeat_count"))
    settings->GetInteger("record_repeat_count", &record_repeat_count_);
}

RasterizeAndRecordBenchmark::~RasterizeAndRecordBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterizeAndRecordBenchmark::DidUpdateLayers(LayerTree* layer_tree) {
  layer_tree_ = layer_tree;
  LayerTreeHostCommon::CallFunctionForEveryLayer(
      layer_tree, [this](Layer* layer) { layer->RunMicroBenchmark(this); });

  DCHECK(!results_.get());
  results_ = base::WrapUnique(new base::DictionaryValue);
  results_->SetInteger("pixels_recorded", record_results_.pixels_recorded);
  results_->SetInteger("picture_memory_usage",
                       static_cast<int>(record_results_.bytes_used));

  for (int i = 0; i < RecordingSource::RECORDING_MODE_COUNT; i++) {
    std::string name = base::StringPrintf("record_time%s_ms", kModeSuffixes[i]);
    results_->SetDouble(name,
                        record_results_.total_best_time[i].InMillisecondsF());
  }
  main_thread_benchmark_done_ = true;
}

void RasterizeAndRecordBenchmark::RecordRasterResults(
    std::unique_ptr<base::Value> results_value) {
  DCHECK(main_thread_benchmark_done_);

  base::DictionaryValue* results = nullptr;
  results_value->GetAsDictionary(&results);
  DCHECK(results);

  results_->MergeDictionary(results);

  NotifyDone(std::move(results_));
}

std::unique_ptr<MicroBenchmarkImpl>
RasterizeAndRecordBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner) {
  return base::WrapUnique(new RasterizeAndRecordBenchmarkImpl(
      origin_task_runner, settings_.get(),
      base::Bind(&RasterizeAndRecordBenchmark::RecordRasterResults,
                 weak_ptr_factory_.GetWeakPtr())));
}

void RasterizeAndRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  DCHECK(layer_tree_);

  if (!layer->DrawsContent())
    return;

  ContentLayerClient* painter = layer->client();

  for (int mode_index = 0; mode_index < RecordingSource::RECORDING_MODE_COUNT;
       mode_index++) {
    ContentLayerClient::PaintingControlSetting painting_control =
        RecordingModeToPaintingControlSetting(
            static_cast<RecordingSource::RecordingMode>(mode_index));
    base::TimeDelta min_time = base::TimeDelta::Max();
    size_t memory_used = 0;

    scoped_refptr<DisplayItemList> display_list;
    for (int i = 0; i < record_repeat_count_; ++i) {
      // Run for a minimum amount of time to avoid problems with timer
      // quantization when the layer is very small.
      LapTimer timer(kWarmupRuns,
                     base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
                     kTimeCheckInterval);

      do {
        display_list = painter->PaintContentsToDisplayList(painting_control);
        if (display_list->ShouldBeAnalyzedForSolidColor()) {
          gfx::Size layer_size = layer->paint_properties().bounds;
          skia::AnalysisCanvas canvas(layer_size.width(), layer_size.height());
          display_list->Raster(&canvas, nullptr, gfx::Rect(layer_size), 1.f);
        }

        if (memory_used) {
          // Verify we are recording the same thing each time.
          DCHECK_EQ(memory_used, display_list->ApproximateMemoryUsage());
        } else {
          memory_used = display_list->ApproximateMemoryUsage();
        }

        timer.NextLap();
      } while (!timer.HasTimeLimitExpired());
      base::TimeDelta duration =
          base::TimeDelta::FromMillisecondsD(timer.MsPerLap());
      if (duration < min_time)
        min_time = duration;
    }

    if (mode_index == RecordingSource::RECORD_NORMALLY) {
      record_results_.bytes_used +=
          memory_used + painter->GetApproximateUnsharedMemoryUsage();
      record_results_.pixels_recorded += painter->PaintableRegion().width() *
                                         painter->PaintableRegion().height();
    }
    record_results_.total_best_time[mode_index] += min_time;
  }
}

RasterizeAndRecordBenchmark::RecordResults::RecordResults()
    : pixels_recorded(0), bytes_used(0) {
}

RasterizeAndRecordBenchmark::RecordResults::~RecordResults() {}

}  // namespace cc
