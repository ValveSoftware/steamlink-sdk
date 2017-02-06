// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_PLATFORM_SCREEN_IMPL_OZONE_H_
#define COMPONENTS_MUS_WS_PLATFORM_SCREEN_IMPL_OZONE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/mus/ws/platform_screen.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Rect;
}

namespace ui {
class NativeDisplayDelegate;
class DisplaySnapshot;
}

namespace mus {
namespace ws {

// PlatformScreenImplOzone provides the necessary functionality to configure all
// attached physical displays on the ozone platform.
class PlatformScreenImplOzone : public PlatformScreen,
                                public ui::NativeDisplayObserver {
 public:
  PlatformScreenImplOzone();
  ~PlatformScreenImplOzone() override;

 private:
  // PlatformScreen
  void Init() override;  // Must not be called until after the ozone platform is
                         // initialized.
  void ConfigurePhysicalDisplay(
      const ConfiguredDisplayCallback& callback) override;

  // TODO(rjkroege): NativeDisplayObserver is misnamed as it tracks changes in
  // the physical "Screen".  Consider renaming it to NativeScreenObserver.
  // ui::NativeDisplayObserver:
  void OnConfigurationChanged() override;

  // Display management callback.
  void OnDisplaysAquired(const ConfiguredDisplayCallback& callback,
                         const std::vector<ui::DisplaySnapshot*>& displays);

  // The display subsystem calls |OnDisplayConfigured| for each display that has
  // been successfully configured. This in turn calls |callback_| with the
  // identity and bounds of each physical display.
  void OnDisplayConfigured(const ConfiguredDisplayCallback& callback,
                           int64_t id,
                           const gfx::Rect& bounds,
                           bool success);

  std::unique_ptr<ui::NativeDisplayDelegate> native_display_delegate_;

  base::WeakPtrFactory<PlatformScreenImplOzone> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformScreenImplOzone);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_PLATFORM_SCREEN_IMPL_OZONE_H_
