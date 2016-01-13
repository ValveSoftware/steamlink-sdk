// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PEPPER_RENDERER_INSTANCE_DATA_H_
#define CONTENT_COMMON_PEPPER_RENDERER_INSTANCE_DATA_H_

#include "url/gurl.h"

namespace content {

// This struct contains data which is associated with a particular plugin
// instance and is related to the renderer in which the plugin instance lives.
// This data is transferred to the browser process from the renderer when the
// instance is created and is stored in the BrowserPpapiHost.
struct PepperRendererInstanceData {
  PepperRendererInstanceData();
  PepperRendererInstanceData(int render_process,
                             int render_frame_id,
                             const GURL& document,
                             const GURL& plugin);
  ~PepperRendererInstanceData();
  int render_process_id;
  int render_frame_id;
  GURL document_url;
  GURL plugin_url;
};

}  // namespace content

#endif  // CONTENT_COMMON_PEPPER_RENDERER_INSTANCE_DATA_H_
