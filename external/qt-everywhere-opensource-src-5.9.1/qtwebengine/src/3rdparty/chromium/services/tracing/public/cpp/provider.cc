// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/provider.h"

#include <utility>

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/tracing/public/cpp/switches.h"
#include "services/tracing/public/interfaces/constants.mojom.h"

namespace tracing {
namespace {

// Controls access to |g_tracing_singleton_created|, which can be accessed from
// different threads.
base::LazyInstance<base::Lock>::Leaky g_singleton_lock =
    LAZY_INSTANCE_INITIALIZER;

// Whether we are the first TracingImpl to be created in this service. The first
// TracingImpl in a physical service connects to the tracing service.
bool g_tracing_singleton_created = false;

}

Provider::Provider()
    : binding_(this), tracing_forced_(false), weak_factory_(this) {}

Provider::~Provider() {
  StopTracing();
}

void Provider::InitializeWithFactoryInternal(mojom::FactoryPtr* factory) {
  mojom::ProviderPtr provider;
  Bind(GetProxy(&provider));
  (*factory)->CreateRecorder(std::move(provider));
#ifdef NDEBUG
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEarlyTracing)) {
    ForceEnableTracing();
  }
#else
  ForceEnableTracing();
#endif
}

void Provider::InitializeWithFactory(mojom::FactoryPtr* factory) {
  {
    base::AutoLock lock(g_singleton_lock.Get());
    if (g_tracing_singleton_created)
      return;
    g_tracing_singleton_created = true;
  }
  InitializeWithFactoryInternal(factory);
}

void Provider::Initialize(service_manager::Connector* connector,
                          const std::string& url) {
  {
    base::AutoLock lock(g_singleton_lock.Get());
    if (g_tracing_singleton_created)
      return;
    g_tracing_singleton_created = true;
  }
  mojom::FactoryPtr factory;
  connector->ConnectToInterface(tracing::mojom::kServiceName, &factory);
  InitializeWithFactoryInternal(&factory);
  // This will only set the name for the first app in a loaded mojo file. It's
  // up to something like CoreServices to name its own child threads.
  base::PlatformThread::SetName(url);
}

void Provider::Bind(mojom::ProviderRequest request) {
  if (!binding_.is_bound()) {
    binding_.Bind(std::move(request));
  } else {
    LOG(ERROR) << "Cannot accept two connections to TraceProvider.";
  }
}

void Provider::StartTracing(const std::string& categories,
                            mojom::RecorderPtr recorder) {
  DCHECK(!recorder_);
  recorder_ = std::move(recorder);
  tracing_forced_ = false;
  if (!base::trace_event::TraceLog::GetInstance()->IsEnabled()) {
    base::trace_event::TraceLog::GetInstance()->SetEnabled(
        base::trace_event::TraceConfig(categories,
                                       base::trace_event::RECORD_UNTIL_FULL),
        base::trace_event::TraceLog::RECORDING_MODE);
  }
}

void Provider::StopTracing() {
  if (recorder_) {
    base::trace_event::TraceLog::GetInstance()->SetDisabled();

    base::trace_event::TraceLog::GetInstance()->Flush(
        base::Bind(&Provider::SendChunk, base::Unretained(this)));
  }
}

void Provider::ForceEnableTracing() {
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig("*", base::trace_event::RECORD_UNTIL_FULL),
      base::trace_event::TraceLog::RECORDING_MODE);
  tracing_forced_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&Provider::DelayedStop, weak_factory_.GetWeakPtr()));
}

void Provider::DelayedStop() {
  // We use this indirection to account for cases where the Initialize method
  // takes more than one second to finish; thus we start the countdown only when
  // the current thread is unblocked.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Provider::StopIfForced, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void Provider::StopIfForced() {
  if (!tracing_forced_) {
    return;
  }
  base::trace_event::TraceLog::GetInstance()->SetDisabled();
  base::trace_event::TraceLog::GetInstance()->Flush(
      base::Callback<void(const scoped_refptr<base::RefCountedString>&,
                          bool)>());
}

void Provider::SendChunk(
    const scoped_refptr<base::RefCountedString>& events_str,
    bool has_more_events) {
  DCHECK(recorder_);
  // The string will be empty if an error eccured or there were no trace
  // events. Empty string is not a valid chunk to record so skip in this case.
  if (!events_str->data().empty()) {
    recorder_->Record(mojo::String(events_str->data()));
  }
  if (!has_more_events) {
    recorder_.reset();
  }
}

}  // namespace tracing
