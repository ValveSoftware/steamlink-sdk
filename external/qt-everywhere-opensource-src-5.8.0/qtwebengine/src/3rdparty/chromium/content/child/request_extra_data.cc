// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/request_extra_data.h"

#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message.h"

using blink::WebString;

namespace content {

RequestExtraData::RequestExtraData()
    : visibility_state_(blink::WebPageVisibilityStateVisible),
      render_frame_id_(MSG_ROUTING_NONE),
      is_main_frame_(false),
      parent_is_main_frame_(false),
      parent_render_frame_id_(-1),
      allow_download_(true),
      transition_type_(ui::PAGE_TRANSITION_LINK),
      should_replace_current_entry_(false),
      transferred_request_child_id_(-1),
      transferred_request_request_id_(-1),
      service_worker_provider_id_(kInvalidServiceWorkerProviderId),
      originated_from_service_worker_(false),
      initiated_in_secure_context_(false) {}

RequestExtraData::~RequestExtraData() {
}

}  // namespace content
