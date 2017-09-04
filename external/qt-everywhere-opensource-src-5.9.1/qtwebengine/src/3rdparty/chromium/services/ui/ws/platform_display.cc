// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/platform_display.h"

#include "base/memory/ptr_util.h"
#include "services/ui/ws/platform_display_default.h"
#include "services/ui/ws/platform_display_factory.h"
#include "services/ui/ws/platform_display_init_params.h"

namespace ui {
namespace ws {

// static
PlatformDisplayFactory* PlatformDisplay::factory_ = nullptr;

// static
std::unique_ptr<PlatformDisplay> PlatformDisplay::Create(
    const PlatformDisplayInitParams& init_params) {
  if (factory_)
    return factory_->CreatePlatformDisplay();

  return base::MakeUnique<PlatformDisplayDefault>(init_params);
}

}  // namespace ws
}  // namespace ui
