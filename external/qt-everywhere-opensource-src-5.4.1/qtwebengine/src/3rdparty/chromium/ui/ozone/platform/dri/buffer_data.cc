// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/buffer_data.h"

#include <gbm.h>

#include "base/logging.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

// Pixel configuration for the current buffer format.
// TODO(dnicoara) These will need to change once we query the hardware for
// supported configurations.
const uint8_t kColorDepth = 24;
const uint8_t kPixelDepth = 32;

BufferData::BufferData(DriWrapper* dri, gbm_bo* buffer)
    : dri_(dri),
      handle_(gbm_bo_get_handle(buffer).u32),
      framebuffer_(0) {
  // Register the buffer with the controller. This will allow us to scan out the
  // buffer once we're done drawing into it. If we can't register the buffer
  // then there's no point in having BufferData associated with it.
  if (!dri_->AddFramebuffer(gbm_bo_get_width(buffer),
                            gbm_bo_get_height(buffer),
                            kColorDepth,
                            kPixelDepth,
                            gbm_bo_get_stride(buffer),
                            handle_,
                            &framebuffer_)) {
    LOG(ERROR) << "Failed to register buffer";
  }
}

BufferData::~BufferData() {
  if (framebuffer_)
    dri_->RemoveFramebuffer(framebuffer_);
}

// static
BufferData* BufferData::CreateData(DriWrapper* dri,
                                   gbm_bo* buffer) {
  BufferData* data = new BufferData(dri, buffer);
  if (!data->framebuffer()) {
    delete data;
    return NULL;
  }

  // GBM can destroy the buffers at any time as long as they aren't locked. This
  // sets a callback such that we can clean up all our state when GBM destroys
  // the buffer.
  gbm_bo_set_user_data(buffer, data, BufferData::Destroy);

  return data;
}

// static
void BufferData::Destroy(gbm_bo* buffer, void* data) {
  BufferData* bd = static_cast<BufferData*>(data);
  delete bd;
}

// static
BufferData* BufferData::GetData(gbm_bo* buffer) {
  return static_cast<BufferData*>(gbm_bo_get_user_data(buffer));
}

}  // namespace ui
