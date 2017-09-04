// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_SERVICE_H_
#define SERVICES_TRACING_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/tracing/data_sink.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"
#include "services/tracing/recorder.h"

namespace tracing {

class Service : public service_manager::Service,
                public service_manager::InterfaceFactory<mojom::Factory>,
                public service_manager::InterfaceFactory<mojom::Collector>,
                public service_manager::InterfaceFactory<
                    mojom::StartupPerformanceDataCollector>,
                public mojom::Factory,
                public mojom::Collector,
                public mojom::StartupPerformanceDataCollector {
 public:
  Service();
  ~Service() override;

 private:
  // service_manager::Service implementation.
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;
  bool OnStop() override;

  // service_manager::InterfaceFactory<mojom::Factory>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::FactoryRequest request) override;

  // service_manager::InterfaceFactory<mojom::Collector>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::CollectorRequest request) override;

  // service_manager::InterfaceFactory<mojom::StartupPerformanceDataCollector>:
  void Create(const service_manager::Identity& remote_identity,
              mojom::StartupPerformanceDataCollectorRequest request) override;

  // mojom::Factory:
  void CreateRecorder(mojom::ProviderPtr provider) override;

  // mojom::Collector:
  void Start(mojo::ScopedDataPipeProducerHandle stream,
             const std::string& categories) override;
  void StopAndFlush() override;

  // mojom::StartupPerformanceDataCollector:
  void SetServiceManagerProcessCreationTime(int64_t time) override;
  void SetServiceManagerMainEntryPointTime(int64_t time) override;
  void SetBrowserMessageLoopStartTicks(int64_t ticks) override;
  void SetBrowserWindowDisplayTicks(int64_t ticks) override;
  void SetBrowserOpenTabsTimeDelta(int64_t delta) override;
  void SetFirstWebContentsMainFrameLoadTicks(int64_t ticks) override;
  void SetFirstVisuallyNonEmptyLayoutTicks(int64_t ticks) override;
  void GetStartupPerformanceTimes(
      const GetStartupPerformanceTimesCallback& callback) override;

  void AllDataCollected();

  mojo::BindingSet<mojom::Factory> bindings_;
  std::unique_ptr<DataSink> sink_;
  ScopedVector<Recorder> recorder_impls_;
  mojo::InterfacePtrSet<mojom::Provider> provider_ptrs_;
  mojo::Binding<mojom::Collector> collector_binding_;
  mojo::BindingSet<mojom::StartupPerformanceDataCollector>
      startup_performance_data_collector_bindings_;
  mojom::StartupPerformanceTimes startup_performance_times_;
  bool tracing_active_;
  mojo::String tracing_categories_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_SERVICE_H_
