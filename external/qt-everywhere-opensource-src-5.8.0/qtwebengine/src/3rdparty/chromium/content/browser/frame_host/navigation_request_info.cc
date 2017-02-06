// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_request_info.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

NavigationRequestInfo::NavigationRequestInfo(
    const CommonNavigationParams& common_params,
    const BeginNavigationParams& begin_params,
    const GURL& first_party_for_cookies,
    const url::Origin& request_initiator,
    bool is_main_frame,
    bool parent_is_main_frame,
    int frame_tree_node_id)
    : common_params(common_params),
      begin_params(begin_params),
      first_party_for_cookies(first_party_for_cookies),
      request_initiator(request_initiator),
      is_main_frame(is_main_frame),
      parent_is_main_frame(parent_is_main_frame),
      frame_tree_node_id(frame_tree_node_id) {}

NavigationRequestInfo::~NavigationRequestInfo() {}

}  // namespace content
