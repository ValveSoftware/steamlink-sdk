// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/unittest_only_benchmark_impl.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"

namespace cc {

UnittestOnlyBenchmarkImpl::UnittestOnlyBenchmarkImpl(
    scoped_refptr<base::MessageLoopProxy> origin_loop,
    base::Value* settings,
    const DoneCallback& callback)
    : MicroBenchmarkImpl(callback, origin_loop) {}

UnittestOnlyBenchmarkImpl::~UnittestOnlyBenchmarkImpl() {}

void UnittestOnlyBenchmarkImpl::DidCompleteCommit(LayerTreeHostImpl* host) {
  NotifyDone(scoped_ptr<base::Value>());
}

}  // namespace cc
