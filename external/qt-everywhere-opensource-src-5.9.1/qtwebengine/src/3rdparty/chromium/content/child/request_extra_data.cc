// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/request_extra_data.h"

#include "content/common/resource_request.h"
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
      initiated_in_secure_context_(false),
      is_prefetch_(false),
      download_to_network_cache_only_(false) {}

RequestExtraData::~RequestExtraData() {
}

void RequestExtraData::CopyToResourceRequest(ResourceRequest* request) const {
  request->visibility_state = visibility_state_;
  request->render_frame_id = render_frame_id_;
  request->is_main_frame = is_main_frame_;

  request->parent_is_main_frame = parent_is_main_frame_;
  request->parent_render_frame_id = parent_render_frame_id_;
  request->allow_download = allow_download_;
  request->transition_type = transition_type_;
  request->should_replace_current_entry = should_replace_current_entry_;
  request->transferred_request_child_id = transferred_request_child_id_;
  request->transferred_request_request_id = transferred_request_request_id_;
  request->service_worker_provider_id = service_worker_provider_id_;
  request->originated_from_service_worker = originated_from_service_worker_;

  request->initiated_in_secure_context = initiated_in_secure_context_;
  request->download_to_network_cache_only = download_to_network_cache_only_;
}

}  // namespace content
