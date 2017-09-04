// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/service.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace tracing {

Service::Service() : collector_binding_(this), tracing_active_(false) {}
Service::~Service() {}

bool Service::OnConnect(const service_manager::ServiceInfo& remote_info,
                        service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::Factory>(this);
  registry->AddInterface<mojom::Collector>(this);
  registry->AddInterface<mojom::StartupPerformanceDataCollector>(this);
  return true;
}

bool Service::OnStop() {
  // TODO(beng): This is only required because Service isn't run by
  // ServiceRunner - instead it's launched automatically by the standalone
  // service manager. It shouldn't be.
  base::MessageLoop::current()->QuitWhenIdle();
  return false;
}

void Service::Create(const service_manager::Identity& remote_identity,
                     mojom::FactoryRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Service::Create(const service_manager::Identity& remote_identity,
                     mojom::CollectorRequest request) {
  collector_binding_.Bind(std::move(request));
}

void Service::Create(const service_manager::Identity& remote_identity,
                     mojom::StartupPerformanceDataCollectorRequest request) {
  startup_performance_data_collector_bindings_.AddBinding(this,
                                                          std::move(request));
}

void Service::CreateRecorder(mojom::ProviderPtr provider) {
  if (tracing_active_) {
    mojom::RecorderPtr recorder_ptr;
    recorder_impls_.push_back(
        new Recorder(GetProxy(&recorder_ptr), sink_.get()));
    provider->StartTracing(tracing_categories_, std::move(recorder_ptr));
  }
  provider_ptrs_.AddPtr(std::move(provider));
}

void Service::Start(mojo::ScopedDataPipeProducerHandle stream,
                    const std::string& categories) {
  tracing_categories_ = categories;
  sink_.reset(new DataSink(std::move(stream)));
  provider_ptrs_.ForAllPtrs(
    [categories, this](mojom::Provider* controller) {
      mojom::RecorderPtr ptr;
      recorder_impls_.push_back(
          new Recorder(GetProxy(&ptr), sink_.get()));
      controller->StartTracing(categories, std::move(ptr));
    });
  tracing_active_ = true;
}

void Service::StopAndFlush() {
  // Remove any collectors that closed their message pipes before we called
  // StopTracing().
  for (int i = static_cast<int>(recorder_impls_.size()) - 1; i >= 0; --i) {
    if (!recorder_impls_[i]->RecorderHandle().is_valid()) {
      recorder_impls_.erase(recorder_impls_.begin() + i);
    }
  }

  tracing_active_ = false;
  provider_ptrs_.ForAllPtrs(
      [](mojom::Provider* controller) { controller->StopTracing(); });

  // Sending the StopTracing message to registered controllers will request that
  // they send trace data back via the collector interface and, when they are
  // done, close the collector pipe. We don't know how long they will take. We
  // want to read all data that any collector might send until all collectors or
  // closed or an (arbitrary) deadline has passed. Since the bindings don't
  // support this directly we do our own MojoWaitMany over the handles and read
  // individual messages until all are closed or our absolute deadline has
  // elapsed.
  static const MojoDeadline kTimeToWaitMicros = 1000 * 1000;
  MojoTimeTicks end = MojoGetTimeTicksNow() + kTimeToWaitMicros;

  while (!recorder_impls_.empty()) {
    MojoTimeTicks now = MojoGetTimeTicksNow();
    if (now >= end)  // Timed out?
      break;

    MojoDeadline mojo_deadline = end - now;
    std::vector<mojo::Handle> handles;
    std::vector<MojoHandleSignals> signals;
    for (auto* it : recorder_impls_) {
      handles.push_back(it->RecorderHandle());
      signals.push_back(MOJO_HANDLE_SIGNAL_READABLE |
                        MOJO_HANDLE_SIGNAL_PEER_CLOSED);
    }
    std::vector<MojoHandleSignalsState> signals_states(signals.size());
    const mojo::WaitManyResult wait_many_result =
        mojo::WaitMany(handles, signals, mojo_deadline, &signals_states);
    if (wait_many_result.result == MOJO_RESULT_DEADLINE_EXCEEDED) {
      // Timed out waiting, nothing more to read.
      LOG(WARNING) << "Timed out waiting for trace flush";
      break;
    }
    if (wait_many_result.IsIndexValid()) {
      // Iterate backwards so we can remove closed pipes from |recorder_impls_|
      // without invalidating subsequent offsets.
      for (size_t i = signals_states.size(); i != 0; --i) {
        size_t index = i - 1;
        MojoHandleSignals satisfied = signals_states[index].satisfied_signals;
        // To avoid dropping data, don't close unless there's no
        // readable signal.
        if (satisfied & MOJO_HANDLE_SIGNAL_READABLE)
          recorder_impls_[index]->TryRead();
        else if (satisfied & MOJO_HANDLE_SIGNAL_PEER_CLOSED)
          recorder_impls_.erase(recorder_impls_.begin() + index);
      }
    }
  }
  AllDataCollected();
}

void Service::SetServiceManagerProcessCreationTime(int64_t time) {
  if (startup_performance_times_.service_manager_process_creation_time == 0)
    startup_performance_times_.service_manager_process_creation_time = time;
}

void Service::SetServiceManagerMainEntryPointTime(int64_t time) {
  if (startup_performance_times_.service_manager_main_entry_point_time == 0)
    startup_performance_times_.service_manager_main_entry_point_time = time;
}

void Service::SetBrowserMessageLoopStartTicks(int64_t ticks) {
  if (startup_performance_times_.browser_message_loop_start_ticks == 0)
    startup_performance_times_.browser_message_loop_start_ticks = ticks;
}

void Service::SetBrowserWindowDisplayTicks(int64_t ticks) {
  if (startup_performance_times_.browser_window_display_ticks == 0)
    startup_performance_times_.browser_window_display_ticks = ticks;
}

void Service::SetBrowserOpenTabsTimeDelta(int64_t delta) {
  if (startup_performance_times_.browser_open_tabs_time_delta == 0)
    startup_performance_times_.browser_open_tabs_time_delta = delta;
}

void Service::SetFirstWebContentsMainFrameLoadTicks(int64_t ticks) {
  if (startup_performance_times_.first_web_contents_main_frame_load_ticks == 0)
    startup_performance_times_.first_web_contents_main_frame_load_ticks = ticks;
}

void Service::SetFirstVisuallyNonEmptyLayoutTicks(int64_t ticks) {
  if (startup_performance_times_.first_visually_non_empty_layout_ticks == 0)
    startup_performance_times_.first_visually_non_empty_layout_ticks = ticks;
}

void Service::GetStartupPerformanceTimes(
    const GetStartupPerformanceTimesCallback& callback) {
  callback.Run(startup_performance_times_.Clone());
}

void Service::AllDataCollected() {
  recorder_impls_.clear();
  sink_.reset();
}

}  // namespace tracing
