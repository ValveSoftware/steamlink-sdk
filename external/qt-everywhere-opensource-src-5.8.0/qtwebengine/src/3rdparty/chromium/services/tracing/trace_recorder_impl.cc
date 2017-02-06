// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/trace_recorder_impl.h"

#include <utility>

namespace tracing {

TraceRecorderImpl::TraceRecorderImpl(
    mojo::InterfaceRequest<TraceRecorder> request,
    TraceDataSink* sink)
    : sink_(sink), binding_(this, std::move(request)) {}

TraceRecorderImpl::~TraceRecorderImpl() {
}

void TraceRecorderImpl::TryRead() {
  binding_.WaitForIncomingMethodCall(MojoDeadline(0));
}

mojo::Handle TraceRecorderImpl::TraceRecorderHandle() const {
  return binding_.handle();
}

void TraceRecorderImpl::Record(const mojo::String& json) {
  sink_->AddChunk(json.To<std::string>());
}

}  // namespace tracing
