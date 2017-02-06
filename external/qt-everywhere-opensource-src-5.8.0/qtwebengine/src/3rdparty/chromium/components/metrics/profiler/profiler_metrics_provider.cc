// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/profiler_metrics_provider.h"

#include <ctype.h>
#include <stddef.h>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/tracked_objects.h"
#include "components/metrics/metrics_log.h"

namespace metrics {
namespace {

// Maps a thread name by replacing trailing sequence of digits with "*".
// Examples:
// 1. "BrowserBlockingWorker1/23857" => "BrowserBlockingWorker1/*"
// 2. "Chrome_IOThread" => "Chrome_IOThread"
std::string MapThreadName(const std::string& thread_name) {
  size_t i = thread_name.length();

  while (i > 0 && isdigit(thread_name[i - 1])) {
    --i;
  }

  if (i == thread_name.length())
    return thread_name;

  return thread_name.substr(0, i) + '*';
}

// Normalizes a source filename (which is platform- and build-method-dependent)
// by extracting the last component of the full file name.
// Example: "c:\b\build\slave\win\build\src\chrome\app\chrome_main.cc" =>
// "chrome_main.cc".
std::string NormalizeFileName(const std::string& file_name) {
  const size_t offset = file_name.find_last_of("\\/");
  return offset != std::string::npos ? file_name.substr(offset + 1) : file_name;
}

void WriteProfilerData(
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    base::ProcessId process_id,
    ProfilerEventProto::TrackedObject::ProcessType process_type,
    ProfilerEventProto* performance_profile) {
  for (const auto& task : process_data_phase.tasks) {
    const tracked_objects::DeathDataSnapshot& death_data = task.death_data;
    ProfilerEventProto::TrackedObject* tracked_object =
        performance_profile->add_tracked_object();
    tracked_object->set_birth_thread_name_hash(
        MetricsLog::Hash(MapThreadName(task.birth.thread_name)));
    tracked_object->set_exec_thread_name_hash(
        MetricsLog::Hash(MapThreadName(task.death_thread_name)));
    tracked_object->set_source_file_name_hash(
        MetricsLog::Hash(NormalizeFileName(task.birth.location.file_name)));
    tracked_object->set_source_function_name_hash(
        MetricsLog::Hash(task.birth.location.function_name));
    tracked_object->set_source_line_number(task.birth.location.line_number);
    tracked_object->set_exec_count(death_data.count);
    tracked_object->set_exec_time_total(death_data.run_duration_sum);
    tracked_object->set_exec_time_sampled(death_data.run_duration_sample);
    tracked_object->set_queue_time_total(death_data.queue_duration_sum);
    tracked_object->set_queue_time_sampled(death_data.queue_duration_sample);
    tracked_object->set_process_type(process_type);
    tracked_object->set_process_id(process_id);
  }
}

}  // namespace

ProfilerMetricsProvider::ProfilerMetricsProvider() {
}

ProfilerMetricsProvider::ProfilerMetricsProvider(
    const base::Callback<bool(void)>& cellular_callback)
    : cellular_callback_(cellular_callback) {
}

ProfilerMetricsProvider::~ProfilerMetricsProvider() {
}

void ProfilerMetricsProvider::ProvideGeneralMetrics(
    ChromeUserMetricsExtension* uma_proto) {
  DCHECK_EQ(0, uma_proto->profiler_event_size());

  for (auto& event : profiler_events_cache_) {
    uma_proto->add_profiler_event()->Swap(&event.second);
  }

  profiler_events_cache_.clear();
}

void ProfilerMetricsProvider::RecordProfilerData(
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    base::ProcessId process_id,
    ProfilerEventProto::TrackedObject::ProcessType process_type,
    int profiling_phase,
    base::TimeDelta phase_start,
    base::TimeDelta phase_finish,
    const ProfilerEvents& past_events) {
  // Omit profiler data on connections where it's likely to cost the user money
  // for us to upload it.
  if (IsCellularLogicEnabled())
    return;

  const bool new_phase = !ContainsKey(profiler_events_cache_, profiling_phase);
  ProfilerEventProto* profiler_event = &profiler_events_cache_[profiling_phase];

  if (new_phase) {
    profiler_event->set_profile_version(
        ProfilerEventProto::VERSION_SPLIT_PROFILE);
    profiler_event->set_time_source(ProfilerEventProto::WALL_CLOCK_TIME);
    profiler_event->set_profiling_start_ms(phase_start.InMilliseconds());
    profiler_event->set_profiling_finish_ms(phase_finish.InMilliseconds());
    for (const auto& event : past_events) {
      profiler_event->add_past_session_event(event);
    }
  }

  WriteProfilerData(process_data_phase, process_id, process_type,
                    profiler_event);
}

bool ProfilerMetricsProvider::IsCellularLogicEnabled() {
  if (cellular_callback_.is_null())
    return false;

  return cellular_callback_.Run();
}

}  // namespace metrics
