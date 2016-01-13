// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/rasterize_and_record_benchmark.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "cc/debug/lap_timer.h"
#include "cc/debug/rasterize_and_record_benchmark_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "ui/gfx/rect.h"

namespace cc {

namespace {

const int kDefaultRecordRepeatCount = 100;

const char* kModeSuffixes[Picture::RECORDING_MODE_COUNT] = {
    "", "_sk_null_canvas", "_painting_disabled", "_skrecord"};

}  // namespace

RasterizeAndRecordBenchmark::RasterizeAndRecordBenchmark(
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback)
    : MicroBenchmark(callback),
      record_repeat_count_(kDefaultRecordRepeatCount),
      settings_(value.Pass()),
      main_thread_benchmark_done_(false),
      host_(NULL),
      weak_ptr_factory_(this) {
  base::DictionaryValue* settings = NULL;
  settings_->GetAsDictionary(&settings);
  if (!settings)
    return;

  if (settings->HasKey("record_repeat_count"))
    settings->GetInteger("record_repeat_count", &record_repeat_count_);
}

RasterizeAndRecordBenchmark::~RasterizeAndRecordBenchmark() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void RasterizeAndRecordBenchmark::DidUpdateLayers(LayerTreeHost* host) {
  host_ = host;
  LayerTreeHostCommon::CallFunctionForSubtree(
      host->root_layer(),
      base::Bind(&RasterizeAndRecordBenchmark::Run, base::Unretained(this)));

  DCHECK(!results_.get());
  results_ = make_scoped_ptr(new base::DictionaryValue);
  results_->SetInteger("pixels_recorded", record_results_.pixels_recorded);

  for (int i = 0; i < Picture::RECORDING_MODE_COUNT; i++) {
    std::string name = base::StringPrintf("record_time%s_ms", kModeSuffixes[i]);
    results_->SetDouble(name,
                        record_results_.total_best_time[i].InMillisecondsF());
  }
  main_thread_benchmark_done_ = true;
}

void RasterizeAndRecordBenchmark::RecordRasterResults(
    scoped_ptr<base::Value> results_value) {
  DCHECK(main_thread_benchmark_done_);

  base::DictionaryValue* results = NULL;
  results_value->GetAsDictionary(&results);
  DCHECK(results);

  results_->MergeDictionary(results);

  NotifyDone(results_.PassAs<base::Value>());
}

scoped_ptr<MicroBenchmarkImpl> RasterizeAndRecordBenchmark::CreateBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop) {
  return scoped_ptr<MicroBenchmarkImpl>(new RasterizeAndRecordBenchmarkImpl(
      origin_loop,
      settings_.get(),
      base::Bind(&RasterizeAndRecordBenchmark::RecordRasterResults,
                 weak_ptr_factory_.GetWeakPtr())));
}

void RasterizeAndRecordBenchmark::Run(Layer* layer) {
  layer->RunMicroBenchmark(this);
}

void RasterizeAndRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  ContentLayerClient* painter = layer->client();
  gfx::Size content_bounds = layer->content_bounds();

  DCHECK(host_);
  gfx::Size tile_grid_size = host_->settings().default_tile_size;

  SkTileGridFactory::TileGridInfo tile_grid_info;
  PicturePileBase::ComputeTileGridInfo(tile_grid_size, &tile_grid_info);

  gfx::Rect visible_content_rect = gfx::ScaleToEnclosingRect(
      layer->visible_content_rect(), 1.f / layer->contents_scale_x());
  if (visible_content_rect.IsEmpty())
    return;

  for (int mode_index = 0; mode_index < Picture::RECORDING_MODE_COUNT;
       mode_index++) {
    Picture::RecordingMode mode =
        static_cast<Picture::RecordingMode>(mode_index);
    base::TimeDelta min_time = base::TimeDelta::Max();

    // Parameters for LapTimer.
    const int kTimeLimitMillis = 1;
    const int kWarmupRuns = 0;
    const int kTimeCheckInterval = 1;

    for (int i = 0; i < record_repeat_count_; ++i) {
      // Run for a minimum amount of time to avoid problems with timer
      // quantization when the layer is very small.
      LapTimer timer(kWarmupRuns,
                     base::TimeDelta::FromMilliseconds(kTimeLimitMillis),
                     kTimeCheckInterval);
      do {
        scoped_refptr<Picture> picture = Picture::Create(
            visible_content_rect, painter, tile_grid_info, false, 0, mode);
        timer.NextLap();
      } while (!timer.HasTimeLimitExpired());
      base::TimeDelta duration =
          base::TimeDelta::FromMillisecondsD(timer.MsPerLap());
      if (duration < min_time)
        min_time = duration;
    }

    if (mode == Picture::RECORD_NORMALLY) {
      record_results_.pixels_recorded +=
          visible_content_rect.width() * visible_content_rect.height();
    }
    record_results_.total_best_time[mode_index] += min_time;
  }
}

RasterizeAndRecordBenchmark::RecordResults::RecordResults()
    : pixels_recorded(0) {}

RasterizeAndRecordBenchmark::RecordResults::~RecordResults() {}

}  // namespace cc
