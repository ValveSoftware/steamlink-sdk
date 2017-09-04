// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/protobuf_to_mojo_converter.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace leak_detector {

TEST(protobuf_to_mojo_converterTest, ConvertParams) {
  MemoryLeakReportProto::Params params;
  params.set_sampling_rate(0.75);
  params.set_max_stack_depth(19);
  params.set_analysis_interval_bytes(25 * 1024 * 1024);
  params.set_size_suspicion_threshold(8);
  params.set_call_stack_suspicion_threshold(11);

  // Convert to equivalent Mojo struct.
  mojo::StructPtr<mojom::LeakDetectorParams> mojo_params =
      mojom::LeakDetectorParams::New();
  protobuf_to_mojo_converter::ParamsToMojo(params, mojo_params.get());

  EXPECT_DOUBLE_EQ(0.75, mojo_params->sampling_rate);
  EXPECT_EQ(19U, mojo_params->max_stack_depth);
  EXPECT_EQ(25U * 1024 * 1024, mojo_params->analysis_interval_bytes);
  EXPECT_EQ(8U, mojo_params->size_suspicion_threshold);
  EXPECT_EQ(11U, mojo_params->call_stack_suspicion_threshold);

  // Convert Mojo struct back to protobuf.
  MemoryLeakReportProto::Params new_params;
  protobuf_to_mojo_converter::MojoToParams(*mojo_params, &new_params);

  EXPECT_DOUBLE_EQ(0.75, new_params.sampling_rate());
  EXPECT_EQ(19U, new_params.max_stack_depth());
  EXPECT_EQ(25U * 1024 * 1024, new_params.analysis_interval_bytes());
  EXPECT_EQ(8U, new_params.size_suspicion_threshold());
  EXPECT_EQ(11U, new_params.call_stack_suspicion_threshold());
}

TEST(protobuf_to_mojo_converterTest, ConvertReport) {
  MemoryLeakReportProto report;
  report.add_call_stack(0xdeadbeef);
  report.add_call_stack(0xc001d00d);
  report.add_call_stack(0x900df00d);
  report.set_size_bytes(24);
  report.set_num_rising_intervals(5);
  report.set_num_allocs_increase(42);

  auto entry1 = report.add_alloc_breakdown_history();
  entry1->add_counts_by_size(1);
  entry1->add_counts_by_size(2);
  entry1->add_counts_by_size(3);
  entry1->set_count_for_call_stack(4);

  auto entry2 = report.add_alloc_breakdown_history();
  entry2->add_counts_by_size(11);
  entry2->add_counts_by_size(12);
  entry2->add_counts_by_size(13);
  entry2->add_counts_by_size(14);
  entry2->set_count_for_call_stack(15);

  auto entry3 = report.add_alloc_breakdown_history();
  entry3->add_counts_by_size(21);
  entry3->add_counts_by_size(22);
  entry3->add_counts_by_size(23);
  entry3->add_counts_by_size(24);
  entry3->add_counts_by_size(25);
  entry3->set_count_for_call_stack(26);

  // Convert to equivalent Mojo struct.
  mojo::StructPtr<mojom::MemoryLeakReport> mojo_report =
      mojom::MemoryLeakReport::New();
  protobuf_to_mojo_converter::ReportToMojo(report, mojo_report.get());

  ASSERT_EQ(3U, mojo_report->call_stack.size());
  EXPECT_EQ(0xdeadbeef, mojo_report->call_stack[0]);
  EXPECT_EQ(0xc001d00d, mojo_report->call_stack[1]);
  EXPECT_EQ(0x900df00d, mojo_report->call_stack[2]);
  EXPECT_EQ(24U, mojo_report->size_bytes);
  EXPECT_EQ(5U, mojo_report->num_rising_intervals);
  EXPECT_EQ(42U, mojo_report->num_allocs_increase);

  ASSERT_EQ(3U, mojo_report->alloc_breakdown_history.size());

  ASSERT_EQ(3U, mojo_report->alloc_breakdown_history[0]->counts_by_size.size());
  EXPECT_EQ(1U, mojo_report->alloc_breakdown_history[0]->counts_by_size[0]);
  EXPECT_EQ(2U, mojo_report->alloc_breakdown_history[0]->counts_by_size[1]);
  EXPECT_EQ(3U, mojo_report->alloc_breakdown_history[0]->counts_by_size[2]);
  EXPECT_EQ(4U, mojo_report->alloc_breakdown_history[0]->count_for_call_stack);

  ASSERT_EQ(4U, mojo_report->alloc_breakdown_history[1]->counts_by_size.size());
  EXPECT_EQ(11U, mojo_report->alloc_breakdown_history[1]->counts_by_size[0]);
  EXPECT_EQ(12U, mojo_report->alloc_breakdown_history[1]->counts_by_size[1]);
  EXPECT_EQ(13U, mojo_report->alloc_breakdown_history[1]->counts_by_size[2]);
  EXPECT_EQ(14U, mojo_report->alloc_breakdown_history[1]->counts_by_size[3]);
  EXPECT_EQ(15U, mojo_report->alloc_breakdown_history[1]->count_for_call_stack);

  ASSERT_EQ(5U, mojo_report->alloc_breakdown_history[2]->counts_by_size.size());
  EXPECT_EQ(21U, mojo_report->alloc_breakdown_history[2]->counts_by_size[0]);
  EXPECT_EQ(22U, mojo_report->alloc_breakdown_history[2]->counts_by_size[1]);
  EXPECT_EQ(23U, mojo_report->alloc_breakdown_history[2]->counts_by_size[2]);
  EXPECT_EQ(24U, mojo_report->alloc_breakdown_history[2]->counts_by_size[3]);
  EXPECT_EQ(25U, mojo_report->alloc_breakdown_history[2]->counts_by_size[4]);
  EXPECT_EQ(26U, mojo_report->alloc_breakdown_history[2]->count_for_call_stack);

  // Convert Mojo struct back to protobuf.
  MemoryLeakReportProto new_report;
  protobuf_to_mojo_converter::MojoToReport(*mojo_report, &new_report);

  ASSERT_EQ(3, new_report.call_stack().size());
  EXPECT_EQ(0xdeadbeef, new_report.call_stack(0));
  EXPECT_EQ(0xc001d00d, new_report.call_stack(1));
  EXPECT_EQ(0x900df00d, new_report.call_stack(2));
  EXPECT_EQ(24U, new_report.size_bytes());
  EXPECT_EQ(5U, new_report.num_rising_intervals());
  EXPECT_EQ(42U, new_report.num_allocs_increase());

  ASSERT_EQ(3, new_report.alloc_breakdown_history().size());

  ASSERT_EQ(3, new_report.alloc_breakdown_history(0).counts_by_size().size());
  EXPECT_EQ(1U, new_report.alloc_breakdown_history(0).counts_by_size(0));
  EXPECT_EQ(2U, new_report.alloc_breakdown_history(0).counts_by_size(1));
  EXPECT_EQ(3U, new_report.alloc_breakdown_history(0).counts_by_size(2));
  EXPECT_EQ(4U, new_report.alloc_breakdown_history(0).count_for_call_stack());

  ASSERT_EQ(4, new_report.alloc_breakdown_history(1).counts_by_size().size());
  EXPECT_EQ(11U, new_report.alloc_breakdown_history(1).counts_by_size(0));
  EXPECT_EQ(12U, new_report.alloc_breakdown_history(1).counts_by_size(1));
  EXPECT_EQ(13U, new_report.alloc_breakdown_history(1).counts_by_size(2));
  EXPECT_EQ(14U, new_report.alloc_breakdown_history(1).counts_by_size(3));
  EXPECT_EQ(15U, new_report.alloc_breakdown_history(1).count_for_call_stack());

  ASSERT_EQ(5, new_report.alloc_breakdown_history(2).counts_by_size().size());
  EXPECT_EQ(21U, new_report.alloc_breakdown_history(2).counts_by_size(0));
  EXPECT_EQ(22U, new_report.alloc_breakdown_history(2).counts_by_size(1));
  EXPECT_EQ(23U, new_report.alloc_breakdown_history(2).counts_by_size(2));
  EXPECT_EQ(24U, new_report.alloc_breakdown_history(2).counts_by_size(3));
  EXPECT_EQ(25U, new_report.alloc_breakdown_history(2).counts_by_size(4));
  EXPECT_EQ(26U, new_report.alloc_breakdown_history(2).count_for_call_stack());
}

}  // namespace leak_detector
}  // namespace metrics
