// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_client_info.h"

#include "base/logging.h"
#include "content/common/service_worker/service_worker_types.h"

namespace content {

ServiceWorkerClientInfo::ServiceWorkerClientInfo()
    : ServiceWorkerClientInfo(std::string(),
                              blink::WebPageVisibilityStateLast,
                              false,
                              GURL(),
                              REQUEST_CONTEXT_FRAME_TYPE_LAST,
                              base::TimeTicks(),
                              blink::WebServiceWorkerClientTypeLast) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const std::string& client_uuid,
    blink::WebPageVisibilityState page_visibility_state,
    bool is_focused,
    const GURL& url,
    RequestContextFrameType frame_type,
    base::TimeTicks last_focus_time,
    blink::WebServiceWorkerClientType client_type)
    : client_uuid(client_uuid),
      page_visibility_state(page_visibility_state),
      is_focused(is_focused),
      url(url),
      frame_type(frame_type),
      last_focus_time(last_focus_time),
      client_type(client_type) {}

ServiceWorkerClientInfo::ServiceWorkerClientInfo(
    const ServiceWorkerClientInfo& other) = default;

bool ServiceWorkerClientInfo::IsEmpty() const {
  return page_visibility_state == blink::WebPageVisibilityStateLast &&
         is_focused == false &&
         url.is_empty() &&
         frame_type == REQUEST_CONTEXT_FRAME_TYPE_LAST &&
         client_type == blink::WebServiceWorkerClientTypeLast;
}

bool ServiceWorkerClientInfo::IsValid() const {
  return !IsEmpty() && !client_uuid.empty();
}

}  // namespace content
