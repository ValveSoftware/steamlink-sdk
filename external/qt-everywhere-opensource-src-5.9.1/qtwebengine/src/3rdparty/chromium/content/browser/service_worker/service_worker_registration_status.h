// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_

#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"

namespace content {

// This should only be called for errors, where status != OK.
void GetServiceWorkerRegistrationStatusResponse(
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    blink::WebServiceWorkerError::ErrorType* error_type,
    base::string16* message);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_STATUS_H_
