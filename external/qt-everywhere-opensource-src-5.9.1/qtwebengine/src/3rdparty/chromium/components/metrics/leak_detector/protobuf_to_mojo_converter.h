// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_PROTOBUF_TO_MOJO_CONVERTER_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_PROTOBUF_TO_MOJO_CONVERTER_H_

#include "components/metrics/leak_detector/leak_detector.mojom.h"
#include "components/metrics/proto/memory_leak_report.pb.h"

namespace metrics {
namespace leak_detector {
namespace protobuf_to_mojo_converter {

// Converts between a MemoryLeakReportProto::Params protobuf and a
// mojom::LeakDetectorParams Mojo structure. The Mojo structure must already be
// allocated.
void ParamsToMojo(const MemoryLeakReportProto::Params& params,
                  mojom::LeakDetectorParams* mojo_params);
void MojoToParams(const mojom::LeakDetectorParams& mojo_params,
                  MemoryLeakReportProto::Params* params);

// Converts between a MemoryLeakReportProto protobuf and a
// mojom::MemoryLeakReport Mojo structure. The Mojo structure must already be
// allocated. The conversion only covers the fields that are filled in by the
// LeakDetector class.
void ReportToMojo(const MemoryLeakReportProto& report,
                  mojom::MemoryLeakReport* mojo_report);
void MojoToReport(const mojom::MemoryLeakReport& mojo_report,
                  MemoryLeakReportProto* report);

}  // namespace protobuf_to_mojo_converter
}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_PROTOBUF_TO_MOJO_CONVERTER_H_
