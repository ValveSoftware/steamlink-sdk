// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_UNITTEST_ONLY_BENCHMARK_H_
#define CC_DEBUG_UNITTEST_ONLY_BENCHMARK_H_

#include "base/memory/weak_ptr.h"
#include "cc/debug/micro_benchmark.h"

namespace cc {

class CC_EXPORT UnittestOnlyBenchmark : public MicroBenchmark {
 public:
  UnittestOnlyBenchmark(scoped_ptr<base::Value> value,
                        const DoneCallback& callback);
  virtual ~UnittestOnlyBenchmark();

  virtual void DidUpdateLayers(LayerTreeHost* host) OVERRIDE;
  virtual bool ProcessMessage(scoped_ptr<base::Value> value) OVERRIDE;

 protected:
  virtual scoped_ptr<MicroBenchmarkImpl> CreateBenchmarkImpl(
      scoped_refptr<base::MessageLoopProxy> origin_loop) OVERRIDE;

 private:
  void RecordImplResults(scoped_ptr<base::Value> results);

  bool create_impl_benchmark_;
  base::WeakPtrFactory<UnittestOnlyBenchmark> weak_ptr_factory_;
};

}  // namespace cc

#endif  // CC_DEBUG_UNITTEST_ONLY_BENCHMARK_H_
