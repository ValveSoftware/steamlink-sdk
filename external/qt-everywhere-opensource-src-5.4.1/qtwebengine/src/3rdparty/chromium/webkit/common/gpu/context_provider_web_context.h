// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_WEB_CONTEXT_H_
#define WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_WEB_CONTEXT_H_

#include "cc/output/context_provider.h"

namespace blink { class WebGraphicsContext3D; }

namespace webkit {
namespace gpu {

class ContextProviderWebContext : public cc::ContextProvider {
 public:
  virtual blink::WebGraphicsContext3D* WebContext3D() = 0;

 protected:
  virtual ~ContextProviderWebContext() {}
};

}  // namespace gpu
}  // namespace webkit

#endif  // WEBKIT_COMMON_GPU_CONTEXT_PROVIDER_WEB_CONTEXT_H_
