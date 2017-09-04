// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/profiler_metrics_provider.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "base/tracked_objects.h"
#include "testing/gtest/include/gtest/gtest.h"

using tracked_objects::ProcessDataPhaseSnapshot;
using tracked_objects::TaskSnapshot;

namespace metrics {

TEST(ProfilerMetricsProviderTest, RecordData) {
  // WARNING: If you broke the below check, you've modified how
  // HashMetricName works. Please also modify all server-side code that
  // relies on the existing way of hashing.
  EXPECT_EQ(UINT64_C(1518842999910132863),
            base::HashMetricName("birth_thread*"));

  ProfilerMetricsProvider profiler_metrics_provider;

  {
    // Add data from the browser process.
    ProcessDataPhaseSnapshot process_data_phase;
    process_data_phase.tasks.push_back(TaskSnapshot());
    process_data_phase.tasks.back().birth.location.file_name = "a/b/file.h";
    process_data_phase.tasks.back().birth.location.function_name = "function";
    process_data_phase.tasks.back().birth.location.line_number = 1337;
    process_data_phase.tasks.back().birth.thread_name = "birth_thread";
    process_data_phase.tasks.back().death_data.count = 37;
    process_data_phase.tasks.back().death_data.run_duration_sum = 31;
    process_data_phase.tasks.back().death_data.run_duration_max = 17;
    process_data_phase.tasks.back().death_data.run_duration_sample = 13;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 8;
    process_data_phase.tasks.back().death_data.queue_duration_max = 5;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 3;
    process_data_phase.tasks.back().death_thread_name = "Still_Alive";
    process_data_phase.tasks.push_back(TaskSnapshot());
    process_data_phase.tasks.back().birth.location.file_name = "c\\d\\file2";
    process_data_phase.tasks.back().birth.location.function_name = "function2";
    process_data_phase.tasks.back().birth.location.line_number = 1773;
    process_data_phase.tasks.back().birth.thread_name = "birth_thread2";
    process_data_phase.tasks.back().death_data.count = 19;
    process_data_phase.tasks.back().death_data.run_duration_sum = 23;
    process_data_phase.tasks.back().death_data.run_duration_max = 11;
    process_data_phase.tasks.back().death_data.run_duration_sample = 7;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 0;
    process_data_phase.tasks.back().death_data.queue_duration_max = 0;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 0;
    process_data_phase.tasks.back().death_thread_name = "death_thread";

    profiler_metrics_provider.RecordProfilerData(
        process_data_phase, 177, ProfilerEventProto::TrackedObject::BROWSER, 0,
        base::TimeDelta::FromMinutes(1), base::TimeDelta::FromMinutes(2),
        ProfilerEvents());
  }

  {
    // Add second phase from the browser process.
    ProcessDataPhaseSnapshot process_data_phase;
    process_data_phase.tasks.push_back(TaskSnapshot());
    process_data_phase.tasks.back().birth.location.file_name = "a/b/file10.h";
    process_data_phase.tasks.back().birth.location.function_name = "function10";
    process_data_phase.tasks.back().birth.location.line_number = 101337;
    process_data_phase.tasks.back().birth.thread_name = "birth_thread_ten";
    process_data_phase.tasks.back().death_data.count = 1037;
    process_data_phase.tasks.back().death_data.run_duration_sum = 1031;
    process_data_phase.tasks.back().death_data.run_duration_max = 1017;
    process_data_phase.tasks.back().death_data.run_duration_sample = 1013;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 108;
    process_data_phase.tasks.back().death_data.queue_duration_max = 105;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 103;
    process_data_phase.tasks.back().death_thread_name = "Already_Dead";
    process_data_phase.tasks.push_back(TaskSnapshot());
    process_data_phase.tasks.back().birth.location.file_name = "c\\d\\file210";
    process_data_phase.tasks.back().birth.location.function_name =
        "function210";
    process_data_phase.tasks.back().birth.location.line_number = 101773;
    process_data_phase.tasks.back().birth.thread_name = "birth_thread_ten2";
    process_data_phase.tasks.back().death_data.count = 1019;
    process_data_phase.tasks.back().death_data.run_duration_sum = 1023;
    process_data_phase.tasks.back().death_data.run_duration_max = 1011;
    process_data_phase.tasks.back().death_data.run_duration_sample = 107;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 100;
    process_data_phase.tasks.back().death_data.queue_duration_max = 100;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 100;
    process_data_phase.tasks.back().death_thread_name = "death_thread_ten";

    profiler_metrics_provider.RecordProfilerData(
        process_data_phase, 177, ProfilerEventProto::TrackedObject::BROWSER, 1,
        base::TimeDelta::FromMinutes(10), base::TimeDelta::FromMinutes(20),
        ProfilerEvents(1, ProfilerEventProto::EVENT_FIRST_NONEMPTY_PAINT));
  }

  {
    // Add data from a renderer process.
    ProcessDataPhaseSnapshot process_data_phase;
    process_data_phase.tasks.push_back(TaskSnapshot());
    process_data_phase.tasks.back().birth.location.file_name = "file3";
    process_data_phase.tasks.back().birth.location.function_name = "function3";
    process_data_phase.tasks.back().birth.location.line_number = 7331;
    process_data_phase.tasks.back().birth.thread_name = "birth_thread3";
    process_data_phase.tasks.back().death_data.count = 137;
    process_data_phase.tasks.back().death_data.run_duration_sum = 131;
    process_data_phase.tasks.back().death_data.run_duration_max = 117;
    process_data_phase.tasks.back().death_data.run_duration_sample = 113;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 108;
    process_data_phase.tasks.back().death_data.queue_duration_max = 105;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 103;
    process_data_phase.tasks.back().death_thread_name = "death_thread3";
    process_data_phase.tasks.push_back(TaskSnapshot());
    process_data_phase.tasks.back().birth.location.file_name = "";
    process_data_phase.tasks.back().birth.location.function_name = "";
    process_data_phase.tasks.back().birth.location.line_number = 7332;
    process_data_phase.tasks.back().birth.thread_name = "";
    process_data_phase.tasks.back().death_data.count = 138;
    process_data_phase.tasks.back().death_data.run_duration_sum = 132;
    process_data_phase.tasks.back().death_data.run_duration_max = 118;
    process_data_phase.tasks.back().death_data.run_duration_sample = 114;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 109;
    process_data_phase.tasks.back().death_data.queue_duration_max = 106;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 104;
    process_data_phase.tasks.back().death_thread_name = "";

    profiler_metrics_provider.RecordProfilerData(
        process_data_phase, 1177, ProfilerEventProto::TrackedObject::RENDERER,
        0, base::TimeDelta::FromMinutes(1), base::TimeDelta::FromMinutes(2),
        ProfilerEvents());
  }

  // Capture the data and verify that it is as expected.
  ChromeUserMetricsExtension uma_proto;
  profiler_metrics_provider.ProvideGeneralMetrics(&uma_proto);

  // Phase 0
  ASSERT_EQ(2, uma_proto.profiler_event_size());

  EXPECT_EQ(ProfilerEventProto::VERSION_SPLIT_PROFILE,
            uma_proto.profiler_event(0).profile_version());
  EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
            uma_proto.profiler_event(0).time_source());
  ASSERT_EQ(0, uma_proto.profiler_event(0).past_session_event_size());
  ASSERT_EQ(60000, uma_proto.profiler_event(0).profiling_start_ms());
  ASSERT_EQ(120000, uma_proto.profiler_event(0).profiling_finish_ms());
  ASSERT_EQ(4, uma_proto.profiler_event(0).tracked_object_size());

  const ProfilerEventProto::TrackedObject* tracked_object =
      &uma_proto.profiler_event(0).tracked_object(0);
  EXPECT_EQ(base::HashMetricName("file.h"),
            tracked_object->source_file_name_hash());
  EXPECT_EQ(base::HashMetricName("function"),
            tracked_object->source_function_name_hash());
  EXPECT_EQ(1337, tracked_object->source_line_number());
  EXPECT_EQ(base::HashMetricName("birth_thread"),
            tracked_object->birth_thread_name_hash());
  EXPECT_EQ(37, tracked_object->exec_count());
  EXPECT_EQ(31, tracked_object->exec_time_total());
  EXPECT_EQ(13, tracked_object->exec_time_sampled());
  EXPECT_EQ(8, tracked_object->queue_time_total());
  EXPECT_EQ(3, tracked_object->queue_time_sampled());
  EXPECT_EQ(base::HashMetricName("Still_Alive"),
            tracked_object->exec_thread_name_hash());
  EXPECT_EQ(177U, tracked_object->process_id());
  EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
            tracked_object->process_type());

  tracked_object = &uma_proto.profiler_event(0).tracked_object(1);
  EXPECT_EQ(base::HashMetricName("file2"),
            tracked_object->source_file_name_hash());
  EXPECT_EQ(base::HashMetricName("function2"),
            tracked_object->source_function_name_hash());
  EXPECT_EQ(1773, tracked_object->source_line_number());
  EXPECT_EQ(base::HashMetricName("birth_thread*"),
            tracked_object->birth_thread_name_hash());
  EXPECT_EQ(19, tracked_object->exec_count());
  EXPECT_EQ(23, tracked_object->exec_time_total());
  EXPECT_EQ(7, tracked_object->exec_time_sampled());
  EXPECT_EQ(0, tracked_object->queue_time_total());
  EXPECT_EQ(0, tracked_object->queue_time_sampled());
  EXPECT_EQ(base::HashMetricName("death_thread"),
            tracked_object->exec_thread_name_hash());
  EXPECT_EQ(177U, tracked_object->process_id());
  EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
            tracked_object->process_type());

  tracked_object = &uma_proto.profiler_event(0).tracked_object(2);
  EXPECT_EQ(base::HashMetricName("file3"),
            tracked_object->source_file_name_hash());
  EXPECT_EQ(base::HashMetricName("function3"),
            tracked_object->source_function_name_hash());
  EXPECT_EQ(7331, tracked_object->source_line_number());
  EXPECT_EQ(base::HashMetricName("birth_thread*"),
            tracked_object->birth_thread_name_hash());
  EXPECT_EQ(137, tracked_object->exec_count());
  EXPECT_EQ(131, tracked_object->exec_time_total());
  EXPECT_EQ(113, tracked_object->exec_time_sampled());
  EXPECT_EQ(108, tracked_object->queue_time_total());
  EXPECT_EQ(103, tracked_object->queue_time_sampled());
  EXPECT_EQ(base::HashMetricName("death_thread*"),
            tracked_object->exec_thread_name_hash());
  EXPECT_EQ(1177U, tracked_object->process_id());
  EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
            tracked_object->process_type());

  tracked_object = &uma_proto.profiler_event(0).tracked_object(3);
  EXPECT_EQ(base::HashMetricName(""), tracked_object->source_file_name_hash());
  EXPECT_EQ(base::HashMetricName(""),
            tracked_object->source_function_name_hash());
  EXPECT_EQ(7332, tracked_object->source_line_number());
  EXPECT_EQ(base::HashMetricName(""), tracked_object->birth_thread_name_hash());
  EXPECT_EQ(138, tracked_object->exec_count());
  EXPECT_EQ(132, tracked_object->exec_time_total());
  EXPECT_EQ(114, tracked_object->exec_time_sampled());
  EXPECT_EQ(109, tracked_object->queue_time_total());
  EXPECT_EQ(104, tracked_object->queue_time_sampled());
  EXPECT_EQ(base::HashMetricName(""), tracked_object->exec_thread_name_hash());
  EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
            tracked_object->process_type());

  // Phase 1
  EXPECT_EQ(ProfilerEventProto::VERSION_SPLIT_PROFILE,
            uma_proto.profiler_event(1).profile_version());
  EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
            uma_proto.profiler_event(1).time_source());
  ASSERT_EQ(1, uma_proto.profiler_event(1).past_session_event_size());
  ASSERT_EQ(ProfilerEventProto::EVENT_FIRST_NONEMPTY_PAINT,
            uma_proto.profiler_event(1).past_session_event(0));
  ASSERT_EQ(600000, uma_proto.profiler_event(1).profiling_start_ms());
  ASSERT_EQ(1200000, uma_proto.profiler_event(1).profiling_finish_ms());
  ASSERT_EQ(2, uma_proto.profiler_event(1).tracked_object_size());

  tracked_object = &uma_proto.profiler_event(1).tracked_object(0);
  EXPECT_EQ(base::HashMetricName("file10.h"),
            tracked_object->source_file_name_hash());
  EXPECT_EQ(base::HashMetricName("function10"),
            tracked_object->source_function_name_hash());
  EXPECT_EQ(101337, tracked_object->source_line_number());
  EXPECT_EQ(base::HashMetricName("birth_thread_ten"),
            tracked_object->birth_thread_name_hash());
  EXPECT_EQ(1037, tracked_object->exec_count());
  EXPECT_EQ(1031, tracked_object->exec_time_total());
  EXPECT_EQ(1013, tracked_object->exec_time_sampled());
  EXPECT_EQ(108, tracked_object->queue_time_total());
  EXPECT_EQ(103, tracked_object->queue_time_sampled());
  EXPECT_EQ(base::HashMetricName("Already_Dead"),
            tracked_object->exec_thread_name_hash());
  EXPECT_EQ(177U, tracked_object->process_id());
  EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
            tracked_object->process_type());

  tracked_object = &uma_proto.profiler_event(1).tracked_object(1);
  EXPECT_EQ(base::HashMetricName("file210"),
            tracked_object->source_file_name_hash());
  EXPECT_EQ(base::HashMetricName("function210"),
            tracked_object->source_function_name_hash());
  EXPECT_EQ(101773, tracked_object->source_line_number());
  EXPECT_EQ(base::HashMetricName("birth_thread_ten*"),
            tracked_object->birth_thread_name_hash());
  EXPECT_EQ(1019, tracked_object->exec_count());
  EXPECT_EQ(1023, tracked_object->exec_time_total());
  EXPECT_EQ(107, tracked_object->exec_time_sampled());
  EXPECT_EQ(100, tracked_object->queue_time_total());
  EXPECT_EQ(100, tracked_object->queue_time_sampled());
  EXPECT_EQ(base::HashMetricName("death_thread_ten"),
            tracked_object->exec_thread_name_hash());
  EXPECT_EQ(177U, tracked_object->process_id());
  EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
            tracked_object->process_type());
}

}  // namespace metrics
