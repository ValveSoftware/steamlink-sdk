// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_STATUS_CODE_TRAITS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_STATUS_CODE_TRAITS_H_

#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::ServiceWorkerEventStatus,
                  content::ServiceWorkerStatusCode> {
  static blink::mojom::ServiceWorkerEventStatus ToMojom(
      content::ServiceWorkerStatusCode input);

  static bool FromMojom(blink::mojom::ServiceWorkerEventStatus input,
                        content::ServiceWorkerStatusCode* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_STATUS_CODE_TRAITS_H_
