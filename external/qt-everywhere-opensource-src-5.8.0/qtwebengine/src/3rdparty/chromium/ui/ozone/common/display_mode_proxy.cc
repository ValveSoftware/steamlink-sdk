// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"

#include "ui/ozone/common/display_mode_proxy.h"

#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

DisplayModeProxy::DisplayModeProxy(const DisplayMode_Params& params)
    : DisplayMode(params.size, params.is_interlaced, params.refresh_rate) {
}

DisplayModeProxy::DisplayModeProxy(const gfx::Size& size,
                                   bool interlaced,
                                   float refresh_rate)
    : DisplayMode(size, interlaced, refresh_rate) {}

DisplayModeProxy::~DisplayModeProxy() {
}

std::unique_ptr<DisplayMode> DisplayModeProxy::Clone() const {
  return base::WrapUnique(
      new DisplayModeProxy(size(), is_interlaced(), refresh_rate()));
}

}  // namespace ui
