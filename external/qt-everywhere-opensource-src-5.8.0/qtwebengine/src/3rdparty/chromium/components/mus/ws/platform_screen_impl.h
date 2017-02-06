// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_PLATFORM_SCREEN_IMPL_H_
#define COMPONENTS_MUS_WS_PLATFORM_SCREEN_IMPL_H_

#include <stdint.h>

#include "base/callback.h"
#include "components/mus/ws/platform_screen.h"

namespace gfx {
class Rect;
}

namespace mus {
namespace ws {

// PlatformScreenImpl provides the necessary functionality to configure all
// attached physical displays on non-ozone platforms.
class PlatformScreenImpl : public PlatformScreen {
 public:
  PlatformScreenImpl();
  ~PlatformScreenImpl() override;

 private:
  // PlatformScreen.
  void Init() override;
  void ConfigurePhysicalDisplay(
      const PlatformScreen::ConfiguredDisplayCallback& callback) override;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenImpl);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_PLATFORM_SCREEN_IMPL_H_
