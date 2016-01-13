// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/micro_benchmark_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"

namespace cc {

namespace {

void RunCallback(const MicroBenchmarkImpl::DoneCallback& callback,
                 scoped_ptr<base::Value> result) {
  callback.Run(result.Pass());
}

}

MicroBenchmarkImpl::MicroBenchmarkImpl(
    const DoneCallback& callback,
    scoped_refptr<base::MessageLoopProxy> origin_loop)
    : callback_(callback), is_done_(false), origin_loop_(origin_loop) {}

MicroBenchmarkImpl::~MicroBenchmarkImpl() {}

bool MicroBenchmarkImpl::IsDone() const {
  return is_done_;
}

void MicroBenchmarkImpl::DidCompleteCommit(LayerTreeHostImpl* host) {}

void MicroBenchmarkImpl::NotifyDone(scoped_ptr<base::Value> result) {
  origin_loop_->PostTask(
    FROM_HERE,
    base::Bind(RunCallback, callback_, base::Passed(&result)));
  is_done_ = true;
}

void MicroBenchmarkImpl::RunOnLayer(LayerImpl* layer) {}

void MicroBenchmarkImpl::RunOnLayer(PictureLayerImpl* layer) {}

}  // namespace cc
