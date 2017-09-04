// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
#define COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/crash_collector.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace arc {

class ArcBridgeService;

// Relays dumps for non-native ARC crashes to the crash reporter in Chrome OS.
class ArcCrashCollectorBridge
    : public ArcService,
      public InstanceHolder<mojom::CrashCollectorInstance>::Observer,
      public mojom::CrashCollectorHost {
 public:
  ArcCrashCollectorBridge(ArcBridgeService* bridge,
                          scoped_refptr<base::TaskRunner> blocking_task_runner);
  ~ArcCrashCollectorBridge() override;

  // InstanceHolder<mojom::CrashCollectorInstance>::Observer overrides.
  void OnInstanceReady() override;

  // mojom::CrashCollectorHost overrides.
  void DumpCrash(const std::string& type, mojo::ScopedHandle pipe) override;

  void SetBuildProperties(const std::string& device,
                          const std::string& board,
                          const std::string& cpu_abi) override;

 private:
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  mojo::Binding<mojom::CrashCollectorHost> binding_;

  std::string device_;
  std::string board_;
  std::string cpu_abi_;

  DISALLOW_COPY_AND_ASSIGN(ArcCrashCollectorBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_CRASH_COLLECTOR_ARC_CRASH_COLLECTOR_BRIDGE_H_
