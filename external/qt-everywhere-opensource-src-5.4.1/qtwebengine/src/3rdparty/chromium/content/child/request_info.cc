// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/request_info.h"

namespace content {

RequestInfo::RequestInfo()
    : referrer_policy(blink::WebReferrerPolicyDefault),
      load_flags(0),
      requestor_pid(0),
      request_type(ResourceType::MAIN_FRAME),
      priority(net::LOW),
      request_context(0),
      appcache_host_id(0),
      routing_id(0),
      download_to_file(false),
      has_user_gesture(false),
      extra_data(NULL) {
}

RequestInfo::~RequestInfo() {}

}  // namespace content
