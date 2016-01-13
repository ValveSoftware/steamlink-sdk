// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_
#define UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "ui/gfx/vsync_provider.h"

namespace ui {

class HardwareDisplayController;

class DriVSyncProvider : public gfx::VSyncProvider {
 public:
  DriVSyncProvider(const base::WeakPtr<HardwareDisplayController>& controller);
  virtual ~DriVSyncProvider();

  virtual void GetVSyncParameters(const UpdateVSyncCallback& callback) OVERRIDE;

 private:
  base::WeakPtr<HardwareDisplayController> controller_;

  DISALLOW_COPY_AND_ASSIGN(DriVSyncProvider);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_IMPL_DRI_VSYNC_PROVIDER_H_
