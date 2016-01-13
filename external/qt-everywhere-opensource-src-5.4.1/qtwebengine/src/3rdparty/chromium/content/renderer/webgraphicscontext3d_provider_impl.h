// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3DProvider.h"

namespace webkit {
namespace gpu {
class ContextProviderWebContext;
}  // namespace webkit
}  // namespace gpu

namespace content {

class CONTENT_EXPORT WebGraphicsContext3DProviderImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3DProvider) {
 public:
  explicit WebGraphicsContext3DProviderImpl(
      scoped_refptr<webkit::gpu::ContextProviderWebContext> provider);
  virtual ~WebGraphicsContext3DProviderImpl();

  // WebGraphicsContext3DProvider implementation.
  virtual blink::WebGraphicsContext3D* context3d() OVERRIDE;
  virtual GrContext* grContext() OVERRIDE;

 private:
  scoped_refptr<webkit::gpu::ContextProviderWebContext> provider_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
