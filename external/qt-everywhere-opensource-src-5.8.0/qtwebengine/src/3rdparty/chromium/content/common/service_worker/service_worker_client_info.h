// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_

#include "base/time/time.h"
#include "content/public/common/request_context_frame_type.h"
#include "third_party/WebKit/public/platform/WebPageVisibilityState.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerClientType.h"
#include "url/gurl.h"

namespace content {

// This class holds the information related to a service worker window client.
// It is the content/ equivalent of Blink's WebServiceWorkerClientInfo.
// An instance can be created empty or can be filed with the expected
// properties. Except for the client_uuid, it is preferred to use the
// constructor to fill the properties.
struct ServiceWorkerClientInfo {
  ServiceWorkerClientInfo();
  ServiceWorkerClientInfo(const std::string& client_uuid,
                          blink::WebPageVisibilityState page_visibility_state,
                          bool is_focused,
                          const GURL& url,
                          RequestContextFrameType frame_type,
                          base::TimeTicks last_focus_time,
                          blink::WebServiceWorkerClientType client_type);
  ServiceWorkerClientInfo(const ServiceWorkerClientInfo& other);

  // Returns whether the instance is empty.
  bool IsEmpty() const;

  // Returns whether the instance is valid. A valid instance is not empty and
  // has a valid client_uuid.
  bool IsValid() const;

  std::string client_uuid;
  blink::WebPageVisibilityState page_visibility_state;
  bool is_focused;
  GURL url;
  RequestContextFrameType frame_type;
  base::TimeTicks last_focus_time;
  blink::WebServiceWorkerClientType client_type;
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_CLIENT_INFO_H_
