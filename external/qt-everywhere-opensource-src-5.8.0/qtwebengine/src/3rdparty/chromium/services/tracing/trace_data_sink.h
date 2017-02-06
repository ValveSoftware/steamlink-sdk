// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACE_DATA_SINK_H_
#define SERVICES_TRACING_TRACE_DATA_SINK_H_

#include <string>

#include "base/macros.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace tracing {

class TraceDataSink {
 public:
  explicit TraceDataSink(mojo::ScopedDataPipeProducerHandle pipe);
  ~TraceDataSink();

  void AddChunk(const std::string& json);

 private:
  mojo::ScopedDataPipeProducerHandle pipe_;
  bool empty_;

  DISALLOW_COPY_AND_ASSIGN(TraceDataSink);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACE_DATA_SINK_H_
