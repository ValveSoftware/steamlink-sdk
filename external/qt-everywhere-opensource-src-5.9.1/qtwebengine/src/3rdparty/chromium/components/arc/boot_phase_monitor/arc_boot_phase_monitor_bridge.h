// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
#define COMPONENTS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/boot_phase_monitor.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// Receives boot phase notifications from ARC.
class ArcBootPhaseMonitorBridge
    : public ArcService,
      public InstanceHolder<mojom::BootPhaseMonitorInstance>::Observer,
      public mojom::BootPhaseMonitorHost {
 public:
  explicit ArcBootPhaseMonitorBridge(ArcBridgeService* bridge_service);
  ~ArcBootPhaseMonitorBridge() override;

  // InstanceHolder<mojom::BootPhaseMonitorInstance>::Observer
  void OnInstanceReady() override;
  void OnInstanceClosed() override;

  // mojom::BootPhaseMonitorHost
  void OnBootCompleted() override;

 private:
  mojo::Binding<mojom::BootPhaseMonitorHost> binding_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(ArcBootPhaseMonitorBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_BOOT_PHASE_MONITOR_ARC_BOOT_PHASE_MONITOR_BRIDGE_H_
