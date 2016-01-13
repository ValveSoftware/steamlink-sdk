// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_RASTERIZE_AND_RECORD_BENCHMARK_H_
#define CC_DEBUG_RASTERIZE_AND_RECORD_BENCHMARK_H_

#include <map>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/resources/picture.h"

namespace base {
class DictionaryValue;
}

namespace cc {

class LayerTreeHost;
class Layer;
class RasterizeAndRecordBenchmark : public MicroBenchmark {
 public:
  explicit RasterizeAndRecordBenchmark(
      scoped_ptr<base::Value> value,
      const MicroBenchmark::DoneCallback& callback);
  virtual ~RasterizeAndRecordBenchmark();

  // Implements MicroBenchmark interface.
  virtual void DidUpdateLayers(LayerTreeHost* host) OVERRIDE;
  virtual void RunOnLayer(PictureLayer* layer) OVERRIDE;

  virtual scoped_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::MessageLoopProxy> origin_loop) OVERRIDE;

 private:
  void Run(Layer* layer);

  void RecordRasterResults(scoped_ptr<base::Value> results);

  struct RecordResults {
    RecordResults();
    ~RecordResults();

    int pixels_recorded;
    base::TimeDelta total_best_time[Picture::RECORDING_MODE_COUNT];
  };

  RecordResults record_results_;
  int record_repeat_count_;
  scoped_ptr<base::Value> settings_;
  scoped_ptr<base::DictionaryValue> results_;

  // The following is used in DCHECKs.
  bool main_thread_benchmark_done_;

  LayerTreeHost* host_;

  base::WeakPtrFactory<RasterizeAndRecordBenchmark> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_DEBUG_RASTERIZE_AND_RECORD_BENCHMARK_H_
