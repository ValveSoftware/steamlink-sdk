// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_TRACING_APP_H_
#define SERVICES_TRACING_TRACING_APP_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shell/public/cpp/interface_factory.h"
#include "services/shell/public/cpp/shell_client.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"
#include "services/tracing/trace_data_sink.h"
#include "services/tracing/trace_recorder_impl.h"

namespace tracing {

class TracingApp
    : public shell::ShellClient,
      public shell::InterfaceFactory<TraceCollector>,
      public TraceCollector,
      public shell::InterfaceFactory<StartupPerformanceDataCollector>,
      public StartupPerformanceDataCollector {
 public:
  TracingApp();
  ~TracingApp() override;

 private:
  // shell::ShellClient implementation.
  bool AcceptConnection(shell::Connection* connection) override;
  bool ShellConnectionLost() override;

  // shell::InterfaceFactory<TraceCollector> implementation.
  void Create(shell::Connection* connection,
              mojo::InterfaceRequest<TraceCollector> request) override;

  // shell::InterfaceFactory<StartupPerformanceDataCollector> implementation.
  void Create(
      shell::Connection* connection,
      mojo::InterfaceRequest<StartupPerformanceDataCollector> request) override;

  // tracing::TraceCollector implementation.
  void Start(mojo::ScopedDataPipeProducerHandle stream,
             const mojo::String& categories) override;
  void StopAndFlush() override;

  // StartupPerformanceDataCollector implementation.
  void SetShellProcessCreationTime(int64_t time) override;
  void SetShellMainEntryPointTime(int64_t time) override;
  void SetBrowserMessageLoopStartTicks(int64_t ticks) override;
  void SetBrowserWindowDisplayTicks(int64_t ticks) override;
  void SetBrowserOpenTabsTimeDelta(int64_t delta) override;
  void SetFirstWebContentsMainFrameLoadTicks(int64_t ticks) override;
  void SetFirstVisuallyNonEmptyLayoutTicks(int64_t ticks) override;
  void GetStartupPerformanceTimes(
      const GetStartupPerformanceTimesCallback& callback) override;

  void AllDataCollected();

  std::unique_ptr<TraceDataSink> sink_;
  ScopedVector<TraceRecorderImpl> recorder_impls_;
  mojo::InterfacePtrSet<TraceProvider> provider_ptrs_;
  mojo::Binding<TraceCollector> collector_binding_;
  mojo::BindingSet<StartupPerformanceDataCollector>
      startup_performance_data_collector_bindings_;
  StartupPerformanceTimes startup_performance_times_;
  bool tracing_active_;
  mojo::String tracing_categories_;

  DISALLOW_COPY_AND_ASSIGN(TracingApp);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_TRACING_APP_H_
