// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_RECORDER_H_
#define SERVICES_TRACING_RECORDER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/string.h"
#include "services/tracing/data_sink.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"

namespace tracing {

class Recorder : public mojom::Recorder {
 public:
  Recorder(mojom::RecorderRequest request, DataSink* sink);
  ~Recorder() override;

  // TryRead attempts to read a single chunk from the Recorder pipe if one is
  // available and passes it to the DataSink. Returns immediately if no
  // chunk is available.
  void TryRead();

  // RecorderHandle returns the handle value of the Recorder binding which can
  // be used to wait until chunks are available.
  mojo::Handle RecorderHandle() const;

 private:
  // mojom::Recorder implementation.
  void Record(const std::string& json) override;

  DataSink* sink_;
  mojo::Binding<mojom::Recorder> binding_;

  DISALLOW_COPY_AND_ASSIGN(Recorder);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_RECORDER_H_
