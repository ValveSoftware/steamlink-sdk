// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/kiosk/arc_kiosk_bridge.h"

#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"

namespace arc {

ArcKioskBridge::ArcKioskBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->kiosk()->AddObserver(this);
}

ArcKioskBridge::~ArcKioskBridge() {
  arc_bridge_service()->kiosk()->RemoveObserver(this);
}

void ArcKioskBridge::OnInstanceReady() {
  mojom::KioskInstance* kiosk_instance =
      arc_bridge_service()->kiosk()->GetInstanceForMethod("Init");
  DCHECK(kiosk_instance);
  kiosk_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcKioskBridge::OnMaintenanceSessionCreated(int32_t session_id) {
  // TODO(poromov@) Show appropriate splash screen.
}

void ArcKioskBridge::OnMaintenanceSessionFinished(int32_t session_id,
                                                  bool success) {
  // TODO(poromov@) Start kiosk app.
}

}  // namespace arc
