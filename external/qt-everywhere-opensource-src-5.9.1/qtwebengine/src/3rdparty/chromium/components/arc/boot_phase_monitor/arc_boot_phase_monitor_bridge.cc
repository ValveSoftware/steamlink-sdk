// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"

#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

namespace {

void PrioritizeArcInstanceCallback(bool success) {
  VLOG(2) << "Finished prioritizing the instance: result=" << success;
  if (!success)
    LOG(WARNING) << "Failed to prioritize ARC";
}

}  // namespace

ArcBootPhaseMonitorBridge::ArcBootPhaseMonitorBridge(
    ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->boot_phase_monitor()->AddObserver(this);
}

ArcBootPhaseMonitorBridge::~ArcBootPhaseMonitorBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
  arc_bridge_service()->boot_phase_monitor()->RemoveObserver(this);
}

void ArcBootPhaseMonitorBridge::OnInstanceReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto* instance =
      arc_bridge_service()->boot_phase_monitor()->GetInstanceForMethod("Init");
  DCHECK(instance);
  instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcBootPhaseMonitorBridge::OnInstanceClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ArcBootPhaseMonitorBridge::OnBootCompleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(2) << "OnBootCompleted";
  chromeos::SessionManagerClient* session_manager_client =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager_client->PrioritizeArcInstance(
      base::Bind(PrioritizeArcInstanceCallback));
  session_manager_client->EmitArcBooted();
}

}  // namespace arc
