// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/trace_data_sink.h"

#include <utility>

#include "base/logging.h"
#include "mojo/common/data_pipe_utils.h"

using mojo::common::BlockingCopyFromString;

namespace tracing {

TraceDataSink::TraceDataSink(mojo::ScopedDataPipeProducerHandle pipe)
    : pipe_(std::move(pipe)), empty_(true) {}

TraceDataSink::~TraceDataSink() {
  if (pipe_.is_valid())
    pipe_.reset();
  DCHECK(!pipe_.is_valid());
}

void TraceDataSink::AddChunk(const std::string& json) {
  if (!empty_)
    BlockingCopyFromString(",", pipe_);
  empty_ = false;
  BlockingCopyFromString(json, pipe_);
}

}  // namespace tracing
