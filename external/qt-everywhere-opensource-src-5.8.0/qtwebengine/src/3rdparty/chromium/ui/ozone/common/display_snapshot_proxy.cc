// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"

#include "ui/ozone/common/display_snapshot_proxy.h"

#include <stddef.h>

#include "ui/ozone/common/display_mode_proxy.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

namespace ui {

namespace {

bool SameModes(const DisplayMode_Params& lhs, const DisplayMode_Params& rhs) {
  return lhs.size == rhs.size && lhs.is_interlaced == rhs.is_interlaced &&
         lhs.refresh_rate == rhs.refresh_rate;
}

}  // namespace

DisplaySnapshotProxy::DisplaySnapshotProxy(const DisplaySnapshot_Params& params)
    : DisplaySnapshot(params.display_id,
                      params.origin,
                      params.physical_size,
                      params.type,
                      params.is_aspect_preserving_scaling,
                      params.has_overscan,
                      params.has_color_correction_matrix,
                      params.display_name,
                      params.sys_path,
                      std::vector<std::unique_ptr<const DisplayMode>>(),
                      params.edid,
                      NULL,
                      NULL),
      string_representation_(params.string_representation) {
  for (size_t i = 0; i < params.modes.size(); ++i) {
    modes_.push_back(base::WrapUnique(new DisplayModeProxy(params.modes[i])));

    if (params.has_current_mode &&
        SameModes(params.modes[i], params.current_mode))
      current_mode_ = modes_.back().get();

    if (params.has_native_mode &&
        SameModes(params.modes[i], params.native_mode))
      native_mode_ = modes_.back().get();
  }

  product_id_ = params.product_id;
  maximum_cursor_size_ = params.maximum_cursor_size;
}

DisplaySnapshotProxy::~DisplaySnapshotProxy() {
}

std::string DisplaySnapshotProxy::ToString() const {
  return string_representation_;
}

}  // namespace ui
