// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_
#define COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "components/arc/arc_service.h"
#include "components/arc/common/obb_mounter.mojom.h"
#include "components/arc/instance_holder.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace arc {

class ArcBridgeService;

// This class handles OBB mount/unmount requests from Android.
class ArcObbMounterBridge
    : public ArcService,
      public InstanceHolder<mojom::ObbMounterInstance>::Observer,
      public mojom::ObbMounterHost {
 public:
  explicit ArcObbMounterBridge(ArcBridgeService* bridge_service);
  ~ArcObbMounterBridge() override;

  // InstanceHolder<mojom::ObbMounterInstance>::Observer overrides:
  void OnInstanceReady() override;

  // mojom::ObbMounterHost overrides:
  void MountObb(const std::string& obb_file,
                const std::string& target_path,
                int32_t owner_gid,
                const MountObbCallback& callback) override;
  void UnmountObb(const std::string& target_path,
                  const UnmountObbCallback& callback) override;

 private:
  mojo::Binding<mojom::ObbMounterHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcObbMounterBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_OBB_MOUNTER_ARC_OBB_MOUNTER_BRIDGE_H_
