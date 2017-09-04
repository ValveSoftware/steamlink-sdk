// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_STUB_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_STUB_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/display/viewport_metrics.h"

namespace display {

// PlatformScreenStub provides the necessary functionality to configure a fixed
// 1024x768 display for non-ozone platforms.
class PlatformScreenStub : public PlatformScreen {
 public:
  PlatformScreenStub();
  ~PlatformScreenStub() override;

 private:
  // Fake creation of a single 1024x768 display.
  void FixedSizeScreenConfiguration();

  // PlatformScreen.
  void AddInterfaces(service_manager::InterfaceRegistry* registry) override;
  void Init(PlatformScreenDelegate* delegate) override;
  void RequestCloseDisplay(int64_t display_id) override;
  int64_t GetPrimaryDisplayId() const override;

  // Sample display information.
  int64_t display_id_ = 1;
  ViewportMetrics display_metrics_;

  PlatformScreenDelegate* delegate_ = nullptr;

  base::WeakPtrFactory<PlatformScreenStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenStub);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_STUB_H_
