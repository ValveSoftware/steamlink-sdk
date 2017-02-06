// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/native_display_delegate_ozone_x11.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/common/display_util.h"

namespace ui {

namespace {

void GetDefaultDisplaySnapshotParams(DisplaySnapshot_Params* params) {
  DisplayMode_Params mode_param;
  mode_param.size = gfx::Size(1024, 768);
  mode_param.refresh_rate = 60;

  params->display_id = 1;
  params->modes.push_back(mode_param);
  params->type = DISPLAY_CONNECTION_TYPE_INTERNAL;
  params->physical_size = gfx::Size(0, 0);
  params->has_current_mode = true;
  params->current_mode = mode_param;
  params->has_native_mode = true;
  params->native_mode = mode_param;
  params->product_id = 1;
}

}  // namespace

NativeDisplayDelegateOzoneX11::NativeDisplayDelegateOzoneX11() {}

NativeDisplayDelegateOzoneX11::~NativeDisplayDelegateOzoneX11() {}

void NativeDisplayDelegateOzoneX11::OnConfigurationChanged() {
  // TODO(kylechar): Listen for X11 window resizes to trigger this.
  FOR_EACH_OBSERVER(NativeDisplayObserver, observers_,
                    OnConfigurationChanged());
}

void NativeDisplayDelegateOzoneX11::Initialize() {
  // Create display snapshot from command line or use default values.
  DisplaySnapshot_Params params;
  if (!CreateSnapshotFromCommandLine(&params))
    GetDefaultDisplaySnapshotParams(&params);

  DCHECK_NE(DISPLAY_CONNECTION_TYPE_NONE, params.type);
  displays().push_back(base::WrapUnique(new DisplaySnapshotProxy(params)));
}

void NativeDisplayDelegateOzoneX11::AddObserver(
    NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void NativeDisplayDelegateOzoneX11::RemoveObserver(
    NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
