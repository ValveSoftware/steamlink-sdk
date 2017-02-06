// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MIME_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MIME_REGISTRY_IMPL_H_

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/mime_registry.mojom.h"

namespace content {

class MimeRegistryImpl : public blink::mojom::MimeRegistry {
 public:
  static void Create(blink::mojom::MimeRegistryRequest request);

 private:
  MimeRegistryImpl(blink::mojom::MimeRegistryRequest request);
  ~MimeRegistryImpl() override;

  void GetMimeTypeFromExtension(
      const mojo::String& extension,
      const GetMimeTypeFromExtensionCallback& callback) override;

  mojo::StrongBinding<blink::mojom::MimeRegistry> binding_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MIME_REGISTRY_IMPL_H_
