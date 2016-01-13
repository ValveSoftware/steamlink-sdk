// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pepper_renderer_instance_data.h"

namespace content {

PepperRendererInstanceData::PepperRendererInstanceData()
    : render_process_id(0),
      render_frame_id(0) {
}

PepperRendererInstanceData::PepperRendererInstanceData(
    int render_process,
    int render_frame,
    const GURL& document,
    const GURL& plugin)
    : render_process_id(render_process),
      render_frame_id(render_frame),
      document_url(document),
      plugin_url(plugin) {
}

PepperRendererInstanceData::~PepperRendererInstanceData() {
}

}  // namespace content
