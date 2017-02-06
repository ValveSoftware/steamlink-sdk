// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
#define CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3DProvider.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace content {
class ContextProviderCommandBuffer;

class CONTENT_EXPORT WebGraphicsContext3DProviderImpl
    : public NON_EXPORTED_BASE(blink::WebGraphicsContext3DProvider) {
 public:
  explicit WebGraphicsContext3DProviderImpl(
      scoped_refptr<ContextProviderCommandBuffer> provider);
  ~WebGraphicsContext3DProviderImpl() override;

  // WebGraphicsContext3DProvider implementation.
  bool bindToCurrentThread() override;
  gpu::gles2::GLES2Interface* contextGL() override;
  GrContext* grContext() override;
  gpu::Capabilities getCapabilities() override;
  void setLostContextCallback(blink::WebClosure) override;
  void setErrorMessageCallback(
      blink::WebFunction<void(const char*, int32_t)>) override;

  ContextProviderCommandBuffer* context_provider() const {
    return provider_.get();
  }

 private:
  scoped_refptr<ContextProviderCommandBuffer> provider_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEBGRAPHICSCONTEXT3D_PROVIDER_IMPL_H_
