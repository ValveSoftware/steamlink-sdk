// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/obb_mounter/arc_obb_mounter_bridge.h"

#include "base/bind.h"
#include "chromeos/dbus/arc_obb_mounter_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/arc_bridge_service.h"

namespace arc {

namespace {

// Used to convert mojo Callback to VoidDBusMethodCallback.
void RunObbCallback(const base::Callback<void(bool)>& callback,
                    chromeos::DBusMethodCallStatus result) {
  callback.Run(result == chromeos::DBUS_METHOD_CALL_SUCCESS);
}

}  // namespace

ArcObbMounterBridge::ArcObbMounterBridge(ArcBridgeService* bridge_service)
    : ArcService(bridge_service), binding_(this) {
  arc_bridge_service()->obb_mounter()->AddObserver(this);
}

ArcObbMounterBridge::~ArcObbMounterBridge() {
  arc_bridge_service()->obb_mounter()->RemoveObserver(this);
}

void ArcObbMounterBridge::OnInstanceReady() {
  mojom::ObbMounterInstance* obb_mounter_instance =
      arc_bridge_service()->obb_mounter()->instance();
  obb_mounter_instance->Init(binding_.CreateInterfacePtrAndBind());
}

void ArcObbMounterBridge::MountObb(const mojo::String& obb_file,
                                   const mojo::String& target_path,
                                   int32_t owner_gid,
                                   const MountObbCallback& callback) {
  chromeos::DBusThreadManager::Get()->GetArcObbMounterClient()->MountObb(
      obb_file.get(), target_path.get(), owner_gid,
      base::Bind(&RunObbCallback, callback));
}

void ArcObbMounterBridge::UnmountObb(const mojo::String& target_path,
                                     const UnmountObbCallback& callback) {
  chromeos::DBusThreadManager::Get()->GetArcObbMounterClient()->UnmountObb(
      target_path.get(), base::Bind(&RunObbCallback, callback));
}

}  // namespace arc
