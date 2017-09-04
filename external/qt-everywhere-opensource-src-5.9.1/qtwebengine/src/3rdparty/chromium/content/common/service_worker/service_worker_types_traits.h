// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_TRAITS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_TRAITS_H_

#include "content/common/service_worker/service_worker_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<content::mojom::ServiceWorkerProviderType,
                  content::ServiceWorkerProviderType> {
  static content::mojom::ServiceWorkerProviderType ToMojom(
      content::ServiceWorkerProviderType input);

  static bool FromMojom(content::mojom::ServiceWorkerProviderType input,
                        content::ServiceWorkerProviderType* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPES_TRAITS_H_
