// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/recorder.h"

namespace tracing {

Recorder::Recorder(mojom::RecorderRequest request, DataSink* sink)
  : sink_(sink), binding_(this, std::move(request)) {}
Recorder::~Recorder() {}

void Recorder::TryRead() {
  binding_.WaitForIncomingMethodCall(MojoDeadline(0));
}

mojo::Handle Recorder::RecorderHandle() const {
  return binding_.handle();
}

void Recorder::Record(const std::string& json) {
  sink_->AddChunk(json);
}

}  // namespace tracing
