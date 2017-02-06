// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/v8_platform.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
#include "base/trace_event/trace_event.h"
#include "gin/per_isolate_data.h"

namespace gin {

namespace {

base::LazyInstance<V8Platform>::Leaky g_v8_platform = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
V8Platform* V8Platform::Get() { return g_v8_platform.Pointer(); }

V8Platform::V8Platform() {}

V8Platform::~V8Platform() {}

size_t V8Platform::NumberOfAvailableBackgroundThreads() {
  // WorkerPool will currently always create additional threads for posted
  // background tasks, unless there are threads sitting idle (on posix).
  // Indicate that V8 should create no more than the number of cores available,
  // reserving one core for the main thread.
  const size_t available_cores =
    static_cast<size_t>(base::SysInfo::NumberOfProcessors());
  if (available_cores > 1) {
    return available_cores - 1;
  }
  return 1;
}

void V8Platform::CallOnBackgroundThread(
    v8::Task* task,
    v8::Platform::ExpectedRuntime expected_runtime) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&v8::Task::Run, base::Owned(task)),
      expected_runtime == v8::Platform::kLongRunningTask);
}

void V8Platform::CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) {
  PerIsolateData::From(isolate)->task_runner()->PostTask(
      FROM_HERE, base::Bind(&v8::Task::Run, base::Owned(task)));
}

void V8Platform::CallDelayedOnForegroundThread(v8::Isolate* isolate,
                                               v8::Task* task,
                                               double delay_in_seconds) {
  PerIsolateData::From(isolate)->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&v8::Task::Run, base::Owned(task)),
      base::TimeDelta::FromSecondsD(delay_in_seconds));
}

void V8Platform::CallIdleOnForegroundThread(v8::Isolate* isolate,
                                            v8::IdleTask* task) {
  DCHECK(PerIsolateData::From(isolate)->idle_task_runner());
  PerIsolateData::From(isolate)->idle_task_runner()->PostIdleTask(task);
}

bool V8Platform::IdleTasksEnabled(v8::Isolate* isolate) {
  return PerIsolateData::From(isolate)->idle_task_runner() != nullptr;
}

double V8Platform::MonotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

const uint8_t* V8Platform::GetCategoryGroupEnabled(const char* name) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(name);
}

const char* V8Platform::GetCategoryGroupName(
    const uint8_t* category_enabled_flag) {
  return base::trace_event::TraceLog::GetCategoryGroupName(
      category_enabled_flag);
}

uint64_t V8Platform::AddTraceEvent(char phase,
                                   const uint8_t* category_enabled_flag,
                                   const char* name,
                                   const char* scope,
                                   uint64_t id,
                                   uint64_t bind_id,
                                   int32_t num_args,
                                   const char** arg_names,
                                   const uint8_t* arg_types,
                                   const uint64_t* arg_values,
                                   unsigned int flags) {
  base::trace_event::TraceEventHandle handle =
      TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID(
          phase, category_enabled_flag, name, scope, id, bind_id, num_args,
          arg_names, arg_types, (const long long unsigned int*)arg_values, NULL,
          flags);
  uint64_t result;
  memcpy(&result, &handle, sizeof(result));
  return result;
}

void V8Platform::UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                          const char* name,
                                          uint64_t handle) {
  base::trace_event::TraceEventHandle traceEventHandle;
  memcpy(&traceEventHandle, &handle, sizeof(handle));
  TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(category_enabled_flag, name,
                                              traceEventHandle);
}

}  // namespace gin
