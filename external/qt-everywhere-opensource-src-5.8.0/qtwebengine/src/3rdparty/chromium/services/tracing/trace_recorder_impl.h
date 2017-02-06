// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACE_RECORDER_IMPL_H_
#define SERVICES_TRACING_TRACE_RECORDER_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/string.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"
#include "services/tracing/trace_data_sink.h"

namespace tracing {

class TraceRecorderImpl : public TraceRecorder {
 public:
  TraceRecorderImpl(mojo::InterfaceRequest<TraceRecorder> request,
                    TraceDataSink* sink);
  ~TraceRecorderImpl() override;

  // TryRead attempts to read a single chunk from the TraceRecorder pipe if one
  // is available and passes it to the TraceDataSink. Returns immediately if no
  // chunk is available.
  void TryRead();

  // TraceRecorderHandle returns the handle value of the TraceRecorder
  // binding which can be used to wait until chunks are available.
  mojo::Handle TraceRecorderHandle() const;

 private:
  // tracing::TraceRecorder implementation.
  void Record(const mojo::String& json) override;

  TraceDataSink* sink_;
  mojo::Binding<TraceRecorder> binding_;

  DISALLOW_COPY_AND_ASSIGN(TraceRecorderImpl);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACE_RECORDER_IMPL_H_
