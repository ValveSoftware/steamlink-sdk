// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_DELEGATED_FRAME_DATA_H_
#define CC_OUTPUT_DELEGATED_FRAME_DATA_H_

#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/transferable_resource.h"
#include "ui/gfx/size.h"

namespace cc {

class CC_EXPORT DelegatedFrameData {
 public:
  DelegatedFrameData();
  ~DelegatedFrameData();

  // The device scale factor used when generating this frame.
  float device_scale_factor;

  TransferableResourceArray resource_list;
  ScopedPtrVector<RenderPass> render_pass_list;

 private:
  DISALLOW_COPY_AND_ASSIGN(DelegatedFrameData);
};

}  // namespace cc

#endif  // CC_OUTPUT_DELEGATED_FRAME_DATA_H_
