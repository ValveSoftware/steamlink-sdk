// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types_traits.h"

namespace mojo {

content::mojom::ServiceWorkerProviderType EnumTraits<
  content::mojom::ServiceWorkerProviderType,
  content::ServiceWorkerProviderType>::ToMojom(
      content::ServiceWorkerProviderType input) {
  return static_cast<content::mojom::ServiceWorkerProviderType>(input);
}

bool EnumTraits<content::mojom::ServiceWorkerProviderType,
                content::ServiceWorkerProviderType>::FromMojom(
                    content::mojom::ServiceWorkerProviderType input,
                    content::ServiceWorkerProviderType* out) {
  *out = static_cast<content::ServiceWorkerProviderType>(input);
  return true;
}

}  // namespace mojo
