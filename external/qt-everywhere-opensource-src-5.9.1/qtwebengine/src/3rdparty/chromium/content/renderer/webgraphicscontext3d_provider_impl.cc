// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webgraphicscontext3d_provider_impl.h"

#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "gpu/command_buffer/client/context_support.h"

namespace content {

WebGraphicsContext3DProviderImpl::WebGraphicsContext3DProviderImpl(
    scoped_refptr<ContextProviderCommandBuffer> provider,
    bool software_rendering)
    : provider_(std::move(provider)), software_rendering_(software_rendering) {}

WebGraphicsContext3DProviderImpl::~WebGraphicsContext3DProviderImpl() {}

bool WebGraphicsContext3DProviderImpl::bindToCurrentThread() {
  return provider_->BindToCurrentThread();
}

gpu::gles2::GLES2Interface* WebGraphicsContext3DProviderImpl::contextGL() {
  return provider_->ContextGL();
}

GrContext* WebGraphicsContext3DProviderImpl::grContext() {
  return provider_->GrContext();
}

gpu::Capabilities WebGraphicsContext3DProviderImpl::getCapabilities() {
  return provider_->ContextCapabilities();
}

bool WebGraphicsContext3DProviderImpl::isSoftwareRendering() const {
  return software_rendering_;
}

void WebGraphicsContext3DProviderImpl::setLostContextCallback(
    const base::Closure& c) {
  provider_->SetLostContextCallback(c);
}

void WebGraphicsContext3DProviderImpl::setErrorMessageCallback(
    const base::Callback<void(const char*, int32_t)>& c) {
  provider_->ContextSupport()->SetErrorMessageCallback(c);
}

void WebGraphicsContext3DProviderImpl::signalQuery(
    uint32_t query, const base::Closure& callback) {
  provider_->ContextSupport()->SignalQuery(query, callback);
}

}  // namespace content
