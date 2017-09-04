// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/protobuf_to_mojo_converter.h"

namespace metrics {
namespace leak_detector {
namespace protobuf_to_mojo_converter {

void ParamsToMojo(const MemoryLeakReportProto::Params& params,
                  mojom::LeakDetectorParams* mojo_params) {
  mojo_params->sampling_rate = params.sampling_rate();
  mojo_params->max_stack_depth = params.max_stack_depth();
  mojo_params->analysis_interval_bytes = params.analysis_interval_bytes();
  mojo_params->size_suspicion_threshold = params.size_suspicion_threshold();
  mojo_params->call_stack_suspicion_threshold =
      params.call_stack_suspicion_threshold();
}

void MojoToParams(const mojom::LeakDetectorParams& mojo_params,
                  MemoryLeakReportProto::Params* params) {
  params->set_sampling_rate(mojo_params.sampling_rate);
  params->set_max_stack_depth(mojo_params.max_stack_depth);
  params->set_analysis_interval_bytes(mojo_params.analysis_interval_bytes);
  params->set_size_suspicion_threshold(mojo_params.size_suspicion_threshold);
  params->set_call_stack_suspicion_threshold(
      mojo_params.call_stack_suspicion_threshold);
}

void ReportToMojo(const MemoryLeakReportProto& report,
                  mojom::MemoryLeakReport* mojo_report) {
  mojo_report->size_bytes = report.size_bytes();
  mojo_report->num_rising_intervals = report.num_rising_intervals();
  mojo_report->num_allocs_increase = report.num_allocs_increase();
  for (auto call_stack_value : report.call_stack()) {
    mojo_report->call_stack.push_back(call_stack_value);
  }

  for (const auto& history_entry : report.alloc_breakdown_history()) {
    metrics::mojom::AllocationBreakdownPtr mojo_entry =
        metrics::mojom::AllocationBreakdown::New();
    for (auto count : history_entry.counts_by_size()) {
      mojo_entry->counts_by_size.push_back(count);
    }
    mojo_entry->count_for_call_stack = history_entry.count_for_call_stack();

    mojo_report->alloc_breakdown_history.push_back(std::move(mojo_entry));
  }
}

void MojoToReport(const mojom::MemoryLeakReport& mojo_report,
                  MemoryLeakReportProto* report) {
  report->set_size_bytes(mojo_report.size_bytes);
  report->set_num_rising_intervals(mojo_report.num_rising_intervals);
  report->set_num_allocs_increase(mojo_report.num_allocs_increase);
  for (auto call_stack_addr : mojo_report.call_stack)
    report->add_call_stack(call_stack_addr);

  for (const auto& history_entry : mojo_report.alloc_breakdown_history) {
    auto proto_entry = report->add_alloc_breakdown_history();
    for (auto count : history_entry->counts_by_size) {
      proto_entry->add_counts_by_size(count);
    }
    proto_entry->set_count_for_call_stack(history_entry->count_for_call_stack);
  }
}

}  // namespace protobuf_to_mojo_converter
}  // namespace leak_detector
}  // namespace metrics
