// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_WEBSTORAGENAMESPACE_IMPL_H_
#define CONTENT_RENDERER_DOM_STORAGE_WEBSTORAGENAMESPACE_IMPL_H_

#include <stdint.h>

#include "third_party/WebKit/public/platform/WebStorageNamespace.h"

namespace content {

class WebStorageNamespaceImpl : public blink::WebStorageNamespace {
 public:
  // The default constructor creates a local storage namespace, the second
  // constructor should be used for session storage namepaces.
  WebStorageNamespaceImpl();
  explicit WebStorageNamespaceImpl(int64_t namespace_id);
  ~WebStorageNamespaceImpl() override;

  // See WebStorageNamespace.h for documentation on these functions.
  blink::WebStorageArea* createStorageArea(
      const blink::WebSecurityOrigin& origin) override;
  virtual blink::WebStorageNamespace* copy();
  bool isSameNamespace(const WebStorageNamespace&) const override;

 private:
  int64_t namespace_id_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_WEBSTORAGENAMESPACE_IMPL_H_
