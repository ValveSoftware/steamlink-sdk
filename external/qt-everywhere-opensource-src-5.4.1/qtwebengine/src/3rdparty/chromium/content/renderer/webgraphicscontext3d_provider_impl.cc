// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "webkit/common/gpu/context_provider_web_context.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<webkit::gpu::ContextProviderWebContext> provider)
    : provider_(provider) {}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {}

blink::WebGraphicsContext3D* WebGraphicsContext3DProviderImpl::context3d() {
  return provider_->WebContext3D();
}

GrContext* WebGraphicsContext3DProviderImpl::grContext() {
  return provider_->GrContext();
}

}  // namespace content
