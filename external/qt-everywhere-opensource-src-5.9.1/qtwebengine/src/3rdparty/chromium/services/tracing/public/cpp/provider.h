// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PROVIDER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PROVIDER_H_

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/tracing/public/interfaces/tracing.mojom.h"

namespace service_manager {
class Connector;
}

namespace tracing {

class Provider : public mojom::Provider {
 public:
  Provider();
  ~Provider() override;

  void InitializeWithFactory(mojom::FactoryPtr* factory);
  void Initialize(service_manager::Connector* connector,
                  const std::string& url);

  void Bind(mojom::ProviderRequest request);

 private:
  void InitializeWithFactoryInternal(mojom::FactoryPtr* factory);

  // mojom::Provider implementation:
  void StartTracing(const std::string& categories,
                    mojom::RecorderPtr recorder) override;
  void StopTracing() override;

  void SendChunk(const scoped_refptr<base::RefCountedString>& events_str,
                 bool has_more_events);

  void DelayedStop();
  // Stop the collection of traces if no external connection asked for them yet.
  void StopIfForced();

  // Enable tracing without waiting for an inbound connection. It will stop if
  // no TraceRecorder is sent within a set time.
  void ForceEnableTracing();

  mojo::Binding<mojom::Provider> binding_;
  bool tracing_forced_;
  mojom::RecorderPtr recorder_;

  base::WeakPtrFactory<Provider> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PROVIDER_H_
