// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/picture_record_benchmark.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_common.h"
#include "ui/gfx/rect.h"

namespace cc {

namespace {

const int kPositionIncrement = 100;
const int kTileGridSize = 512;
const int kTileGridBorder = 1;

}  // namespace

PictureRecordBenchmark::PictureRecordBenchmark(
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback)
    : MicroBenchmark(callback) {
  if (!value)
    return;

  base::ListValue* list = NULL;
  value->GetAsList(&list);
  if (!list)
    return;

  for (base::ListValue::iterator it = list->begin(); it != list->end(); ++it) {
    base::DictionaryValue* dictionary = NULL;
    (*it)->GetAsDictionary(&dictionary);
    if (!dictionary ||
        !dictionary->HasKey("width") ||
        !dictionary->HasKey("height"))
      continue;

    int width, height;
    dictionary->GetInteger("width", &width);
    dictionary->GetInteger("height", &height);

    dimensions_.push_back(std::make_pair(width, height));
  }
}

PictureRecordBenchmark::~PictureRecordBenchmark() {}

void PictureRecordBenchmark::DidUpdateLayers(LayerTreeHost* host) {
  LayerTreeHostCommon::CallFunctionForSubtree(
      host->root_layer(),
      base::Bind(&PictureRecordBenchmark::Run, base::Unretained(this)));

  scoped_ptr<base::ListValue> results(new base::ListValue());
  for (std::map<std::pair<int, int>, TotalTime>::iterator it = times_.begin();
       it != times_.end();
       ++it) {
    std::pair<int, int> dimensions = it->first;
    base::TimeDelta total_time = it->second.first;
    unsigned total_count = it->second.second;

    double average_time = 0.0;
    if (total_count > 0)
      average_time = total_time.InMillisecondsF() / total_count;

    scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger("width", dimensions.first);
    result->SetInteger("height", dimensions.second);
    result->SetInteger("samples_count", total_count);
    result->SetDouble("time_ms", average_time);

    results->Append(result.release());
  }

  NotifyDone(results.PassAs<base::Value>());
}

void PictureRecordBenchmark::Run(Layer* layer) {
  layer->RunMicroBenchmark(this);
}

void PictureRecordBenchmark::RunOnLayer(PictureLayer* layer) {
  ContentLayerClient* painter = layer->client();
  gfx::Size content_bounds = layer->content_bounds();

  SkTileGridFactory::TileGridInfo tile_grid_info;
  tile_grid_info.fTileInterval.set(kTileGridSize - 2 * kTileGridBorder,
                                   kTileGridSize - 2 * kTileGridBorder);
  tile_grid_info.fMargin.set(kTileGridBorder, kTileGridBorder);
  tile_grid_info.fOffset.set(-kTileGridBorder, -kTileGridBorder);

  for (size_t i = 0; i < dimensions_.size(); ++i) {
    std::pair<int, int> dimensions = dimensions_[i];
    int width = dimensions.first;
    int height = dimensions.second;

    int y_limit = std::max(1, content_bounds.height() - height);
    int x_limit = std::max(1, content_bounds.width() - width);
    for (int y = 0; y < y_limit; y += kPositionIncrement) {
      for (int x = 0; x < x_limit; x += kPositionIncrement) {
        gfx::Rect rect = gfx::Rect(x, y, width, height);

        base::TimeTicks start = base::TimeTicks::HighResNow();

        scoped_refptr<Picture> picture = Picture::Create(
            rect, painter, tile_grid_info, false, 0, Picture::RECORD_NORMALLY);

        base::TimeTicks end = base::TimeTicks::HighResNow();
        base::TimeDelta duration = end - start;
        TotalTime& total_time = times_[dimensions];
        total_time.first += duration;
        total_time.second++;
      }
    }
  }
}

}  // namespace cc
