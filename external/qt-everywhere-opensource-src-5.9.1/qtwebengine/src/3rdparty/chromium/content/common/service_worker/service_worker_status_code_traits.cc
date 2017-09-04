// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_status_code_traits.h"

#include "base/logging.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

namespace mojo {

// static
blink::mojom::ServiceWorkerEventStatus EnumTraits<
    blink::mojom::ServiceWorkerEventStatus,
    content::ServiceWorkerStatusCode>::ToMojom(content::ServiceWorkerStatusCode
                                                   input) {
  switch (input) {
    case content::SERVICE_WORKER_OK:
      return blink::mojom::ServiceWorkerEventStatus::COMPLETED;
    case content::SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED:
      return blink::mojom::ServiceWorkerEventStatus::REJECTED;
    case content::SERVICE_WORKER_ERROR_ABORT:
      return blink::mojom::ServiceWorkerEventStatus::ABORTED;
    default:
      NOTREACHED() << "Unexpected ServiceWorkerStatusCode: " << input;
      return blink::mojom::ServiceWorkerEventStatus::ABORTED;
  }
}

// static
bool EnumTraits<blink::mojom::ServiceWorkerEventStatus,
                content::ServiceWorkerStatusCode>::
    FromMojom(blink::mojom::ServiceWorkerEventStatus input,
              content::ServiceWorkerStatusCode* out) {
  switch (input) {
    case blink::mojom::ServiceWorkerEventStatus::COMPLETED:
      *out = content::SERVICE_WORKER_OK;
      return true;
    case blink::mojom::ServiceWorkerEventStatus::REJECTED:
      *out = content::SERVICE_WORKER_ERROR_EVENT_WAITUNTIL_REJECTED;
      return true;
    case blink::mojom::ServiceWorkerEventStatus::ABORTED:
      *out = content::SERVICE_WORKER_ERROR_ABORT;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace content
