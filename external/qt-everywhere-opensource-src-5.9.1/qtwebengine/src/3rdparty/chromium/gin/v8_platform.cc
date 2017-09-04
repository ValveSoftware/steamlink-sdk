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

void RunWithLocker(v8::Isolate* isolate, v8::Task* task) {
  v8::Locker lock(isolate);
  task->Run();
}

class IdleTaskWithLocker : public v8::IdleTask {
 public:
  IdleTaskWithLocker(v8::Isolate* isolate, v8::IdleTask* task)
      : isolate_(isolate), task_(task) {}

  ~IdleTaskWithLocker() override = default;

  // v8::IdleTask implementation.
  void Run(double deadline_in_seconds) override {
    v8::Locker lock(isolate_);
    task_->Run(deadline_in_seconds);
  }

 private:
  v8::Isolate* isolate_;
  std::unique_ptr<v8::IdleTask> task_;

  DISALLOW_COPY_AND_ASSIGN(IdleTaskWithLocker);
};

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
  PerIsolateData* data = PerIsolateData::From(isolate);
  if (data->access_mode() == IsolateHolder::kUseLocker) {
    data->task_runner()->PostTask(
        FROM_HERE, base::Bind(RunWithLocker, base::Unretained(isolate),
                              base::Owned(task)));
  } else {
    data->task_runner()->PostTask(
        FROM_HERE, base::Bind(&v8::Task::Run, base::Owned(task)));
  }
}

void V8Platform::CallDelayedOnForegroundThread(v8::Isolate* isolate,
                                               v8::Task* task,
                                               double delay_in_seconds) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  if (data->access_mode() == IsolateHolder::kUseLocker) {
    data->task_runner()->PostDelayedTask(
        FROM_HERE,
        base::Bind(RunWithLocker, base::Unretained(isolate), base::Owned(task)),
        base::TimeDelta::FromSecondsD(delay_in_seconds));
  } else {
    data->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&v8::Task::Run, base::Owned(task)),
        base::TimeDelta::FromSecondsD(delay_in_seconds));
  }
}

void V8Platform::CallIdleOnForegroundThread(v8::Isolate* isolate,
                                            v8::IdleTask* task) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  DCHECK(data->idle_task_runner());
  if (data->access_mode() == IsolateHolder::kUseLocker) {
    data->idle_task_runner()->PostIdleTask(
        new IdleTaskWithLocker(isolate, task));
  } else {
    data->idle_task_runner()->PostIdleTask(task);
  }
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

namespace {

class ConvertableToTraceFormatWrapper
    : public base::trace_event::ConvertableToTraceFormat {
 public:
  explicit ConvertableToTraceFormatWrapper(
      std::unique_ptr<v8::ConvertableToTraceFormat>& inner)
      : inner_(std::move(inner)) {}
  ~ConvertableToTraceFormatWrapper() override = default;
  void AppendAsTraceFormat(std::string* out) const final {
    inner_->AppendAsTraceFormat(out);
  }

 private:
  std::unique_ptr<v8::ConvertableToTraceFormat> inner_;

  DISALLOW_COPY_AND_ASSIGN(ConvertableToTraceFormatWrapper);
};

}  // namespace

uint64_t V8Platform::AddTraceEvent(
    char phase,
    const uint8_t* category_enabled_flag,
    const char* name,
    const char* scope,
    uint64_t id,
    uint64_t bind_id,
    int32_t num_args,
    const char** arg_names,
    const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags) {
  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> convertables[2];
  if (num_args > 0 && arg_types[0] == TRACE_VALUE_TYPE_CONVERTABLE) {
    convertables[0].reset(
        new ConvertableToTraceFormatWrapper(arg_convertables[0]));
  }
  if (num_args > 1 && arg_types[1] == TRACE_VALUE_TYPE_CONVERTABLE) {
    convertables[1].reset(
        new ConvertableToTraceFormatWrapper(arg_convertables[1]));
  }
  DCHECK_LE(num_args, 2);
  base::trace_event::TraceEventHandle handle =
      TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_BIND_ID(
          phase, category_enabled_flag, name, scope, id, bind_id, num_args,
          arg_names, arg_types, (const long long unsigned int*)arg_values,
          convertables, flags);
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

namespace {

class EnabledStateObserverImpl final
    : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  EnabledStateObserverImpl() = default;

  void OnTraceLogEnabled() final {
    base::AutoLock lock(mutex_);
    for (auto o : observers_) {
      o->OnTraceEnabled();
    }
  }

  void OnTraceLogDisabled() final {
    base::AutoLock lock(mutex_);
    for (auto o : observers_) {
      o->OnTraceDisabled();
    }
  }

  void AddObserver(v8::Platform::TraceStateObserver* observer) {
    base::AutoLock lock(mutex_);
    DCHECK(!observers_.count(observer));
    observers_.insert(observer);
    if (observers_.size() == 1) {
      base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
    }
  }

  void RemoveObserver(v8::Platform::TraceStateObserver* observer) {
    base::AutoLock lock(mutex_);
    DCHECK(observers_.count(observer) == 1);
    observers_.erase(observer);
    if (observers_.empty()) {
      base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(
          this);
    }
  }

 private:
  base::Lock mutex_;
  std::unordered_set<v8::Platform::TraceStateObserver*> observers_;

  DISALLOW_COPY_AND_ASSIGN(EnabledStateObserverImpl);
};

base::LazyInstance<EnabledStateObserverImpl>::Leaky g_trace_state_dispatcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

void V8Platform::AddTraceStateObserver(
    v8::Platform::TraceStateObserver* observer) {
  g_trace_state_dispatcher.Get().AddObserver(observer);
}

void V8Platform::RemoveTraceStateObserver(
    v8::Platform::TraceStateObserver* observer) {
  g_trace_state_dispatcher.Get().RemoveObserver(observer);
}

}  // namespace gin
