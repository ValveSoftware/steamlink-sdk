// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_INVALIDATION_BENCHMARK_H_
#define CC_DEBUG_INVALIDATION_BENCHMARK_H_

#include <stdint.h>

#include <string>

#include "cc/debug/micro_benchmark_controller.h"

namespace cc {

class LayerTreeHost;
class Layer;
// NOTE: this benchmark will not measure or return any results, it will simply
// invalidate a certain area of each layer every frame. It is intended to be
// used in combination with a telemetry benchmark that does the actual
// measurement.
class CC_EXPORT InvalidationBenchmark : public MicroBenchmark {
 public:
  explicit InvalidationBenchmark(std::unique_ptr<base::Value> value,
                                 const MicroBenchmark::DoneCallback& callback);
  ~InvalidationBenchmark() override;

  // Implements MicroBenchmark interface.
  void DidUpdateLayers(LayerTreeHost* host) override;
  void RunOnLayer(PictureLayer* layer) override;
  bool ProcessMessage(std::unique_ptr<base::Value> value) override;

 private:
  enum Mode { FIXED_SIZE, LAYER, VIEWPORT, RANDOM };

  float LCGRandom();

  Mode mode_;
  int width_;
  int height_;
  uint32_t seed_;
};

}  // namespace cc

#endif  // CC_DEBUG_INVALIDATION_BENCHMARK_H_
